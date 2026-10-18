// GPL-2.0-or-later. Offline DSP gate. No dependency on the GUI or VST3 build.
#include "Core.h"
#define xs reference_xs
#include "reference/Core.h"
#undef xs
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <functional>
#include <chrono>
#include <atomic>
#include <new>
#include <cstdlib>
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
// Friend access has no storage, code, or public plugin API cost. Test-only probes
// check internal state so the output limiter cannot conceal unstable DSP.
struct DspValidationProbe {
  static auto &voice(Engine &e) { return e.pool[0]; }
  static void prepare(Engine &e) { e.prepare(); }
  static float osc(Engine &e, int o, float hz, float shape) {
    bool w;
    return e.oscillator(e.pool[0], o, hz, 0, shape, w);
  }
  static float raw(float p, int w, float pw) { return Engine::raw(p, w, pw); }
  static float env(Engine &e, int i) { return e.envelope(e.pool[0], i); }
  static void route(Engine &e, float lfo) { e.control(e.pool[0], lfo); }
  static auto &voices(Engine &e) { return e.pool; }
  static float filter(Engine &e, float in) { return e.filter(e.pool[0], in); }
  static float lfo(Engine &e) { return e.lfoValue; }
  static bool stable(Engine &e) {
    for (auto &v : e.pool) {
      for (float f : v.filter)
        if (!std::isfinite(f) || std::abs(f) > 10000)
          return false;
      for (auto &eg : v.eg)
        if (!std::isfinite(eg.value))
          return false;
    }
    return true;
  }
};
} // namespace xs
using P = xs::DspValidationProbe;
using Audio = std::vector<float>;
void require(bool v, const char *m) {
  if (!v)
    throw std::runtime_error(m);
}
void option(xs::Engine &e, int id, int v) { e.set(id, double(v) / xs::stepsFor(id)); }
void fixture(xs::Engine &e, double sr = 48000) {
  e.reset(sr);
  option(e, xs::VoiceMode, 2);
  option(e, xs::Voices, 0);
  e.set(23, .3);
  e.set(24, .4);
  e.set(25, 1);
  e.set(7, 0);
}
void advance(xs::Engine &e, int n) {
  for (int i = 0; i < n; ++i)
    e.next();
}
double rms(const Audio &a) {
  double s = 0;
  for (auto v : a)
    s += double(v) * v;
  return std::sqrt(s / a.size());
}
double mean(const Audio &a) {
  double s = 0;
  for (auto v : a)
    s += v;
  return s / a.size();
}
double tone(const Audio &a, double hz, double sr) {
  double re = 0, im = 0;
  for (size_t i = 0; i < a.size(); ++i) {
    double p = 2 * xs::Pi * hz * i / sr;
    re += a[i] * std::cos(p);
    im += a[i] * std::sin(p);
  }
  return 2 * std::hypot(re, im) / a.size();
}
Audio oscillator(int wave, int clean, int osc, int note, double sr, float pw = .5f) {
  xs::Engine e;
  fixture(e, sr);
  option(e, 24 + osc, wave);
  option(e, xs::Engine1 + osc, clean);
  P::prepare(e);
  Audio a(static_cast<size_t>(sr), 0.f);
  float hz = 440 * std::exp2((note - 69) / 12.f);
  for (auto &v : a)
    v = P::osc(e, osc, hz, pw);
  return a;
}
void safe(const Audio &a) {
  for (float v : a)
    require(std::isfinite(v) && std::abs(v) <= 1 && std::fpclassify(v) != FP_SUBNORMAL,
            "Nonfinite, unbounded or denormal output");
}
struct Case {
  const char *name;
  int mode = 0, mix = 0, clean = 0;
  bool sync = false, sweep = false, lfo = false, loop = false, chord = false;
};
const Case cases[] = {{"classic-saw"},
                      {"clean-saw", 0, 0, 3},
                      {"hard-sync", 0, 0, 0, true},
                      {"pm", 2},
                      {"fm", 1},
                      {"xor", 0, 4},
                      {"resonant-filter-sweep", 0, 0, 0, false, true},
                      {"synced-lfo", 0, 0, 0, false, false, true},
                      {"looping-eg2", 0, 0, 0, false, false, false, true},
                      {"matrix-chord", 2, 0, 2, false, false, false, false, true}};
