// GPL-2.0-or-later. Differential test against the original C DSP, not a rewrite.
#include "Core.h"
#include "LegacyReference.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <stdexcept>
int main(int argc, char **argv) {
  try {
    double worst = 0;
    for (auto &file : std::filesystem::directory_iterator(argc > 1 ? argv[1] : ".")) {
      if (file.path().extension() != ".Xpatch")
        continue;
      std::ifstream in(file.path(), std::ios::binary);
      std::vector<uint8_t> bytes{std::istreambuf_iterator<char>(in), {}};
      xs::State state;
      if (!xs::importPatch(bytes, state))
        throw std::runtime_error("legacy import");
      value controls{};
      for (int i = 0; i < 24; ++i)
        controls.cont[i] = xs::getFloat(bytes.data() + i * 8 + 4);
      for (int i = 0; i < 3; ++i)
        controls.dete[i] = bytes[192 + i * 5 + 4];
      controls.on_off[0] = bytes[207];
      controls.on_off[1] = bytes[208];
      midi_info midi{};
      midi.note_table[60] = 100;
      midi.pitch_wheel = midi.mod_wheel = 1;
      auto reference = legacyRender(&controls, &midi);
      xs::Engine port;
      port.state = state;
      port.reset(44100);
      port.note(60, true);
      double error = 0, energy = 0, maxError = 0;
      for (auto sample : reference) {
        if (!std::isfinite(sample))
          throw std::runtime_error("nonfinite original DSP");
        double expected = std::clamp(sample, -1.f, 1.f), actual = port.next();
        error += (actual - expected) * (actual - expected);
        energy += expected * expected;
        maxError = std::max(maxError, std::abs(actual - expected));
      }
      double relative = std::sqrt(error / std::max(energy, 1.e-20));
      worst = std::max(worst, relative);
      std::cout << file.path().filename().string() << ": relative RMS " << std::setprecision(7)
                << relative << ", peak error " << maxError << '\n';
    }
    if (worst > 0.002)
      throw std::runtime_error("legacy comparison exceeds 0.2% RMS tolerance");
    std::cout << "Passed: original C DSP comparison at 44.1 kHz, initialized startup, all bundled "
                 "patches\n";
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
