// GPL-2.0-or-later. Compare actual production selector glyphs against untruncated text.
#pragma once
#include "UiGeometry.h"
#include "DssiDialog.h"
inline void verifyHeaderText(const std::vector<uint32_t> &closed, int width, int height,
                             double scale, xs::theme::Id material, const char *label) {
  BITMAPINFO headerInfo{};
  headerInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  headerInfo.bmiHeader.biWidth = width;
  headerInfo.bmiHeader.biHeight = -height;
  headerInfo.bmiHeader.biPlanes = 1;
  headerInfo.bmiHeader.biBitCount = 32;
  HDC expected = CreateCompatibleDC(nullptr);
  void *expectedBits{};
  auto expectedBitmap =
      CreateDIBSection(expected, &headerInfo, DIB_RGB_COLORS, &expectedBits, nullptr, 0);
  auto oldBitmap = SelectObject(expected, expectedBitmap);
  std::memset(expectedBits, 0, size_t(width) * height * 4);
  auto expectedFont =
      CreateFontW(-int(std::round(xs::ui::PresetFontPixels * scale)), 0, 0, 0, FW_NORMAL, FALSE,
                  FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                  NONANTIALIASED_QUALITY, DEFAULT_PITCH, L"Tahoma");
  auto oldFont = SelectObject(expected, expectedFont);
  SetTextColor(expected, RGB(255, 255, 255));
  SetBkMode(expected, TRANSPARENT);
  RECT textRect{
      int(std::round((xs::ui::PresetField.x + xs::ui::PresetTextLeft) * scale)),
      int(std::round((xs::ui::PresetField.y + 3) * scale)),
      int(std::round((xs::ui::PresetField.x + xs::ui::PresetField.w - xs::ui::PresetTextRight) *
                     scale)),
      int(std::round((xs::ui::PresetField.y + xs::ui::PresetField.h - 3) * scale))};
  auto wideLabel=xs::dssi::wide(label);
  DrawTextW(expected, wideLabel.c_str(), -1, &textRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
  GdiFlush();
  auto *glyphs = static_cast<uint32_t *>(expectedBits);
  auto ink = xs::theme::style(material).readoutText;
  for (int y = textRect.top; y < textRect.bottom; ++y)
    for (int x = textRect.left; x < textRect.right; ++x)
      check(((glyphs[y * width + x] & 0xffffff) == 0xffffff) ==
                ((closed[y * width + x] & 0xffffff) == ink),
            "Production longest title glyphs clipped, ellipsized or misaligned");
  SelectObject(expected, oldFont);
  DeleteObject(expectedFont);
  SelectObject(expected, oldBitmap);
  DeleteObject(expectedBitmap);
  DeleteDC(expected);
}