xs::State patch(const Case &c) {
  xs::Engine e;
  fixture(e);
  e.set(7, .5);
  e.set(2, .5+.5*.37); // Preserve the physical tuning of the retained V1 fixtures.
  option(e, xs::XMode, c.mode);
  e.set(xs::XDepth, c.mode ? .18 : 0);
  option(e, xs::MixerMode, c.mix);
  e.set(xs::Engine1, (c.clean & 1) ? 1 : 0);
  e.set(xs::Engine2, (c.clean & 2) ? 1 : 0);
  e.set(27, c.sync);
  e.set(12, .4);
  e.set(18, .4);
  if (c.sweep) {
    e.set(22, .95);
    e.set(21, .01);
  }
  if (c.lfo) {
    e.set(xs::LfoClock, 1);
    option(e, xs::LfoPhase, 2);
    option(e, xs::Division, 20);
    e.set(5, .2);
  }
  if (c.loop) {
    e.set(21, .015);
    option(e, xs::Loop2, 2);
    e.set(15, .4);
    e.set(16, .4);
    e.set(17, .2);
    e.set(18, .4);
    e.set(20, .15);
  }
  if (c.chord) {
    option(e, xs::Voices, 2);
    for (int r = 0; r < 6; ++r) {
      int id = xs::MatrixFirst + 3 * r;
      const int src[] = {1, 2, 3, 4, 9, 10}, dst[] = {0, 4, 8, 9, 3, 7};
      option(e, id, src[r]);
      option(e, id + 1, dst[r]);
      e.set(id + 2, .53);
    }
    e.set(22, .4);
  }
  if (std::string(c.name) == "classic-saw" || std::string(c.name) == "clean-saw")
    e.set(7, 0);
  return e.state;
}
Audio render(const Case &c, double sr, int block) {
  xs::Engine e;
  e.state = patch(c);
  e.reset(sr);
  int total = int(sr * 2);
  Audio a(total);
  for (int base = 0; base < total; base += block) {
    e.transport(123, true, base * 123 / (60 * sr), true, true);
    for (int i = base; i < std::min(total, base + block); ++i) {
      if (i == 0) {
        e.note(60, true, .8f, 1);
        if (c.chord) {
          e.note(64, true, .65f, 2);
          e.note(67, true, .7f, 3);
          e.note(72, true, .6f, 4);
        }
      }
      if (i == int(sr * .37))
        e.set(30, .53);
      if (i == int(sr * .71))
        e.set(30, .5);
      if (c.sweep)
        e.set(21, .002 + .25 * double(i) / total);
      if (i == int(sr * 1.25))
        e.allOff();
      a[i] = e.next();
      require(P::stable(e), "Internal filter instability hidden by output limiter");
    }
  }
  safe(a);
  return a;
}
void wav(const std::filesystem::path &p, const Audio &a) {
  std::ofstream o(p, std::ios::binary);
  auto u = [&](uint32_t x, int n) {
    for (int j = 0; j < n; ++j)
      o.put(char(x >> (j * 8)));
  };
  o.write("RIFF", 4);
  u(36 + uint32_t(a.size() * 4), 4);
  o.write("WAVEfmt ", 8);
  u(16, 4);
  u(3, 2);
  u(1, 2);
  u(48000, 4);
  u(192000, 4);
  u(4, 2);
  u(32, 2);
  o.write("data", 4);
  u(uint32_t(a.size() * 4), 4);
  o.write(reinterpret_cast<const char *>(a.data()), a.size() * 4);
  require(bool(o), "WAV write failed");
}
int main(int argc, char **argv) {
  std::filesystem::path output = argc > 1 ? argv[1] : "dsp-gate";
  bool record = argc > 2 && std::string(argv[2]) == "--record";
  std::filesystem::create_directories(output);
  int passed = 0, failed = 0;
  auto test = [&](const char *name, auto f) {
    try {
      f();
      ++passed;
      std::cout << "PASS " << name << std::endl;
    } catch (const std::exception &e) {
      ++failed;
      std::cout << "FAIL " << name << ": " << e.what() << std::endl;
    }
  };
  test("Reference edition matched patches / four sample rates", [] {
    double worst = 0;
    for (double sr : {44100., 48000., 88200., 96000.})
      for (int kind = 0; kind < 8; ++kind) {
        reference_xs::Engine ref;
        xs::Engine e;
        auto s = xs::defaults();
        s[24] = kind == 0 ? 0 : kind == 2 ? 1 : .4;
        s[25] = .8;
        s[7] = kind == 3 ? .3 : 0;
        s[21] = kind == 4 ? .035 : 1;
        s[22] = kind == 4 ? .8 : 0;
        if (kind == 5) {
          s[9] = .5;
          s[10] = .6;
          s[11] = .3;
          s[12] = .5;
        }
        if (kind == 6)
          s[8] = .7;
        if (kind == 7) {
          s[27] = 1;
          s[7] = 1;
          s[2] = .5+.5*.37;
        }
        for (int i = 0; i < 32; ++i)
          ref.set(i, i==0||i==2?reference_xs::normalized(i,xs::physical(i,s[i])):s[i]);
        e.state = s;
        ref.reset(sr);
        e.reset(sr);
        double err = 0, en = 0;
        for (int n = 0; n < int(sr); ++n) {
          if (n == 0) {
            e.note(60, true);
            ref.note(60, true);
          }
          if (n == int(sr / 3)) {
            e.note(72, true);
            ref.note(72, true);
          }
          if (n == int(sr * .7)) {
            e.allOff();
            ref.allOff();
          }
          double a = e.next(), b = ref.next();
          err += (a - b) * (a - b);
          en += b * b;
        }
        double rel = std::sqrt(err / std::max(en, 1.e-20));
        worst = std::max(worst, rel);
        require(rel < .002, "Reference deviation exceeds 0.2% RMS");
      }
    std::cout << "reference worst relative RMS=" << worst << '\n';
  });
  test("Independent oscillators: pitch, amplitude, shape, DC, rates", [] {
    for (double sr : {44100., 48000., 88200., 96000.})
      for (int o : {0, 1})
        for (int clean : {0, 1})
          for (int w = 0; w < 6; ++w)
            for (int note : {33, 69, 93}) {
              auto a = oscillator(w, clean, o, note, sr);
              safe(a);
              double hz = 440 * std::exp2((note - 69) / 12.);
              require(tone(a, hz, sr) > .15, "Fundamental missing or tuning shifted");
              require(tone(a, hz, sr) > tone(a, hz * 1.01, sr) * 1.5,
                      "Pitch differs by one percent");
              require(std::abs(mean(a)) < .035, "Unexpected oscillator DC");
              require(rms(a) > .15 && rms(a) < 1.01, "Oscillator amplitude out of range");
            }
    for (float pw : {0.f, .1f, .5f, .9f, 1.f})
      for (int clean : {0, 1}) {
        auto a = oscillator(5, clean, 0, 69, 48000, pw);
        require(std::abs(mean(a) - (2 * pw - 1)) < .015, "Pulse width / natural DC wrong");
      }
    for (int w = 0; w < 6; ++w)
      for (int i = 0; i < 100; ++i) {
        float p = i / 100.f;
        float expected = w == 0                   ? xs::LegacyEngine::sineSample(p)
                         : w == 1                 ? (p < .25   ? 4 * p
                                                     : p < .75 ? 2 - 4 * p
                                                               : 4 * p - 4)
                         : w == 2                 ? 2 * p - 1
                         : w == 3                 ? 1 - 2 * p
                         : p < (w == 4 ? .5 : .3) ? 1
                                                  : -1;
        require(std::abs(P::raw(p, w, .3) - expected) < 1.e-6, "RAW waveform geometry mismatch");
      }
  });
  test("CLEAN alias reduction without global mode or tuning change", [] {
    for (int w : {1, 2, 3, 4, 5}) {
      xs::Engine raw, clean;
      fixture(raw);
      fixture(clean);
      option(raw, 24, w);
      option(clean, 24, w);
      clean.set(xs::Engine1, 1);
      P::prepare(raw);
      P::prepare(clean);
      Audio a(48000), b(48000);
      for (int i = 0; i < 48000; ++i) {
        a[i] = P::osc(raw, 0, 7500, .3);
        b[i] = P::osc(clean, 0, 7500, .3);
      }
      double aliasHz = w == 1 || w == 4 ? 10500 : 18000;
      double ar = tone(a, aliasHz, 48000), ac = tone(b, aliasHz, 48000);
      std::cout << "wave=" << w << " alias raw/clean=" << ar << '/' << ac
                << " fundamental ratio=" << tone(b, 7500, 48000) / tone(a, 7500, 48000) << '\n';
      require(ac < ar * .85, "CLEAN alias not reduced");
      require(tone(b, 7500, 48000) > .5 * tone(a, 7500, 48000),
              "CLEAN fundamental excessively attenuated");
    }
  });
  test("Block-size invariance, deterministic renders, four rates", [] {
    for (double sr : {44100., 48000., 88200., 96000.})
      for (const auto &c : cases) {
        auto base = render(c, sr, 32);
        require(rms(base) > 1.e-5, "Unexpected silence");
        for (int block : {64, 128, 256, 512, 1024}) {
          auto other = render(c, sr, block);
          double max = 0;
          for (size_t i = 0; i < base.size(); ++i)
            max = std::max(max, double(std::abs(base[i] - other[i])));
          require(max < 2.e-5, "Block size changed DSP timing/audio");
        }
        require(base == render(c, sr, 32), "Fixed seed render is not reproducible");
      }
  });
  test("Sync and XMOD sweeps; zero-depth identity; source direction", [] {
    for (double sr : {44100., 48000., 88200., 96000.})
      for (int note : {24, 60, 96})
        for (double ratio : {0., .25, .37, 1.})
          for (int engines = 0; engines < 4; ++engines)
            for (int direction : {0, 1})
              for (int mode : {1, 2})
                for (double depth : {0., .05, .5, 1.}) {
                  xs::Engine e;
                  fixture(e, sr);
                  e.set(7, .5);
                  e.set(2, ratio);
                  e.set(27, 1);
                  e.set(xs::Engine1, engines & 1 ? 1 : 0);
                  e.set(xs::Engine2, engines & 2 ? 1 : 0);
                  option(e, xs::XMode, mode);
                  e.set(xs::XDirection, direction);
                  e.set(xs::XDepth, depth);
                  e.note(note, true);
                  double en = 0;
                  for (int n = 0; n < 2048; ++n) {
                    float v = e.next();
                    require(std::isfinite(v) && std::abs(v) <= 1 && P::stable(e),
                            "Sync/XMOD instability");
                    en += v * v;
                  }
                  require(en > 1.e-10, "Sync/XMOD unexpected silence");
                }
    for (int direction : {0, 1})
      for (int mode : {1, 2})
        for (int engines = 0; engines < 4; ++engines) {
          xs::Engine a, b;
          fixture(a);
          fixture(b);
          for (auto e : {&a, &b}) {
            e->set(xs::Engine1, engines & 1 ? 1 : 0);
            e->set(xs::Engine2, engines & 2 ? 1 : 0);
            e->set(xs::XDirection, direction);
            e->set(7, .5);
            e->set(27, 1);
          }
          option(b, xs::XMode, mode);
          a.note(69, true);
          b.note(69, true);
          for (int i = 0; i < 8192; ++i)
            require(a.next() == b.next(), "Zero depth differs from OFF");
          xs::Engine src, mod;
          fixture(src);
          fixture(mod);
          for (auto e : {&src, &mod}) {
            e->set(7, direction ? 1 : 0);
            e->set(xs::XDirection, direction);
          }
          option(mod, xs::XMode, mode);
          mod.set(xs::XDepth, 1);
          src.note(69, true);
          mod.note(69, true);
          for (int i = 0; i < 8192; ++i)
            require(src.next() == mod.next(),
                    "XMOD affected source oscillator: direction or recursive feedback");
        }
  });
  test("Filter identity / slopes / tracking / extreme sweeps", [] {
    for (double sr : {44100., 48000., 88200., 96000.})
      for (int type = 0; type < 4; ++type)
        for (int slope : {0, 1})
          for (int note : {24, 60, 96})
            for (double tracking : {0., .5, 1.}) {
              xs::Engine e;
              fixture(e, sr);
              option(e, xs::FilterType, type);
              e.set(28, slope);
              e.set(xs::Keytrack, tracking);
              e.note(note, true);
              for (int i = 0; i < 8192; ++i) {
                e.set(21, double(i) / 8191);
                e.set(22, double(i % 2048) / 2047);
                float s = e.next();
                require(std::isfinite(s) && std::abs(s) <= 1 && P::stable(e),
                        "Filter sweep instability");
              }
            }
    for (double sr : {44100., 48000., 88200., 96000.})
      for (int note : {24, 60, 96})
        for (double tracking : {0., .5, 1.}) {
          xs::Engine e;
          fixture(e, sr);
          e.set(21, .03);
          e.set(xs::Keytrack, tracking);
          e.note(note, true);
          advance(e, 8);
          double hz = 440 * std::exp2((note - 69) / 12.);
          double tracked = 440 * std::pow(hz / 440, tracking);
          double expected = std::min(.825, 2 * xs::Pi * tracked * 1.5 / sr);
          require(std::abs(P::voice(e).g - expected) < 1.e-6,
                  "Keytracking or sample-rate coefficient scaling");
        }
    auto response = [](int type, double hz) {
      xs::Engine e;
      fixture(e);
      option(e, xs::FilterType, type);
      P::prepare(e);
      P::voice(e).g = .13f;
      P::voice(e).damping = 1.4f;
      double energy = 0;
      for (int i = 0; i < 48000; ++i) {
        float y = P::filter(e, float(std::sin(2 * xs::Pi * hz * i / 48000)));
        if (i > 24000)
          energy += y * y;
      }
      return energy;
    };
    require(response(0, 100) > response(0, 10000) * 100, "LP identity");
    require(response(1, 10000) > response(1, 100) * 100, "HP identity");
    require(response(2, 1000) > response(2, 100) * 10, "BP identity");
    require(response(3, 100) > response(3, 1000) * 10, "NOTCH identity");
  });
  test("Envelope times, curves, sustain, velocity, loops and release", [] {
    for (double sr : {44100., 48000., 88200., 96000.})
      for (int index : {0, 1})
        for (int curve = 0; curve < 3; ++curve)
          for (double setting : {0., .5, 1.}) {
            xs::Engine e;
            fixture(e, sr);
            int off = index ? 15 : 9;
            option(e, index ? xs::Curve2 : xs::Curve1, curve);
            e.set(off, setting);
            e.set(off + 1, setting);
            e.set(off + 2, .35);
            e.set(off + 3, setting);
            e.note(60, true, .25);
            auto &v = P::voice(e);
            double coefficient = v.envelopeRate[index][0];
            int attack = int(std::ceil(4.60517 / coefficient)) + 32;
            for (int i = 0; i < attack; ++i)
              P::env(e, index);
            if (v.eg[index].stage != 1)
              std::cout << "envelope sr=" << sr << " index=" << index << " curve=" << curve
                        << " setting=" << setting << " coefficient=" << coefficient
                        << " progress=" << v.eg[index].progress << " value=" << v.eg[index].value
                        << " attack samples=" << attack << std::endl;
            require(v.eg[index].stage == 1, "Attack did not complete on time");
            int decay = int(std::ceil(13 / coefficient)) + 32;
            for (int i = 0; i < decay; ++i)
              P::env(e, index);
            require(std::abs(v.eg[index].value - .35) < 2.e-5, "Decay stalled above sustain");
            e.allOff();
            for (int i = 0; i < decay; ++i)
              P::env(e, index);
            require(v.eg[index].value < 2.e-5, "Release failed");
          }
    for (int loop = 0; loop < 3; ++loop)
      for (int curve = 0; curve < 3; ++curve) {
        xs::Engine e;
        fixture(e);
        option(e, xs::Loop2, loop);
        option(e, xs::Curve2, curve);
        e.set(17, .2);
        e.note(60, true);
        int transitions = 0, previous = -1;
        for (int i = 0; i < 20000; ++i) {
          P::env(e, 1);
          int stage = P::voice(e).eg[1].stage;
          if (stage == 0 && previous != 0)
            ++transitions;
          previous = stage;
        }
        require(loop ? transitions > 2 : transitions == 1, "EG2 loop transition failure");
        e.allOff();
        for (int i = 0; i < 10000; ++i)
          P::env(e, 1);
        require(P::voice(e).eg[1].value < 1.e-6, "EG2 loops after note off");
      }
  });
  test("LFO timing / transport / divisions / phase / polarity", [] {
    for (double sr : {44100., 48000., 88200., 96000.})
      for (int block : {32, 64, 128, 256, 512, 1024})
        for (double bpm : {60., 123., 180.})
          for (int division : {9, 15, 18, 21, 16, 17}) {
            xs::Engine e;
            fixture(e, sr);
            e.set(xs::LfoClock, 1);
            option(e, xs::LfoPhase, 2);
            option(e, xs::Division, division);
            for (int base = 0; base < 4096; base += block) {
              e.transport(bpm, true, 7.25 + base * bpm / (60 * sr), true, true);
              for (int i = base; i < std::min(4096, base + block); ++i) {
                e.next();
                double cycles =
                    (7.25 + i * bpm / (60 * sr)) / xs::divisionBeats(e.state[xs::Division]);
                double delta = std::abs(e.modulationPhase() - (cycles - std::floor(cycles)));
                require(std::min(delta, 1 - delta) < 2.e-6, "HOST LFO drift");
              }
            }
          }
    for (int kind = 0; kind < 6; ++kind)
      for (int random = 0; random < 3; ++random)
        for (int polarity : {0, 1})
          for (int phase = 0; phase < 3; ++phase) {
            xs::Engine e;
            fixture(e);
            option(e, 26, kind);
            option(e, xs::LfoRandom, random);
            e.set(xs::LfoPolarity, polarity);
            option(e, xs::LfoPhase, phase);
            e.set(4, 1);
            e.transport(120, true, 0, true);
            e.note(60, true);
            for (int i = 0; i < 12000; ++i) {
              e.next();
              float v = P::lfo(e);
              require(v >= (polarity ? 0 : -1) && v <= 1, "LFO range");
            }
            if (phase == 1) {
              e.note(67, true);
              e.next();
              require(e.modulationPhase() < .001, "KEY reset failed");
            }
          }
  });
  test("Matrix all sources/destinations/slots/signs/summing/OFF/random", [] {
    for (int slot = 0; slot < 6; ++slot)
      for (int src = 0; src < 11; ++src)
        for (int dest = 0; dest < 18; ++dest)
          for (double amount : {0., .5, 1.}) {
            xs::Engine e;
            fixture(e);
            int id = xs::MatrixFirst + slot * 3;
            option(e, id, src);
            option(e, id + 1, dest);
            e.set(id + 2, amount);
            e.set(31, .3);
            e.set(xs::Pressure, .7);
            e.set(30, .75);
            e.note(72, true, .6);
            auto &v = P::voice(e);
            v.eg[0].value = .2;
            v.eg[1].value = .4;
            P::route(e, -.8f);
            float source[] = {0, -.8f, .2f, .4f, .6f, 1.f / 3, .3f, .7f, .5f, v.random, 1};
            float target = v.mod[dest] + 8 * v.increment[dest];
            require(std::abs(target - source[src] * (2 * amount - 1)) < 1.e-6,
                    "Matrix scaling/sign/destination");
          }
    xs::Engine e;
    fixture(e);
    for (int r = 0; r < 6; ++r) {
      option(e, xs::MatrixFirst + 3 * r, 10);
      option(e, xs::MatrixFirst + 3 * r + 1, 8);
      e.set(xs::MatrixFirst + 3 * r + 2, .6);
    }
    e.note(60, true);
    P::route(e, 0);
    require(std::abs(P::voice(e).increment[8] * 8 - 1.2) < 1.e-6, "Matrix sum");
    float random = P::voice(e).random;
    advance(e, 48000);
    require(P::voice(e).random == random, "Note Random changed during note");
    xs::Engine b;
    fixture(b);
    b.note(60, true);
    require(P::voice(b).random == random, "Fixed seed mismatch");
    e.note(61, true);
    require(P::voice(e).random != random, "New note reused random value");
  });
  test("Polyphony/stealing/priority/note IDs/glide/tune/bend", [] {
    for (int count = 0; count < 6; ++count) {
      xs::Engine e;
      fixture(e);
      option(e, xs::Voices, count);
      for (int n = 30; n < 75; ++n) {
        e.note(n, true, .7, n);
        advance(e, 31);
      }
      require(e.activeVoices() == xs::VoiceCounts[count], "Voice cap");
      e.allOff();
      advance(e, 30000);
      require(e.activeVoices() == 0, "Stuck poly notes");
      for (int n = 0; n < 2000; ++n) {
        e.note(60, true, .8, n);
        e.note(60, false, 0, n);
        advance(e, 32);
      }
      e.allOff();
      advance(e, 30000);
      require(!e.activeVoices(), "Fast repeated notes stuck");
    }
    for (int mode : {0, 1}) {
      xs::Engine e;
      fixture(e);
      option(e, xs::VoiceMode, mode);
      e.set(xs::Keytrack, .5);
      e.note(60, true);
      e.note(67, true);
      e.note(64, true);
      require(P::voice(e).note == 67, "Mono highest priority");
      e.note(67, false);
      require(P::voice(e).note == 64, "Mono fallback");
      e.allOff();
      advance(e, 30000);
      require(!e.activeVoices(), "Mono stuck");
    }
    for (int mode = 0; mode < 4; ++mode)
      for (bool overlap : {false, true}) {
        xs::Engine e;
        fixture(e);
        option(e, xs::VoiceMode, 1);
        option(e, xs::GlideMode, mode);
        e.set(8, .8);
        e.note(60, true);
        advance(e, 1000);
        if (!overlap)
          e.note(60, false);
        e.note(72, true);
        bool expected = mode == 1 || (mode == 2 && overlap) || (mode == 3 && !overlap);
        require(P::voice(e).glide == expected, "Glide gating");
        float old = P::voice(e).hz;
        advance(e, 1000);
        require(expected ? (P::voice(e).hz > old && P::voice(e).hz < P::voice(e).targetHz)
                         : P::voice(e).hz == P::voice(e).targetHz,
                "Glide transition");
      }
    for (int bend = 0; bend < 10; ++bend)
      for (double wheel : {0., .5, 1.})
        for (double tune : {0., .5, 1.}) {
          xs::Engine e;
          fixture(e);
          option(e, xs::BendRange, bend);
          e.set(30, wheel);
          e.set(xs::MasterTune, tune);
          e.note(69, true);
          double ratio = std::exp2(((wheel * 2 - 1) * xs::BendRanges[bend] + (tune * 2 - 1)) / 12);
          require(std::abs(P::voice(e).pitchRatio[0] - ratio) < 1.e-5, "Tune/bend mapping");
        }
  });
  test("32-voice stress / internal state / no allocations / release", [] {
    for (double sr : {44100., 48000., 88200., 96000.})
      for (int mixer : {4, 5})
        for (int engine = 0; engine < 4; ++engine) {
          xs::Engine e;
          fixture(e, sr);
          option(e, xs::Voices, 5);
          option(e, xs::MixerMode, mixer);
          e.set(xs::Engine1, engine & 1 ? 1 : 0);
          e.set(xs::Engine2, engine & 2 ? 1 : 0);
          e.set(22, 1);
          e.set(27, 1);
          option(e, xs::XMode, 2);
          e.set(xs::XDepth, 1);
          for (int r = 0; r < 6; ++r) {
            option(e, xs::MatrixFirst + r * 3, 1 + r);
            option(e, xs::MatrixFirst + r * 3 + 1, r % 2 ? 8 : 6);
            e.set(xs::MatrixFirst + r * 3 + 2, r % 2 ? 1 : 0);
          }
          auto before = allocations.load();
          for (int n = 36; n < 68; ++n)
            e.note(n, true, .8, n);
          double energy = 0;
          for (int i = 0; i < 24000; ++i) {
            float s = e.next();
            require(std::isfinite(s) && std::abs(s) <= 1 && P::stable(e), "Stress unstable");
            require(std::fpclassify(s) != FP_SUBNORMAL, "Output denormal");
            energy += s * s;
          }
          require(energy > 0, "Stress silence");
          e.allOff();
          advance(e, 30000);
          require(!e.activeVoices(), "Stress stuck voices");
          require(allocations.load() == before, "Audio thread allocation");
        }
  });
  test("CPU scenarios / memory", [] {
    std::cout << "DSP bytes=" << sizeof(xs::Engine) << " shared tables=1212 bytes\n";
    for (int voices : {0, 1, 8, 32})
      for (int clean : {0, 1})
        for (int mode : {0, 1, 2}) {
          xs::Engine e;
          fixture(e);
          option(e, xs::Voices, 5);
          e.set(xs::Engine1, clean);
          e.set(xs::Engine2, clean);
          option(e, xs::XMode, mode);
          e.set(xs::XDepth, .3);
          for (int r = 0; r < 6; ++r) {
            option(e, xs::MatrixFirst + r * 3, 1 + r);
            option(e, xs::MatrixFirst + r * 3 + 1, r);
            e.set(xs::MatrixFirst + r * 3 + 2, .55);
          }
          for (int n = 0; n < voices; ++n)
            e.note(36 + n, true);
          advance(e, 4800);
          auto start = std::chrono::steady_clock::now();
          double sum = 0;
          auto before = allocations.load();
          for (int i = 0; i < 48000; ++i)
            sum += e.next();
          double ms =
              std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start)
                  .count();
          require(allocations.load() == before, "Timed DSP allocated");
          volatile double sink = sum;
          (void)sink;
          std::cout << "cpu voices=" << voices << " clean=" << clean << " xmod=" << mode
                    << " six-routes ms/audio-second=" << ms << '\n';
        }
  });

  test("Mixer equations and switching before filter / envelope", [] {
    for (int mode = 0; mode < 6; ++mode)
      for (double balance : {0., .25, .5, .75, 1.}) {
        xs::Engine e;
        fixture(e);
        option(e, xs::FilterType, 1);
        e.set(21, 0);
        e.set(7, balance);
        e.set(2, .37);
        e.note(69, true);
        advance(e, 2048);
        option(e, xs::MixerMode, mode);
        for (int n = 0; n < 256; ++n) {
          float actual = e.next();
          auto &v = P::voice(e);
          float a = P::raw(v.phase[0], 2, .5), b = P::raw(v.phase[1], 5, .5), mix = 0;
          switch (mode) {
          case 0:
            mix = float((1 - balance) * a + balance * b);
            break;
          case 1:
            mix = float(.5 * (std::min(1., 2 * (1 - balance)) * a + std::min(1., 2 * balance) * b));
            break;
          case 2:
            mix = float((1 - balance) * a + balance * a * b);
            break;
          case 3:
            mix = float((1 - balance) * a + balance * a * (b + 1) * .5);
            break;
          case 4:
            mix = float((1 - balance) * a + balance * ((a >= 0) != (b >= 0) ? 1 : -1));
            break;
          case 5:
            mix = float((1 - balance) * a - balance * b);
            break;
          }
          float expected = float(mix * v.eg[0].value * std::pow(.3, 1 / (2 * std::log10(2.))));
          require(std::abs(expected - actual) < 2.e-6,
                  "Mixer equation or synthesis-stage order mismatch");
        }
      }
  });
  test("Envelope duration across rates / slow loop / velocity", [] {
    for (int curve = 0; curve < 3; ++curve)
      for (int index : {0, 1})
        for (double setting : {0., .5, 1.}) {
          double referenceTime = 0;
          for (double sr : {44100., 48000., 88200., 96000.}) {
            xs::Engine e;
            fixture(e, sr);
            option(e, index ? xs::Curve2 : xs::Curve1, curve);
            e.set(index ? 15 : 9, setting);
            e.note(60, true);
            int n = 0;
            while (P::voice(e).eg[index].stage == 0 && n < int(sr * 30)) {
              P::env(e, index);
              ++n;
            }
            double seconds = n / sr;
            if (sr == 44100)
              referenceTime = seconds;
            require(std::abs(seconds - referenceTime) < std::max(.0007, referenceTime * .001),
                    "Envelope duration varies with sample rate");
          }
        }
    for (int curve = 0; curve < 3; ++curve)
      for (int loop : {1, 2}) {
        xs::Engine e;
        fixture(e, 96000);
        option(e, xs::Curve2, curve);
        option(e, xs::Loop2, loop);
        e.set(15, 1);
        e.set(16, 1);
        e.set(17, .35);
        e.set(18, 1);
        e.note(60, true);
        int attacks = 0, previous = -1;
        for (int i = 0; i < 96000 * 100; ++i) {
          P::env(e, 1);
          int stage = P::voice(e).eg[1].stage;
          if (stage == 0 && previous != 0)
            ++attacks;
          previous = stage;
        }
        require(attacks >= 2, "Slow loop stalled at sustain");
        e.allOff();
        for (int i = 0; i < 96000 * 40; ++i)
          P::env(e, 1);
        require(P::voice(e).eg[1].value < 1.e-6, "Slow loop release stuck");
      }
    for (int index : {0, 1})
      for (int curve = 0; curve < 3; ++curve)
        for (double sustain : {0., .5, 1.}) {
          xs::Engine e;
          fixture(e);
          option(e, index ? xs::Curve2 : xs::Curve1, curve);
          e.set(index ? 17 : 11, sustain);
          e.note(60, true);
          for (int i = 0; i < 10000; ++i)
            P::env(e, index);
          require(std::abs(P::voice(e).eg[index].value - sustain) < 1.e-6,
                  "Sustain endpoint/midpoint");
        }
    for (int index : {0, 1})
      for (double sensitivity : {0., .5, 1.})
        for (float velocity : {.1f, .5f, 1.f}) {
          xs::Engine e;
          fixture(e);
          e.set(index ? xs::Velocity2 : xs::Velocity1, sensitivity);
          e.note(60, true, velocity);
          P::voice(e).eg[index].value = 1;
          P::voice(e).eg[index].stage = 1;
          require(std::abs(P::env(e, index) - (1 - sensitivity + sensitivity * velocity)) < 1.e-6,
                  "Velocity envelope scaling");
        }
  });
  test("Hz and FREE Sync timing / transport stop and seek", [] {
    for (double sr : {44100., 48000., 88200., 96000.})
      for (bool sync : {false, true})
        for (double rate : {.1, 1., 10.}) {
          xs::Engine e;
          fixture(e, sr);
          e.set(4, xs::normalized(4, rate));
          e.set(xs::LfoClock, sync);
          e.transport(120, true, 0, true);
          double expectedRate = sync ? 2 : rate;
          advance(e, 8);
          double start = e.modulationPhase();
          int count = int(sr * .137);
          advance(e, count);
          double cycles = start + count * expectedRate / sr;
          double delta = std::abs(e.modulationPhase() - (cycles - std::floor(cycles)));
          require(std::min(delta, 1 - delta) < .0002, "FREE LFO frequency varies with sample rate");
        }
    xs::Engine e;
    fixture(e);
    e.set(xs::LfoClock, 1);
    option(e, xs::LfoPhase, 2);
    e.transport(120, true, 3.25, true, false);
    advance(e, 5000);
    require(std::abs(e.modulationPhase() - .25) < 1.e-6, "HOST moved while stopped");
    e.transport(120, true, 7.75, true, true);
    e.next();
    require(std::abs(e.modulationPhase() - .75) < 1.e-6, "HOST seek mismatch");
  });
  test("Modulation interpolation and disabled route settling", [] {
    xs::Engine e;
    fixture(e);
    option(e, xs::MatrixFirst, 10);
    option(e, xs::MatrixFirst + 1, 8);
    e.set(xs::MatrixFirst + 2, 1);
    e.note(60, true);
    for (int i = 1; i <= 8; ++i) {
      e.next();
      require(std::abs(P::voice(e).mod[8] - i / 8.f) < 1.e-6,
              "Control interpolation does not span eight samples");
    }
    option(e, xs::MatrixFirst, 0);
    for (int i = 1; i <= 8; ++i) {
      e.next();
      require(std::abs(P::voice(e).mod[8] - (1 - i / 8.f)) < 1.e-6,
              "OFF route retained modulation or did not interpolate");
    }
  });

  test("PM carrier phase stability and FM direction/depth equations", [] {
    for (int mode : {1, 2})
      for (int direction : {0, 1})
        for (double depth : {0., .01, .3, 1.})
          for (double ratio : {0., .37, 1.}) {
            xs::Engine e;
            fixture(e);
            option(e, xs::XMode, mode);
            e.set(xs::XDirection, direction);
            e.set(xs::XDepth, depth);
            e.set(24, 0);
            e.set(25, 0);
            e.set(2, ratio);
            e.note(69, true);
            for (int n = 0; n < 2048; ++n) {
              auto &v = P::voice(e);
              float previous[2] = {v.phase[0], v.phase[1]};
              float hz[2] = {v.hz * v.pitchRatio[0], v.hz * v.pitchRatio[1]};
              e.next();
              int source = direction ? 1 : 0, target = 1 - source;
              auto wrap = [](float x) { return x - std::floor(x); };
              float sourcePhase = wrap(previous[source] + hz[source] / 48000.f);
              float sourceValue = xs::LegacyEngine::sineSample(sourcePhase);
              float targetHz = hz[target] * (mode == 1 ? 1 + 8 * float(depth) * sourceValue : 1);
              float targetPhase = wrap(previous[target] + targetHz / 48000.f);
              require(std::abs(v.phase[source] - sourcePhase) < 2.e-6, "XMOD source pitch changed");
              require(std::abs(v.phase[target] - targetPhase) < 2.e-6,
                      "FM law or PM carrier phase changed");
            }
          }
  });

  // Failed tests never bless new baselines. Existing references are never overwritten.
  if (!failed)
    test("Retained WAV and parameter regression references", [&] {
      std::ofstream metrics(output / "metrics.csv");
      metrics << "case,rms,dc,peak,max_reference_error\n";
      for (const auto &c : cases) {
        auto a = render(c, 48000, 128);
        auto path = output / (std::string(c.name) + ".wav");
        double error = 0;
        if (std::filesystem::exists(path)) {
          std::ifstream in(path, std::ios::binary);
          in.seekg(44);
          Audio old(a.size());
          in.read(reinterpret_cast<char *>(old.data()), old.size() * 4);
          require(bool(in), "Reference length mismatch");
          for (size_t i = 0; i < a.size(); ++i)
            error = std::max(error, double(std::abs(old[i] - a[i])));
          require(error < 1.e-6,
                  "Audio regression; review deliberately before updating references");
        } else {
          require(record, "Missing baseline; explicit --record required");
          wav(path, a);
          auto bytes = xs::encode(patch(c));
          std::ofstream state(output / (std::string(c.name) + ".XseDev"), std::ios::binary);
          state.write(reinterpret_cast<const char *>(bytes.data()), bytes.size());
        }
        float peak = 0;
        for (auto v : a)
          peak = std::max(peak, std::abs(v));
        metrics << c.name << ',' << std::setprecision(10) << rms(a) << ',' << mean(a) << ',' << peak
                << ',' << error << '\n';
      }
    });
  std::cout << "RESULT " << passed << " groups passed, " << failed << " failed\n";
  return failed ? 1 : 0;
}
