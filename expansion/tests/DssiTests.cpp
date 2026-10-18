// GPL-2.0-or-later. Offline importer/corpus/immutable audio regression tests.
#include "DssiImport.h"
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
namespace fs = std::filesystem;
void check(bool b, const std::string &message) {
  if (!b)
    throw std::runtime_error(message);
}
void near(double a, double b, const char *message) { check(std::abs(a - b) < 1e-7, message); }
std::string read(const fs::path &path) {
  std::ifstream in(path, std::ios::binary);
  check(bool(in), "Read failed: " + path.string());
  return {std::istreambuf_iterator<char>(in), {}};
}
void immutable(const fs::path &path, const std::string &data, bool record) {
  if (record && !fs::exists(path)) {
    std::ofstream out(path, std::ios::binary);
    out.write(data.data(), data.size());
    check(bool(out), "Write failed");
  } else
    check(read(path) == data, "Regression mismatch: " + path.string());
}
std::string basic(int version) {
  return "# Fixture written for the importer, not an upstream patch\nxsynth-dssi patch format " +
         std::to_string(version) +
         " begin\nname Test%20Patch%25\nosc1 1 2 0.3\nosc2 2 5 0.2\nsync 1\nbalance 0.4\nlfo 2 3 "
         "0.2 0.3\neg1 0.01 0.002 0.6 0.003 " +
         (version ? "0.5 " : "") + "0.1 4\neg2 0.02 0.003 0.7 0.004 " + (version ? "0.8 " : "") +
         "0.2 5\nvcf 6 1.2 1\nglide 0.1\nvolume 0.6\nxsynth-dssi patch end\n";
}
std::string replace(std::string s, const std::string &a, const std::string &b) {
  auto at = s.find(a);
  check(at != std::string::npos, "Test replacement missing");
  s.replace(at, a.size(), b);
  return s;
}
void unit() {
  for (int version : {0, 1}) {
    auto bank = xs::dssi::parse(basic(version));
    check(bank.converted == 1, "Version fixture failed");
    auto &e = bank.entries[0];
    auto &s = e.state;
    auto &p = e.patch;
    check(p.format == version && p.name == "Test Patch%", "Version/name mismatch");
    near(xs::physical(0, s[0]), 1, "OSC1");
    near(xs::physical(2, s[2]), 2, "OSC2");
    near(s[1], .3, "PW1");
    near(s[3], .2, "PW2");
    near(s[24], .4, "Wave1");
    near(s[25], 1, "Wave2");
    near(s[27], 1, "Sync");
    near(s[7], .4, "Balance");
    near(xs::physical(4, s[4]), 2, "LFO rate");
    near(s[26], .6, "LFO wave");
    near(s[5], .2, "LFO pitch");
    near(s[6], .3, "LFO filter");
    near(xs::physical(9, s[9]), .01, "EG1 attack");
    near(xs::physical(10, s[10]), .002, "EG1 decay");
    near(s[11], .6, "EG1 sustain");
    near(xs::physical(12, s[12]), .003, "EG1 release");
    near(s[13], .1, "EG1 pitch");
    near(xs::physical(14, s[14]), 4, "EG1 filter");
    near(xs::physical(15, s[15]), .02, "EG2 attack");
    near(xs::physical(16, s[16]), .003, "EG2 decay");
    near(s[17], .7, "EG2 sustain");
    near(xs::physical(18, s[18]), .004, "EG2 release");
    near(s[19], .2, "EG2 pitch");
    near(xs::physical(20, s[20]), 5, "EG2 filter");
    near(p.eg[0].velocity, version ? .5 : 0, "Version-specific EG1 velocity");
    near(p.eg[1].velocity, version ? .8 : 0, "Version-specific EG2 velocity");
    near(xs::physical(21, s[21]), 6, "Cutoff");
    near(xs::physical(22, s[22]), .8, "Resonance inversion");
    near(s[28], 1, "Slope");
    near(xs::physical(8, s[8]), .3439, "64 to 256 glide interval");
    double exponent = 1 / (2 * std::log10(2.));
    near(std::pow(s[23], exponent), std::pow(1.2, exponent) / 8,
         "Oscillator/output level compensation");
    check(s[xs::Engine1] == 1 && s[xs::Engine2] == 1 && s[xs::FilterType] == 0 &&
              s[xs::Keytrack] == 1,
          "Engine/filter defaults");
    check(s[xs::XMode] == 0 && s[xs::XDepth] == 0 && s[xs::MixerMode] == 0 &&
              s[xs::LfoClock] == 0 && s[xs::Loop2] == 0,
          "Neutral extension defaults");
    for (int i = 0; i < 6; ++i)
      check(s[xs::MatrixFirst + i * 3] == 0 && s[xs::MatrixFirst + i * 3 + 2] == .5,
            "Non-neutral matrix");
    auto bytes = xs::encode(s);
    xs::State restored{};
    check(xs::decode(bytes.data(), bytes.size(), restored) && restored == s,
          "Native import roundtrip");
  }
  auto base = basic(1);
  std::vector<std::string> bad = {replace(base, "xsynth-dssi patch end", ""),
                                  replace(base, "osc1 1 2 0.3", "osc1 1 2"),
                                  replace(base, "osc1 1 2 0.3", "osc1 NaN 2 0.3"),
                                  replace(base, "format 1", "format 9"),
                                  base.substr(0, 80),
                                  replace(base, "balance 0.4", "unknown 0.4"),
                                  replace(base, "vcf 6 1.2 1", "vcf 6 1.2 1 extra"),
                                  replace(base, "osc1 1 2 0.3", "osc1 1 2.5 0.3"),
                                  replace(base, "glide 0.1", "glide 0"),
                                  replace(base, "eg1 0.01", "eg1 1e999"),
                                  replace(base, "begin", "begin junk")};
  for (auto &text : bad) {
    auto b = xs::dssi::parse(text);
    check(b.converted == 0, "Malformed input accepted");
  }
  auto badName = xs::dssi::parse(replace(base, "Test%20Patch%25", "Bad%QZ%00"));
  check(badName.converted == 1 && !badName.entries[0].warnings.empty(), "Malformed name fallback");
  auto emptyName = xs::dssi::parse(replace(base, "Test%20Patch%25", ""));
  check(emptyName.converted == 1 && emptyName.entries[0].patch.name.find("Unnamed patch") == 0,
        "Empty name fallback");
  check(xs::dssi::parse(replace(base, "name Test", "name\tTest")).converted == 1,
        "Name field whitespace");
  near(xs::dssi::velocity(0), 0, "Format 0 must disable velocity");
  check(xs::dssi::velocity(.2) > 0 && xs::dssi::velocity(.2) < xs::dssi::velocity(.4) &&
            xs::dssi::velocity(1) <= 1,
        "Velocity fit monotonicity/bounds");
  check(xs::dssi::parse(base + std::string(1, '\0')).converted == 0, "NUL accepted");
  check(!xs::dssi::parse(std::string(xs::dssi::MaxBytes + 1, ' ')).error.empty(),
        "Oversize accepted");
  check(xs::dssi::parse(bad[0] + base).converted == 1, "Could not recover next valid entry");
  auto low = xs::dssi::parse(replace(base, "osc1 1 2", "osc1 0.5 2"));
  check(low.parsed == 1 && low.converted == 1, "Sub-unity pitch rejected");
  near(xs::physical(0, low.entries[0].state[0]), .5, "Sub-unity pitch altered");
  auto detune = xs::dssi::parse(replace(base, "osc1 1 2", "osc1 0.99 2"));
  check(detune.converted == 1, "Small common detune failed");
  auto &s = detune.entries[0].state;
  near(s[xs::MasterTune], .5, "Artificial Master Tune correction");
  near(xs::physical(0, s[0]), .99, "Direct detune conversion");
  near(xs::physical(21, s[21]), 6, "Direct filter conversion");
  for (double first : {.25, .5, 1., 2., 4.})
    for (double second : {.25, .5, 1., 2., 4.}) {
      auto entry = xs::dssi::parse(base).entries[0];
      entry.patch.osc[0].pitch = first;
      entry.patch.osc[1].pitch = second;
      check(xs::dssi::convert(entry), "Valid oscillator pair rejected");
      near(xs::physical(0, entry.state[0]), first, "VCO1 pair altered");
      near(xs::physical(2, entry.state[2]), second, "VCO2 pair altered");
      near(entry.state[xs::MasterTune], .5, "Pair changed Master Tune");
    }
  for (double b : {0., .5, 1.}) {
    auto e = xs::dssi::parse(base).entries[0];
    e.patch.balance = b;
    check(xs::dssi::convert(e), "Balance conversion");
    near(e.state[7], b, "Balance law changed");
  }
  std::cout << "Format 0/1 independent mapping; malformed inputs; name fallback; neutral defaults; "
               "direct full-range pitch; native state: PASS\n";
}
std::string stateText(const xs::dssi::Entry &e, const char *bank, int index) {
  std::ostringstream out;
  out.imbue(std::locale::classic());
  out << std::setprecision(17);
  out << "Source " << bank << " index " << index << " line " << e.patch.line << " format "
      << e.patch.format << "\nName " << e.patch.name << "\nEncoded " << e.patch.encodedName << "\n";
  auto &p = e.patch;
  for (int i = 0; i < 2; ++i) {
    auto &o = p.osc[i];
    out << "osc" << i + 1 << ' ' << o.pitch << ' ' << o.wave << ' ' << o.width << '\n';
  }
  out << "sync " << p.sync << "\nbalance " << p.balance << "\nlfo " << p.rate << ' ' << p.wave
      << ' ' << p.lfoPitch << ' ' << p.lfoFilter << '\n';
  for (int i = 0; i < 2; ++i) {
    auto &g = p.eg[i];
    out << "eg" << i + 1 << ' ' << g.attack << ' ' << g.decay << ' ' << g.sustain << ' '
        << g.release << ' ' << g.velocity << ' ' << g.pitch << ' ' << g.filter << '\n';
  }
  out << "vcf " << p.cutoff << ' ' << p.resonance << ' ' << p.filter << "\nglide " << p.glide
      << "\nvolume " << p.volume << '\n';
  if (e.converted)
    for (int i = 0; i < xs::Count; ++i)
      out << "state " << i << ' ' << e.state[i]
          << (i < 24 ? " physical " + std::to_string(xs::physical(i, e.state[i])) : "") << '\n';
  else
    out << "NOT CONVERTED: " << e.error << '\n';
  for (auto &w : e.warnings)
    out << "NOTE " << w << '\n';
  return out.str();
}
std::vector<float> render(const xs::State &state, int note, float velocity, bool chord, bool glide,
                          int rate, int frames = 0) {
  xs::Engine synth;
  synth.reset(rate);
  for (int i = 0; i < xs::Count; ++i)
    synth.set(i, state[i]);
  synth.note(note, true, velocity);
  if (chord) {
    synth.note(note + 4, true, .65f);
    synth.note(note + 7, true, .7f);
  }
  std::vector<float> audio(frames ? frames : rate * 2);
  for (int i = 0; i < int(audio.size()); ++i) {
    if (glide && i == rate / 2)
      synth.note(note + 7, true, velocity);
    if (i == rate * 5 / 4)
      synth.allOff();
    audio[i] = synth.next();
    check(std::isfinite(audio[i]) && std::abs(audio[i]) <= 1, "Invalid imported audio");
  }
  return audio;
}
std::string wav(const std::vector<float> &samples, int rate) {
  std::vector<uint8_t> b;
  auto tag = [&](const char *t) {
    for (int i = 0; i < 4; ++i)
      b.push_back(uint8_t(t[i]));
  };
  auto u16 = [&](unsigned n) {
    b.push_back(uint8_t(n));
    b.push_back(uint8_t(n >> 8));
  };
  tag("RIFF");
  xs::append32(b, unsigned(36 + samples.size() * 4));
  tag("WAVE");
  tag("fmt ");
  xs::append32(b, 16);
  u16(3);
  u16(1);
  xs::append32(b, rate);
  xs::append32(b, rate * 4);
  u16(4);
  u16(32);
  tag("data");
  xs::append32(b, unsigned(samples.size() * 4));
  for (float f : samples)
    xs::putFloat(b, f);
  return {reinterpret_cast<const char *>(b.data()), b.size()};
}
int main(int argc, char **argv) {
  try {
    check(argc >= 2, "Expected fixture directory");
    fs::path root = argv[1];
    bool record = argc > 2 && std::string(argv[2]) == "--record";
    unit();
    fs::create_directories(root / "reference-direct-pitch");
    for (int v : {0, 1})
      immutable(root / ("format" + std::to_string(v) + ".dssi"), basic(v), record);
    const char *names[] = {"XsynthBank1", "XsynthBank3", "XsynthBank2018", "MoogLikeBank"};
    std::array<xs::dssi::Bank, 4> banks;
    size_t discovered = 0, parsed = 0, converted = 0, unsupported = 0;
    int versions[2]{};
    std::ostringstream diagnostic;
    for (int b = 0; b < 4; ++b) {
      banks[b] = xs::dssi::parse(read(root / "banks" / names[b]));
      auto &bank = banks[b];
      check(bank.error.empty(), bank.error);
      check(!bank.entries.empty(), "Empty bank");
      size_t warned = 0, warningCount = 0;
      for (auto &e : bank.entries) {
        warned += !e.warnings.empty();
        warningCount += e.warnings.size();
      }
      std::cout << names[b] << ": discovered=" << bank.entries.size() << " parsed=" << bank.parsed
                << " converted=" << bank.converted << " unsupported=" << bank.unsupported
                << " warning-patches=" << warned << " warnings=" << warningCount
                << " malformed=" << bank.entries.size() - bank.parsed << '\n';
      discovered += bank.entries.size();
      parsed += bank.parsed;
      converted += bank.converted;
      unsupported += bank.unsupported;
      for (size_t i = 0; i < bank.entries.size(); ++i) {
        auto &e = bank.entries[i];
        if (e.parsed)
          ++versions[e.patch.format];
        diagnostic << stateText(e, names[b], int(i)) << '\n';
        if (e.converted) {
          near(e.state[xs::MasterTune], .5, "Corpus Master Tune");
          for (int o = 0; o < 2; ++o)
            near(xs::physical(o * 2, e.state[o * 2]), e.patch.osc[o].pitch,
                 "Corpus direct oscillator ratio");
          near(xs::physical(21, e.state[21]), e.patch.cutoff, "Corpus direct cutoff");
          near(xs::physical(14, e.state[14]), e.patch.eg[0].filter, "Corpus direct EG1 filter");
          near(xs::physical(20, e.state[20]), e.patch.eg[1].filter, "Corpus direct EG2 filter");
          auto encoded = xs::encode(e.state);
          xs::State result{};
          check(xs::decode(encoded.data(), encoded.size(), result) && result == e.state,
                "Corpus native recall");
          for (int note : {36, 60, 84})
            render(e.state, note, 100 / 127.f, false, false, 44100, 4096);
        }
      }
    }
    check(converted == discovered && unsupported == 0, "Valid corpus patch remains unavailable");
    immutable(root / "reference-direct-pitch" / "corpus-mapping.txt", diagnostic.str(), record);
    std::cout << "TOTAL discovered=" << discovered << " parsed=" << parsed
              << " converted=" << converted << " unsupported=" << unsupported
              << " format0=" << versions[0] << " format1=" << versions[1] << '\n';
    struct Fixture {
      int bank, index;
      const char *label;
    };
    const Fixture fixtures[] = {{0, 0, "strings-velocity"},
                                {0, 4, "sync-glide"},
                                {1, 9, "basic-saw"},
                                {1, 42, "sync-saws"},
                                {2, 2, "pulse-lead"},
                                {2, 38, "resonant-pluck"},
                                {2, 49, "bass"},
                                {3, 1, "moog-filter-approximation"},
                                {0, 6, "format0-xwobbbly"},
                                {3, 0, "formerly-blocked-moog"},
                                {0, 11, "formerly-blocked-juno"}};
    for (auto &f : fixtures) {
      auto &e = banks[f.bank].entries.at(f.index);
      check(e.converted, std::string("Fixture unconvertible: ") + f.label + " " + e.error);
      for (int n : {36, 60, 84}) {
        auto audio = render(e.state, n, 100 / 127.f, false, false, 44100);
        check(audio == render(e.state, n, 100 / 127.f, false, false, 44100),
              "Nondeterministic render");
        immutable(root / "reference-direct-pitch" /
                      (std::string(f.label) + "-" + std::to_string(n) + ".wav"),
                  wav(audio, 44100), record);
      }
      for (int mode = 0; mode < 3; ++mode) {
        auto audio =
            render(e.state, 60, mode == 0 ? 32 / 127.f : 100 / 127.f, mode == 1, mode == 2, 44100);
        immutable(root / "reference-direct-pitch" /
                      (std::string(f.label) +
                       (mode == 0   ? "-velocity"
                        : mode == 1 ? "-chord"
                                    : "-glide") +
                       ".wav"),
                  wav(audio, 44100), record);
      }
      for (int rate : {48000, 88200, 96000})
        render(e.state, 84, 1, false, false, rate);
    }
    std::cout << "66 deterministic imported WAVs + four sample rates; all converted patches "
                 "rendered at low/mid/high notes: PASS. Genuine DSSI audio A/B: OUTSTANDING.\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
