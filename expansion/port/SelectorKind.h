// GPL-2.0-or-later. One classification shared by rendering and interaction.
#pragma once
#include "Layout.h"
namespace xs {
// Follow a displayed list: wheel up is previous/up, down is next/down.
// Numeric knobs keep their separate up=increase convention.
inline constexpr int selectorWheelStep(int delta) { return delta>0?-1:delta<0?1:0; }
inline int selectorCount(const Control &c, const State &s) {
  int id = effectiveId(c, s);
  if(id==Voices)return 7;
  if(id==Eg2Sync)return 5;
  if(id==Loop2)return 2;
  if(matrixDestination(id))return DestinationCount;
  if(matrixSource(id))return SourceCount;
  return lfoBase(id) == 26 ? 8 : stepsFor(id) + 1;
}
inline bool isDropdown(const Control &c, const State &s) {
  int id = effectiveId(c, s);
  if(id==Voices)return false; // Preserve the established cycling-button style.
  if(matrixDestination(id))return DestinationCount;
  if(matrixSource(id))return SourceCount;
  return c.kind == ControlKind::Selector &&
         (lfoBase(id) == 26 || matrixSlot(id)>=0 || selectorCount(c, s) > 6);
}
} // namespace xs
