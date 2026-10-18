// GPL-2.0-or-later. Context ownership derives from the exact drawn module shells.
#pragma once
#include "Layout.h"
namespace xs::context {
inline Bounds pixels(const Panel &panel, double scale) {
  const auto b = faceGrid::shell(panel);
  int left = int(std::round(b.x * scale)),
      top = int(std::round((b.y + faceGrid::HeaderHeight) * scale));
  return {left, top, int(std::round((b.x + b.w) * scale)) - left,
          int(std::round((b.y + b.h + faceGrid::HeaderHeight) * scale)) - top};
}
inline bool contains(Bounds b, int x, int y) {
  return x >= b.x && y >= b.y && x < b.x + b.w && y < b.y + b.h;
}
// Input is signed client pixels, never truncated native/body coordinates.
// Half-open rectangles assign each bevel pixel once and leave gutters unowned.
inline int panelAt(int x, int y, int width, int height) {
  if (width <= 0 || height <= 0 || x < 0 || y < 0 || x >= width || y >= height)
    return -1;
  double scale = width / double(NativeWidth);
  for (int i = 0; i < int(std::size(panels)); ++i)
    if (contains(pixels(panels[i], scale), x, y))
      return i;
  return -1;
}
inline constexpr int menuForPanel(int panel) {
  return panel == 0 ? 0 : panel == 2 ? 1 : panel == 7 ? 2 : panel == 8 ? 3 : -1;
}
inline int sectionAt(int x, int y, int width, int height) {
  return menuForPanel(panelAt(x, y, width, height));
}
// Nested popup/file/chooser calls restore their previous suppression state.
struct ModalScope {
  bool &flag;
  bool prior;
  explicit ModalScope(bool &value) : flag(value), prior(value) { flag = true; }
  ~ModalScope() { flag = prior; }
};
} // namespace xs::context
