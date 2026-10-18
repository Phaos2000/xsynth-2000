// GPL-2.0-or-later. One classification shared by rendering and interaction.
#pragma once
#include "Layout.h"
namespace xs {
inline int selectorCount(const Control &c, const State &s) {
  int id = effectiveId(c, s);
  if(matrixDestination(id))return DestinationCount;
  if(matrixSource(id))return SourceCount;
  return lfoBase(id) == 26 ? 8 : stepsFor(id) + 1;
}
inline bool isDropdown(const Control &c, const State &s) {
  int id = effectiveId(c, s);
  if(matrixDestination(id))return DestinationCount;
  if(matrixSource(id))return SourceCount;
  return c.kind == ControlKind::Selector &&
         (lfoBase(id) == 26 || matrixSlot(id)>=0 || selectorCount(c, s) > 6);
}
} // namespace xs
