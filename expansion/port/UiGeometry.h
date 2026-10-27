// GPL-2.0-or-later. Header additions and explicitly approved About-only changes.
#pragma once
#include "Layout.h"
#include "SelectorGeometry.h"
#include "TitleBitmap.h"
namespace xs::ui {
inline constexpr int HeaderRight = NativeWidth - 2, HeaderGap = 8;
inline constexpr Bounds SavePatch{HeaderRight - HeaderGap - 104, 16, 104, 24};
inline constexpr Bounds LoadPatch{SavePatch.x - HeaderGap - 104, 16, 104, 24};
inline constexpr int PresetFontPixels = 11, PresetTextLeft = selector::TextLeftPadding, PresetTextRight = selector::ArrowGutterWidth+selector::TextRightPadding;
// GDI Tahoma metrics: longest title 157/208/244/320 px at 100/125/150/200%.
// 195 is the minimum whole native width allowing the 125% hinted glyph run
// plus the existing 8 px left inset and 20 px right inset/indicator region.
inline constexpr Bounds PresetNext{LoadPatch.x - HeaderGap - 24, 16, 24, 24};
inline constexpr Bounds PresetField{PresetNext.x - 4 - 195, 16, 195, 24};
inline constexpr Bounds PresetPrevious{PresetField.x - 4 - 24, 16, 24, 24};
inline constexpr Bounds ThemeButton{406, 480, 180, 24};
inline constexpr Bounds ProtectDivider{328, 424, 336, 2};
inline constexpr int PlaquePadding = 8, AboutTitleCenterY = 161;
inline constexpr int AboutEmailY = 248;
inline constexpr int ProtectNoteY[] = {428, 444, 460};
inline constexpr const wchar_t* ProtectNote[] = {
  L"PROTECT is an output limiter that controls peaks.",
  L"Adds 1.67 ms latency at 48 kHz, even when OFF.",
  L"Host delay compensation aligns playback; live playing retains this delay."
};
inline constexpr int CreditY[] = {194, 212, 230, 266, 282, 298, 316, 332, 350, 364, 378, 392, 406};
// Bounds are physical pixels: the same integer title scaling as the main logo.
inline Bounds aboutLogo(int width, double scale) {
  int step = std::max(1, int(std::floor(scale)));
  return {(width - title::Width * step) / 2,
          int(std::round(AboutTitleCenterY * scale)) - title::Height * step / 2,
          title::Width * step, title::Height * step};
}
inline Bounds plaque(int width, double scale) {
  auto b = aboutLogo(width, scale);
  int padding = int(std::round(PlaquePadding * scale));
  return {b.x - padding, b.y - padding, b.w + 2 * padding, b.h + 2 * padding};
}
} // namespace xs::ui
