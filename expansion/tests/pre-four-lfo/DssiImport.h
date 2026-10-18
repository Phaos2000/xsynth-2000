// GPL-2.0-or-later. Independently written, offline Xsynth-DSSI text importer.
// Format/semantics authority: Xsynth-DSSI 0.9.4 (see DSSI_COMPATIBILITY.md).
#pragma once
#include "Core.h"
#include <locale>
#include <sstream>
#include <string>

namespace xs::dssi {
constexpr size_t MaxBytes = 4 * 1024 * 1024, MaxEntries = 4096;
struct Oscillator {
  double pitch = 1, wave = 0, width = .5;
};
struct Envelope {
  double attack = .1, decay = .1, sustain = 1, release = .1, velocity = 0, pitch = 0, filter = 0;
};
struct Patch {
  int format = -1;
  size_t line = 0;
  std::string name, encodedName;
  Oscillator osc[2];
  Envelope eg[2];
  double sync = 0, balance = .5, rate = .1, wave = 0, lfoPitch = 0, lfoFilter = 0;
  double cutoff = 50, resonance = 0, filter = 0, glide = 1, volume = .5;
};
struct Entry {
  Patch patch;
  State state = defaults();
  bool parsed = false, converted = false;
  std::string error;
  std::vector<std::string> warnings;
};
struct Bank {
  std::vector<Entry> entries;
  std::string error;
  size_t parsed = 0, converted = 0, unsupported = 0;
};
inline std::string trim(const std::string &s) {
  auto a = s.find_first_not_of(" \t\r\n");
  return a == std::string::npos ? "" : s.substr(a, s.find_last_not_of(" \t\r\n") - a + 1);
}
inline bool numbers(const std::string &line, const char *field,
                    std::initializer_list<double *> values) {
  std::istringstream in(line);
  in.imbue(std::locale::classic());
  std::string key, token;
  if (!(in >> key) || key != field)
    return false;
  for (auto p : values) {
    if (!(in >> token) || token.size() > 64)
      return false;
    // Historical files use decimal/scientific notation, never NaN/Inf/hex.
    if (token.find_first_not_of("0123456789.eE+-") != std::string::npos)
      return false;
    std::istringstream n(token);
    n.imbue(std::locale::classic());
    if (!(n >> *p) || !std::isfinite(*p))
      return false;
    char extra;
    if (n >> extra)
      return false;
  }
  return !(in >> token);
}
inline int hex(char c) {
  if (c >= '0' && c <= '9')
    return c - '0';
  if (c >= 'A' && c <= 'F')
    return c - 'A' + 10;
  if (c >= 'a' && c <= 'f')
    return c - 'a' + 10;
  return -1;
}
inline void name(Entry &e, const std::string &encoded) {
  e.patch.encodedName = encoded;
  bool malformed = encoded.empty();
  std::string result;
  for (size_t i = 0; i < encoded.size(); ++i) {
    unsigned char c = static_cast<unsigned char>(encoded[i]);
    if (c == '%') {
      if (i + 2 >= encoded.size() || hex(encoded[i + 1]) < 0 || hex(encoded[i + 2]) < 0) {
        malformed = true;
        result += '?';
        continue;
      }
      c = static_cast<unsigned char>(hex(encoded[i + 1]) * 16 + hex(encoded[i + 2]));
      i += 2;
    } else if (c <= 32 || c > 126)
      malformed = true;
    if (c < 32 || c == 127) {
      malformed = true;
      result += '?';
    } else
      result += char(c);
  }
  while (!result.empty() && result.back() == ' ')
    result.pop_back();
  if (result.size() > 30) {
    malformed = true;
    result.resize(30);
  }
  if (result.empty())
    result = "Unnamed patch at line " + std::to_string(e.patch.line);
  e.patch.name = result;
  if (malformed)
    e.warnings.push_back("Malformed name; readable fallback used (source line " +
                         std::to_string(e.patch.line) + ", raw name retained in diagnostics).");
}
inline bool range(double v, double lo, double hi, bool integer = false) {
  return std::isfinite(v) && v >= lo - 1e-9 && v <= hi + 1e-6 && (!integer || v == std::floor(v));
}
inline bool ranges(const Patch &p) {
  for (auto &o : p.osc)
    if (!range(o.pitch, .25, 4) || !range(o.wave, 0, 6, true) || !range(o.width, 0, 1))
      return false;
  for (auto &e : p.eg)
    if (!range(e.attack, .00001, .1) || !range(e.decay, .00001, .1) ||
        !range(e.release, .00001, .1) || !range(e.sustain, 0, 1) || !range(e.velocity, 0, 1) ||
        !range(e.pitch, 0, 1) || !range(e.filter, 0, 50))
      return false;
  return range(p.sync, 0, 1, true) && range(p.balance, 0, 1) && range(p.rate, .1, 10) &&
         range(p.wave, 0, 5, true) && range(p.lfoPitch, 0, 1) && range(p.lfoFilter, 0, 1) &&
         range(p.cutoff, 0, 50) && range(p.resonance, 0, 1.995) && range(p.filter, 0, 2, true) &&
         range(p.glide, .002, 1) && range(p.volume, 0, 1);
}
// DSSI's TX7-style sensitivity is nonlinear; fit the existing linear velocity
// control over MIDI 1..127. This is explicitly an approximation, not new DSP.
inline double velocity(double sensitivity) {
  double numerator = 0, denominator = 0;
  for (int k = 1; k <= 127; ++k) {
    double v = k / 127., knee = std::max(k, 10) / 127.;
    double amplitude = std::exp2((std::pow(knee, .32) - 1) * 12.5) * std::min(1., k / 10.);
    double target = std::pow(amplitude, sensitivity);
    numerator += (1 - v) * (1 - target);
    denominator += (1 - v) * (1 - v);
  }
  return std::clamp(numerator / denominator, 0., 1.);
}
inline bool convert(Entry &e) {
  const auto &p = e.patch;
  auto s = defaults();
  // The ordinary VCO controls now cover DSSI's complete range directly.
  // Validate strictly before inverse mapping; never clamp imported pitch.
  for(const auto& oscillator:p.osc)if(!std::isfinite(oscillator.pitch)||oscillator.pitch<.25||oscillator.pitch>4) {
    e.error="Oscillator pitch is outside the historical 0.25..4 range.";return false;
  }
  auto put = [&](int id, double value) {
    double lo = std::min(physical(id, 0), physical(id, 1)),
           hi = std::max(physical(id, 0), physical(id, 1));
    if (value < lo - 1e-7 || value > hi + 1e-6)
      e.warnings.push_back(
          std::string(specs[id].name) +
          " exceeds the destination range after conversion; limited to its endpoint.");
    s[id] = normalized(id, std::clamp(value, lo, hi));
  };
  for (int o = 0; o < 2; ++o) {
    s[o*2]=normalized(o*2,p.osc[o].pitch);
    put(o * 2 + 1, p.osc[o].width);
    int wave = int(p.osc[o].wave);
    if (wave == 6) {
      wave = 1;
      if (std::abs(p.osc[o].width - .5) > 1e-6)
        e.warnings.push_back("Variable-slope triangle approximated by symmetric triangle.");
    }
    s[24 + o] = wave / 5.;
    s[Engine1 + o] = 1;
    auto &g = p.eg[o];
    int base = o ? 15 : 9;
    put(base, g.attack);
    put(base + 1, g.decay);
    put(base + 2, g.sustain);
    put(base + 3, g.release);
    put(base + 4, g.pitch);
    put(base + 5, g.filter);
    s[o ? Velocity2 : Velocity1] = velocity(g.velocity);
    if (g.velocity > 0)
      e.warnings.push_back(
          std::string(o ? "EG2" : "EG1") +
          " velocity uses a fitted linear response; DSSI's nonlinear response is not exact.");
  }
  s[27] = p.sync;
  put(7, p.balance);
  put(4, p.rate);
  s[26] = p.wave / 5.;
  put(5, p.lfoPitch);
  put(6, p.lfoFilter);
  s[LfoPhase] = .5; // KEY: nearest onset behavior to DSSI's per-voice LFO.
  if (p.lfoPitch || p.lfoFilter)
    e.warnings.push_back(
        "LFO is shared and restarts on a new key; DSSI has independent per-voice LFO phases.");
  put(21, p.cutoff);
  put(22, 2 - p.resonance);
  s[28] = p.filter == 0 ? 0 : 1;
  if (p.filter == 2)
    e.warnings.push_back("DSSI MVCLPF-3 filter approximated by LP 24 dB; its nonlinear resonance "
                         "and tone are not reproduced.");
  put(8, p.glide >= 1 ? 1 : -std::expm1(4 * std::log1p(-p.glide)));
  // DSSI oscillators have half amplitude; output gain is (2*v)^e / 4.
  const double exponent = 1 / (2 * std::log10(2.));
  put(23, 2 * p.volume / std::pow(8., 1 / exponent));
  s[VoiceMode] = 1;
  s[Voices] = 2. / 5;
  s[GlideMode] = 2. / 3;
  s[BendRange] = 2. / 9;
  if (p.sync)
    e.warnings.push_back("Hard-sync reset/antialiasing differs from DSSI's minBLEP sync renderer.");
  e.warnings.push_back("CLEAN is the nearest oscillator engine, not DSSI minBLEP. Envelope/glide "
                       "timing is calibrated at 44.1 kHz; genuine DSSI A/B remains outstanding.");
  for (double v : s)
    if (!valid(v)) {
      e.error = "Converted state is invalid.";
      return false;
    }
  e.state = s;
  return true;
}
inline Bank parse(const std::string &text) {
  Bank bank;
  if (text.size() > MaxBytes || text.find('\0') != std::string::npos) {
    bank.error = "Bank is too large or contains binary data.";
    return bank;
  }
  std::vector<std::pair<size_t, std::string>> lines;
  std::istringstream input(text);
  std::string line;
  size_t number = 0;
  while (std::getline(input, line)) {
    ++number;
    line = trim(line);
    if (line.size() > 1024) {
      bank.error = "Line exceeds 1024 bytes at line " + std::to_string(number);
      return bank;
    }
    if (!line.empty() && line[0] != '#')
      lines.emplace_back(number, line);
  }
  for (size_t i = 0; i < lines.size();) {
    if (lines[i].second.rfind("xsynth-dssi patch format", 0) != 0) {
      bank.error = "Unexpected data at line " + std::to_string(lines[i].first);
      return bank;
    }
    if (bank.entries.size() == MaxEntries) {
      bank.error = "Too many patches in bank.";
      return bank;
    }
    size_t start = i++;
    while (i < lines.size() && lines[i].second.rfind("xsynth-dssi patch format", 0) != 0)
      ++i;
    Entry e;
    e.patch.line = lines[start].first;
    auto &p = e.patch;
    std::string a, b, c, end, extra;
    std::istringstream header(lines[start].second);
    bool headerOK = bool(header >> a >> b >> c >> p.format >> end) && a == "xsynth-dssi" &&
                    b == "patch" && c == "format" && end == "begin" && !(header >> extra);
    if (!headerOK)
      e.error = "Malformed patch header.";
    else if (p.format != 0 && p.format != 1) {
      e.error = "Unsupported patch format " + std::to_string(p.format);
      ++bank.unsupported;
    } else if (i - start != 13)
      e.error = "Incomplete patch, missing end, or unknown/extra field.";
    else {
      size_t j = start + 1;
      bool ok = true;
      const auto &nameLine = lines[j].second;
      if (nameLine == "name")
        name(e, "");
      else if (nameLine.size() > 4 && nameLine.compare(0, 4, "name") == 0 &&
               (nameLine[4] == ' ' || nameLine[4] == '\t'))
        name(e, trim(nameLine.substr(5)));
      else
        ok = false;
      ++j;
      for (int o = 0; o < 2; ++o) {
        auto &v = p.osc[o];
        ok = numbers(lines[j++].second, o ? "osc2" : "osc1", {&v.pitch, &v.wave, &v.width}) && ok;
      }
      ok = numbers(lines[j++].second, "sync", {&p.sync}) && ok;
      ok = numbers(lines[j++].second, "balance", {&p.balance}) && ok;
      ok = numbers(lines[j++].second, "lfo", {&p.rate, &p.wave, &p.lfoPitch, &p.lfoFilter}) && ok;
      for (int o = 0; o < 2; ++o) {
        auto &g = p.eg[o];
        const char *key = o ? "eg2" : "eg1";
        if (p.format == 0)
          ok = numbers(lines[j++].second, key,
                       {&g.attack, &g.decay, &g.sustain, &g.release, &g.pitch, &g.filter}) &&
               ok;
        else
          ok = numbers(lines[j++].second, key,
                       {&g.attack, &g.decay, &g.sustain, &g.release, &g.velocity, &g.pitch,
                        &g.filter}) &&
               ok;
      }
      ok = numbers(lines[j++].second, "vcf", {&p.cutoff, &p.resonance, &p.filter}) && ok;
      ok = numbers(lines[j++].second, "glide", {&p.glide}) && ok;
      ok = numbers(lines[j++].second, "volume", {&p.volume}) && ok;
      ok = lines[j].second == "xsynth-dssi patch end" && ok;
      if (!ok)
        e.error = "Malformed, missing, out-of-order or unknown field.";
      else if (!ranges(p))
        e.error = "Historical parameter outside the documented range.";
      else {
        e.parsed = true;
        ++bank.parsed;
        e.converted = convert(e);
        if (e.converted)
          ++bank.converted;
        else
          ++bank.unsupported;
      }
    }
    if (p.name.empty())
      p.name = "Unreadable patch at line " + std::to_string(p.line);
    bank.entries.push_back(std::move(e));
  }
  if (bank.entries.empty())
    bank.error = "No Xsynth-DSSI patches found.";
  return bank;
}
} // namespace xs::dssi
