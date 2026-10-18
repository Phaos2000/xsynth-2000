// GPL-2.0-or-later. Canonical source conversion and immutable native library.
#include "PresetBrowser.h"
#include "DssiImport.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
namespace fs = std::filesystem;
void check(bool b, const char *m) {
  if (!b)
    throw std::runtime_error(m);
}
std::string read(const fs::path &p) {
  std::ifstream in(p, std::ios::binary);
  check(bool(in), "Read historical bank");
  return {std::istreambuf_iterator<char>(in), {}};
}
int main(int argc, char **argv) {
  try {
    check(argc == 2, "Expected factory source directory");
    fs::path source = argv[1];
    int index = 0;
    std::vector<fs::path> files;
    for (auto &f : fs::directory_iterator(source / "xsynth-1.0.2"))
      if (f.path().extension() == ".Xpatch")
        files.push_back(f.path());
    std::sort(files.begin(), files.end());
    for (auto &f : files) {
      auto b = read(f);
      std::vector<uint8_t> bytes(b.begin(), b.end());
      xs::State state{};
      check(xs::importPatch(bytes, state), "Original patch conversion");
      auto &p = xs::factory::Presets[index++];
      check(p.name == f.stem().string() && p.state == state && p.bank == 0,
            "Original names/values/order");
    }
    check(index == 11, "Original source count");
    for (int bank = 1; bank <= 2; ++bank) {
      auto parsed = xs::dssi::parse(
          read(source / "xsynth-dssi-0.9.4" /
               (bank == 1 ? "factory_patches.Xsynth" : "version_0.1_patches.Xsynth")));
      check(parsed.error.empty() && parsed.converted == parsed.entries.size(),
            "Official bank conversion");
      check(parsed.entries.size() == size_t(xs::factory::Banks[bank].count), "Official bank count");
      for (auto &e : parsed.entries) {
        auto &p = xs::factory::Presets[index++];
        check(p.name == e.patch.name && p.state == e.state && p.bank == bank,
              "DSSI names/order/native values");
      }
    }
    check(index == xs::factory::Count, "Total count");
    xs::factory::Browser browser;
    check(browser.label(xs::defaults()) == "DEFAULT", "Initial label");
    auto init=xs::factory::initState();browser.select(xs::factory::Init);
    check(browser.label(init)=="INIT","INIT label");init[23]+=.01;
    check(browser.label(init)=="INIT *","INIT dirty label");
    browser.loaded(xs::factory::initState(),"User Init");
    check(browser.label(xs::factory::initState())=="User Init","User INIT filename authority");
    browser.select(xs::factory::Init);
    for (int i = 0; i < xs::factory::Count; ++i) {
      check(browser.adjacent(1) == i, "Next order");
      browser.select(i);
      auto state = xs::factory::Presets[i].state;
      check(browser.label(state) == xs::factory::Presets[i].name, "Loaded label");
      state[23] = state[23] < .9 ? state[23] + .05 : state[23] - .05;
      check(browser.label(state).back() == '*', "Dirty label");
    }
    check(browser.adjacent(1) == xs::factory::Init, "End wrap");
    browser.select(xs::factory::Init);
    check(browser.adjacent(-1) == xs::factory::Count - 1, "Previous wrap");
    for (int i = xs::factory::Count - 1; i >= 0; --i) {
      check(browser.adjacent(-1) == i, "Previous order");
      browser.select(i);
    }
    auto external = xs::defaults();
    external[0] = .317;
    browser.loaded(external, "user patch");
    check(browser.current() == -1 && browser.label(external) == "user patch",
          "External after factory");
    browser.select(3);
    check(browser.label(xs::factory::Presets[3].state) == xs::factory::Presets[3].name,
          "Factory after external");
    for (int i = 0; i < xs::factory::Count; ++i) {
      const auto &state = xs::factory::Presets[i].state;
      for (double n : state)
        check(xs::valid(n), "Factory invalid value");
      auto bytes = xs::encode(state);
      xs::State restored{};
      check(xs::decode(bytes.data(), bytes.size(), restored) && restored == state,
            "Factory native save/recall");
      xs::Engine synth;
      synth.state = state;
      synth.reset(44100);
      synth.note(60, true, .8f);
      double energy = 0;
      for (int k = 0; k < 4096; ++k) {
        float f = synth.next();
        check(std::isfinite(f) && std::abs(f) <= 1, "Factory note-on safety");
        energy += f * f;
      }
      check(energy > 0, "Silent factory patch");
      synth.allOff();
      int quiet = 0;
      for (int k = 0; k < 44100 * 40 && quiet < 512; ++k) {
        float f = synth.next();
        check(std::isfinite(f) && std::abs(f) <= 1, "Factory release safety");
        quiet = f == 0 ? quiet + 1 : 0;
      }
      check(quiet == 512, "Factory note-off did not complete");
    }
    std::cout << "PASS: all " << index
              << " immutable factory presets: source names/values/order, three banks, "
                 "navigation/boundaries, dirty/external labels, native state, finite deterministic "
                 "note-on and completed release\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
