// GPL-2.0-or-later. One classification shared by rendering and interaction.
#pragma once
#include "Layout.h"
namespace xs {
inline int selectorCount(const Control &c, const State &s) {
  int id = effectiveId(c, s);
  return id == 26 ? 8 : stepsFor(id) + 1;
}
inline bool isDropdown(const Control &c, const State &s) {
  int id = effectiveId(c, s);
  return c.kind == ControlKind::Selector &&
         (id == 26 || id >= MatrixFirst || selectorCount(c, s) > 6);
}
} // namespace xs
