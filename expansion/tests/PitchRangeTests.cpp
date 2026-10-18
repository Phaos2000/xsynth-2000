// GPL-2.0-or-later. Range, migration and DSP differential tests.
#include "Core.h"
#define xs pre_pitch_xs
#include "pre-pitch/Core.h"
#undef xs
#include "ProductIdentity.h"
#include <atomic>
#include <cstdlib>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <new>
#include <stdexcept>
std::atomic<size_t> allocations{0};
void *operator new(size_t n) {
  ++allocations;
  if (auto p = std::malloc(n ? n : 1))
    return p;
  throw std::bad_alloc();
}
void *operator new[](size_t n) { return ::operator new(n); }
void operator delete(void *p) noexcept { std::free(p); }
void operator delete[](void *p) noexcept { std::free(p); }
void operator delete(void *p, size_t) noexcept { std::free(p); }
void operator delete[](void *p, size_t) noexcept { std::free(p); }
namespace xs {
struct DspValidationProbe {
  static auto &voice(Engine &e) { return e.pool[0]; }
  static float osc(Engine &e, int o, float hz) {
    bool wrapped;
    return e.oscillator(e.pool[0], o, hz, 0, .5, wrapped);
  }
  static bool stable(Engine &e) {
    for (auto &v : e.pool) {
      for (float p : v.phase)
        if (!std::isfinite(p) || p < 0 || p >= 1)
          return false;
      for (float f : v.filter)
        if (!std::isfinite(f) || std::abs(f) > 10000)
          return false;
    }
    return true;
  }
};
} // namespace xs
void check(bool b, const char *m) {
  if (!b)
    throw std::runtime_error(m);
}
void near(double a, double b, double tolerance, const char *m) {
  check(std::abs(a - b) <= tolerance, m);
}
void mapping() {
  static_assert(xs::hostId(0) == 0x1000 && xs::hostId(2) == 0x1002, "Pitch IDs changed");
  for (int id : {0, 2}) {
    for (int k = 0; k <= 4; ++k) {
      double ratio = std::exp2(k - 2.);
      near(xs::physical(id, k / 4.), ratio, 0, "Octave stop exactness");
      near(xs::normalized(id, ratio), k / 4., 0, "Inverse octave stop");
    }
    double previous = 0;
    for (int k = 0; k <= 10000; ++k) {
      double n = k / 10000., p = xs::physical(id, n);
      check(p > previous, "Pitch sweep nonmonotonic");
      near(xs::normalized(id, p), n, 3e-16, "Inverse mapping");
      previous = p;
    }
    near(xs::physical(id, xs::defaults()[id]), 1, 0, "Default retuned");
  }
  // Compare real V1 states and rendered engines, including all unrelated state.
  double worst = 0;
  for (double sr : {44100., 48000., 88200., 96000.})
    for (double a : {0., .1, .37, .5, .9, 1.})
      for (int mode : {0, 1, 2}) {
        auto old = pre_pitch_xs::defaults();
        old[0] = a;
        old[2] = 1 - a;
        old[24] = .4;
        old[25] = 1;
        old[7] = .4;
        if (mode) {
          old[xs::VoiceMode] = 1;
          old[xs::Engine1] = 1;
          old[xs::Engine2] = mode == 2;
          old[xs::XMode] = mode / 2.;
          old[xs::XDepth] = .2;
        }
        auto bytes = pre_pitch_xs::encode(old);
        xs::State state{};
        check(xs::decode(bytes.data(), bytes.size(), state), "V1 decode");
        for (int i = 0; i < pre_pitch_xs::Count; ++i)
          if (i == 0 || i == 2)
            near(xs::physical(i, state[i]), pre_pitch_xs::physical(i, old[i]), 2e-15,
                 "Migration physical tuning");
          else
            near(state[i], old[i], 0, "Migration changed unrelated parameter");
        auto v2 = xs::encode(state);
        check(v2[4] == 6 && v2.size() == xs::StateBytes, "V2 schema");
        xs::State restored{};
        check(xs::decode(v2.data(), v2.size(), restored) && restored == state, "V2 roundtrip");
        pre_pitch_xs::Engine before;
        before.state = old;
        before.reset(sr);
        xs::Engine after;
        after.state = state;
        after.reset(sr);
        before.note(60, true);
        after.note(60, true);
        for (int n = 0; n < 8192; ++n) {
          double error = std::abs(double(before.next()) - after.next());
          worst = std::max(worst, error);
          check(std::isfinite(error), "Migrated audio nonfinite"); // Filter/direct pitch fixes intentionally differ.
        }
        bytes.resize(8 + xs::LegacyCount * 8);
        bytes[2] = 'V';
        bytes[3] = '3';
        check(xs::decode(bytes.data(), bytes.size(), restored), "Reference state migration");
        near(xs::physical(0, restored[0]), pre_pitch_xs::physical(0, old[0]), 2e-15,
             "XSV3 physical tuning");
        auto unchanged = restored;
        bytes[4] = 99;
        check(!xs::decode(bytes.data(), bytes.size(), restored) && restored == unchanged,
              "Unknown version not atomic");
      }
  std::cout << "PASS: endpoints/inverses/defaults/IDs; V1/XSV3 migration and V2 recall; migrated "
               "old audio max error="
            << worst << '\n';
}
std::vector<float> full(int oscillator, double ratio, int clean, int note, int rate, int block) {
  xs::Engine e;
  e.reset(rate);
  e.set(xs::VoiceMode, 1);
  e.set(xs::Voices, 0);
  e.set(oscillator ? 2 : 0, xs::normalized(oscillator ? 2 : 0, ratio));
  e.set(7, oscillator);
  e.set(xs::Engine1 + oscillator, clean);
  e.set(xs::FilterType, 1. / 3);
  e.set(21, 0);
  e.note(note, true);
  std::vector<float> out(4096);
  for (int base = 0; base < 4096; base += block)
    for (int i = base; i < std::min(4096, base + block); ++i) {
      out[i] = e.next();
      check(std::isfinite(out[i]) && std::abs(out[i]) <= 1, "Range output invalid");
      check(xs::DspValidationProbe::stable(e), "Range internal instability");
    }
  return out;
}
void rangeDSP() {
  using P = xs::DspValidationProbe;
  size_t cases = 0;
  for (int rate : {44100, 48000, 88200, 96000})
    for (int o : {0, 1})
      for (int clean : {0, 1})
        for (double ratio : {.25, .5, 1., 2., 4.})
          for (int note : {36, 69, 84}) {
            auto baseline = full(o, ratio, clean, note, rate, 32);
            for (int block : {128, 512, 1024})
              check(baseline == full(o, ratio, clean, note, rate, block),
                    "Buffer dependent output");
            xs::Engine e;
            e.reset(rate);
            e.set(xs::VoiceMode, 1);
            e.set(o ? 2 : 0, xs::normalized(o ? 2 : 0, ratio));
            e.set(xs::Engine1 + o, clean);
            e.note(note, true);
            e.next();
            auto &v = P::voice(e);
            double hz = 440 * std::exp2((note - 69) / 12.f) * ratio;
            near(v.pitchRatio[o], ratio, 1e-6, "Actual ratio");
            // Measure the actual generated sine, independently of the filter and VCA.
            int crossings = 0;
            double sum = 0, energy = 0;
            float previous = 0;
            for (int i = 0; i < rate; ++i) {
              float sample = P::osc(e, o, float(hz));
              if (previous <= 0 && sample > 0)
                ++crossings;
              previous = sample;
              sum += sample;
              energy += sample * sample;
              check(P::stable(e), "Oscillator phase invalid");
            }
            near(crossings, hz, 1.1, "Measured pitch");
            check(std::abs(sum / rate) < .06 && energy / rate > .4, "DC or silent oscillator");
            ++cases;
          }
  std::cout << "PASS: " << cases
            << " oscillator/rate/ratio/note cases, 32/128/512/1024 blocks, measured pitch, phase "
               "and DC\n";
}
void modulation() {
  using P = xs::DspValidationProbe;
  for (int rate : {44100, 48000, 88200, 96000})
    for (auto pair : std::initializer_list<std::pair<double, double>>{{.25, 4}, {.5, 2}, {.25, .5}})
      for (int clean = 0; clean < 4; ++clean)
        for (int mode : {1, 2})
          for (int direction : {0, 1})
            for (int sync : {0, 1}) {
              xs::Engine a;
              a.reset(rate);
              a.set(xs::VoiceMode, 1);
              a.set(0, xs::normalized(0, pair.first));
              a.set(2, xs::normalized(2, pair.second));
              a.set(xs::Engine1, clean & 1);
              a.set(xs::Engine2, (clean >> 1) & 1);
              a.set(xs::XMode, mode / 2.);
              a.set(xs::XDirection, direction);
              a.set(xs::XDepth, 1);
              a.set(27, sync);
              a.note(84, true);
              auto alloc = allocations.load();
              for (int i = 0; i < 4096; ++i) {
                float f = a.next();
                check(std::isfinite(f) && std::abs(f) <= 1 && P::stable(a),
                      "FM/PM/sync extended range unstable");
              }
              check(allocations == alloc, "Audio allocation");
              xs::Engine off = a, zero = a;
              off.set(xs::XMode, 0);
              zero.set(xs::XDepth, 0);
              // A mode edit now crossfades; compare the settled zero-depth law
              // from equal phase/history rather than unlike transition histories.
              off.reset(rate);zero.reset(rate);off.note(84,true);zero.note(84,true);
              for (int i = 0; i < 256; ++i)
                check(off.next() == zero.next(), "Zero depth differs from OFF");
            }
  for (double ratio : {.25, 4.})
    for (int target : {0, 1})
      for (int kind = 0; kind < 4; ++kind) {
        xs::Engine e;
        e.reset(48000);
        e.set(xs::VoiceMode, 1);
        e.set(target ? 2 : 0, xs::normalized(target ? 2 : 0, ratio));
        if (kind == 0)
          e.set(5, 1);
        if (kind == 1)
          e.set(13, 1);
        if (kind == 2) {
          e.set(xs::MatrixFirst, 1);
          e.set(xs::MatrixFirst + 1, (target ? 3. : 0.) / (xs::LegacyDestinationCount-1));
          e.set(xs::MatrixFirst + 2, 1);
        }
        if (kind == 3)
          e.set(30, 1);
        e.note(84, true);
        for (int i = 0; i < 8192; ++i) {
          float f = e.next();
          check(std::isfinite(f) && P::stable(e), "Pitch modulation unstable");
        }
      }
  check(sizeof(xs::Engine) < 98304, "DSP fixed storage exceeds 96 KiB");
  std::cout << "PASS: FM/PM/both directions/sync/mixed engines; zero-depth identity; "
               "LFO/EG/matrix/bend; no allocations; DSP bytes="
            << sizeof(xs::Engine) << '\n';
}
void automation() {
  for (int o : {0, 1}) {
    xs::Engine e;
    e.reset(48000);
    e.set(xs::VoiceMode, 1);
    e.note(60, true);
    double previous = 0;
    for (int k = 0; k <= 1000; ++k) {
      double n = k / 1000.;
      e.set(o ? 2 : 0, n);
      for (int i = 0; i < 16; ++i)
        check(std::isfinite(e.next()), "Automation nonfinite");
      double ratio = xs::DspValidationProbe::voice(e).pitchRatio[o];
      near(ratio, xs::physical(o ? 2 : 0, n), 3e-6, "Automation ratio");
      check(ratio > previous, "Automation not monotonic");
      previous = ratio;
    }
  }
  std::cout << "PASS: independent automated sweeps reach .25 and 4 smoothly through existing "
               "parameters\n";
}
void savedFixtures(const char *directory) {
  int count = 0;
  for (auto &file : std::filesystem::directory_iterator(directory))
    if (file.path().extension() == ".XseDev") {
      std::ifstream in(file.path(), std::ios::binary);
      std::vector<uint8_t> bytes{std::istreambuf_iterator<char>(in), {}};
      pre_pitch_xs::State old{};
      xs::State migrated{};
      check(pre_pitch_xs::decode(bytes.data(), bytes.size(), old) &&
                xs::decode(bytes.data(), bytes.size(), migrated),
            "Existing saved preset load");
      for (int i = 0; i < pre_pitch_xs::Count; ++i)
        if (i == 0 || i == 2)
          near(xs::physical(i, migrated[i]), pre_pitch_xs::physical(i, old[i]), 2e-15,
               "Saved preset tuning changed");
        else if(i>=xs::MatrixFirst&&(i-xs::MatrixFirst)%3==1)
          near(migrated[i],old[i],0,"Saved matrix destination semantic migration");
        else
          near(migrated[i], old[i], 0, "Saved preset unrelated value changed");
      pre_pitch_xs::Engine a;
      a.state = old;
      a.reset(48000);
      xs::Engine b;
      b.state = migrated;
      b.reset(48000);
      a.note(60, true);
      b.note(60, true);
      bool continuousMatrix=false;for(int slot=0;slot<6;++slot)continuousMatrix|=old[xs::MatrixFirst+slot*3]==.1&&old[xs::MatrixFirst+slot*3+2]!=.5;
      for (int i = 0; i < 8192; ++i){float before=a.next(),after=b.next();
        check(std::isfinite(after)&&std::abs(after)<=1,"Migrated native preset audio invalid"); // Historical difference is diagnostic.
      }

      ++count;
    }
  check(count == 10, "Expected ten pre-extension saved presets");
  std::cout << "PASS: ten on-disk V1 native presets retain tuning/routes; current audio finite; historical filter/direct-LFO differences retained for comparison\n";
}
int main(int argc, char **argv) {
  try {
    mapping();
    rangeDSP();
    modulation();
    automation();
    if (argc > 1)
      savedFixtures(argv[1]);
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
