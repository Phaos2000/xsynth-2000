// GPL-2.0-or-later. Canonical INIT preset, derived from compatibility defaults.
// Does not change engine/import/migration defaults or any synthesis algorithm.
#pragma once
#include "Core.h"
namespace xs::factory {
inline constexpr int Init = -2, InitCommand = 999;
inline State initState() {
  auto s = defaults();
  s[24] = 2. / 5; // VCO1 SAW UP; both pitches/PW already neutral.
  s[7] = 0;       // BAL, VCO1 only. No hidden mute parameter.
  s[Keytrack] = 0;
  // Retain established 1.04 ms attack/decay; use short practical 50 ms releases.
  s[12] = normalized(12, 1 - std::exp(-4605.17 / (44100 * 50)));
  s[18] = s[12];
  s[VoiceMode] = 1;      // POLY
  s[GlideMode] = 2. / 3; // LEGATO; glide time remains zero.
  return s;
}
} // namespace xs::factory
