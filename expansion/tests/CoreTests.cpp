#include "Core.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
void check(bool b, const char *m) {
  if (!b)
    throw std::runtime_error(m);
}
int main(int argc, char **argv) {
  try {
    for (int i = 0; i < 24; ++i)
      for (int k = 0; k <= 100; ++k) {
        double n = k / 100.;
        check(std::abs(xs::normalized(i, xs::physical(i, n)) - n) < 1.e-8,
              "mapping");
      }
    int count = 0;
    for (auto &f :
         std::filesystem::directory_iterator(argc > 1 ? argv[1] : ".")) {
      if (f.path().extension() != ".Xpatch")
        continue;
      std::ifstream in(f.path(), std::ios::binary);
      std::vector<uint8_t> b{std::istreambuf_iterator<char>(in), {}};
      xs::State s;
      check(xs::importPatch(b, s), "legacy import");
      xs::State s2;
      check(xs::importPatch(xs::exportPatch(s), s2), "legacy roundtrip");
      for (int i = 0; i < xs::Count; ++i)
        check(std::abs(s[i] - s2[i]) < 1.e-5, "patch value");
      auto serialized = xs::encode(s);
      check(xs::decode(serialized.data(), serialized.size(), s2) && s == s2,
            "state roundtrip");
      serialized[0] = 0;
      check(!xs::decode(serialized.data(), serialized.size(), s2),
            "state corruption");
      for (double rate : {44100., 48000., 96000., 192000.}) {
        xs::Engine e;
        e.state = s;
        e.reset(rate);
        e.note(60, true);
        double energy = 0;
        for (int n = 0; n < 30000; ++n) {
          float y = e.next();
          check(std::isfinite(y) && std::abs(y) <= 1, "finite audio");
          energy += y * y;
        }
        check(energy > 0, "patch sound");
      }
      b.resize(208);
      s2 = xs::defaults();
      auto original = s2;
      check(!xs::importPatch(b, s2) && s2 == original, "atomic rejection");
      ++count;
    }
    check(count == 11, "all 11 patches");
    xs::Engine a, b;
    a.reset(44100);
    b.reset(44100);
    a.note(60, true);
    b.note(60, true);
    double energy = 0;
    for (int n = 0; n < 10000; ++n) {
      auto x = a.next();
      check(x == b.next(), "determinism");
      energy += x * x;
    }
    check(energy > 1, "non silent");
    a.note(72, true);
    a.next();
    check(a.heldNote() == 72, "highest priority");
    a.note(72, false);
    a.next();
    check(a.heldNote() == 60, "fallback");
    a.allOff();
    for (int n = 0; n < 150000; ++n)
      a.next();
    check(a.next() == 0, "release silence");
    std::cout << "Passed: maps, " << count
              << " legacy patches at four rates, state, rejection, "
                 "determinism, priority, release\n";
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
