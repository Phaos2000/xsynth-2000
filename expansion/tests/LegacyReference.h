// GPL-2.0-or-later. Portable copies of the original non-UI data structures.
#pragma once
#include <vector>
struct value {
  float cont[24];
  unsigned char dete[3];
  unsigned char on_off[5];
};
struct midi_info {
  unsigned char note_table[128];
  float pitch_wheel;
  float mod_wheel;
};
std::vector<float> legacyRender(value *, midi_info *);
