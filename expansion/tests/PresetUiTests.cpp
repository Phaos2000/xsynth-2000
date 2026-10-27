// GPL-2.0-or-later. Hidden-window render/geometry/browser tests: no popup dialogs.
#include "Editor.h"
#include "FactoryMenu.h"
#include "PresetBrowser.h"
#include "UiGeometry.h"
#include <iostream>
#include <stdexcept>
#include <vector>
void check(bool b, const char *m) {
  if (!b)
    throw std::runtime_error(m);
}
#include "HeaderRenderCheck.h"

std::vector<uint32_t> capture(HWND w, int width, int height) {
  BITMAPINFO info{};
  info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  info.bmiHeader.biWidth = width;
  info.bmiHeader.biHeight = -height;
  info.bmiHeader.biPlanes = 1;
  info.bmiHeader.biBitCount = 32;
  HDC dc = CreateCompatibleDC(nullptr);
  void *data{};
  auto bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &data, nullptr, 0);
  auto old = SelectObject(dc, bitmap);
  SendMessageW(w, WM_PRINTCLIENT, reinterpret_cast<WPARAM>(dc), PRF_CLIENT);
  GdiFlush();
  auto *p = static_cast<uint32_t *>(data);
  std::vector<uint32_t> out(p, p + width * height);
  SelectObject(dc, old);
  DeleteObject(bitmap);
  DeleteDC(dc);
  return out;
}
void click(HWND w, int x, int y, double scale = 1) {
  SendMessageW(w, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(int(x * scale), int(y * scale)));
  SendMessageW(w, WM_LBUTTONUP, 0, MAKELPARAM(int(x * scale), int(y * scale)));
}
int main() {
  try {
    using namespace xs;
    check(ui::PresetPrevious.w == ui::PresetNext.w && ui::PresetPrevious.h == ui::PresetNext.h,
          "Arrow sizes");
    check(ui::PresetPrevious.y == ui::PresetField.y && ui::PresetField.y == ui::PresetNext.y &&
              ui::PresetField.y == 16,
          "Header alignment");
    check(ui::PresetPrevious.x >= 384 && ui::PresetNext.x + ui::PresetNext.w < ui::LoadPatch.x,
          "Header available width");
    check(ui::LoadPatch.x - ui::PresetNext.x - ui::PresetNext.w == ui::HeaderGap &&
              ui::SavePatch.x - ui::LoadPatch.x - ui::LoadPatch.w == ui::HeaderGap &&
              ui::HeaderRight - ui::SavePatch.x - ui::SavePatch.w == ui::HeaderGap,
          "Repeated right header gaps");
    check(ui::ThemeButton.x * 2 + ui::ThemeButton.w == xs::NativeWidth &&
              ui::ThemeButton.y >= ui::CreditY[12] + 16 + 16,
          "Theme separation/center");
    check(ui::ThemeButton.y + ui::ThemeButton.h == 504, "Theme bottom margin");
    check(ui::CreditY[2]+16<=ui::AboutEmailY && ui::AboutEmailY+16<=ui::CreditY[3],
          "Website/email/year rows overlap");
    HMENU menu = factory::createMenu(12);
    check(GetMenuItemCount(menu) == 5, "Menu grouping");
    for (int bank = 0; bank < 3; ++bank) {
      char label[128]{};
      GetMenuStringA(menu, bank+2, label, 128, MF_BYPOSITION);
      check(std::string(label) == factory::Banks[bank].name, "Bank label");
      auto child = GetSubMenu(menu, bank+2);
      check(GetMenuItemCount(child) == factory::Banks[bank].count, "Grouped count");
      for (int j = 0; j < factory::Banks[bank].count; ++j) {
        int id = factory::Banks[bank].first + j;
        check(GetMenuItemID(child, j) == 1000 + id, "Menu order/identity");
        GetMenuStringA(child, j, label, 128, MF_BYPOSITION);
        check(std::string(label) == factory::Presets[id].name, "Menu historical name");
      }
    }
    DestroyMenu(menu);
    HWND parent = CreateWindowW(L"STATIC", L"Factory UI test", WS_POPUP, 0, 0, 1920, 1136, nullptr,
                                nullptr, nullptr, nullptr);
    check(parent != nullptr, "Parent");
    State state = defaults();
    int edits = 0;
    Editor editor(nullptr);
    editor.read = [&] { return state; };
    editor.begin = editor.end = [](int) {};
    editor.change = [&](int id, double n) {
      state[id] = n;
      ++edits;
    };
    HWND child = editor.open(parent);
    capture(child, xs::NativeWidth, 568);
    click(child,ui::PresetNext.x+12,28);
    check(factory::sameSound(state,factory::initState()),"INIT before first factory");
    for (int index = 0; index < factory::Count; ++index) {
      click(child, ui::PresetNext.x + 12, 28);
      check(factory::sameSound(state, factory::presetState(index)), "Next preset state");
    }
    click(child, ui::PresetNext.x + 12, 28);
    check(factory::sameSound(state, factory::initState()), "Bank wrap");
    click(child, ui::PresetPrevious.x + 12, 28);
    check(factory::sameSound(state, factory::presetState(factory::Count - 1)),
          "Previous bank wrap");
    for (int i = 49; i > 36; --i)
      click(child, ui::PresetPrevious.x + 12, 28);
    check(factory::sameSound(state, factory::presetState(36)),
          "Longest-title production selection");
    int minimumX = title::Width, minimumY = title::Height, maximumX = 0, maximumY = 0;
    for (int y = 0; y < title::Height; ++y)
      for (int x = 0; x < title::Width; ++x)
        if ((title::Pixels[y * title::Width + x] >> 24) ||
            (x && y && (title::Pixels[(y - 1) * title::Width + x - 1] >> 24))) {
          minimumX = std::min(minimumX, x);
          minimumY = std::min(minimumY, y);
          maximumX = std::max(maximumX, x);
          maximumY = std::max(maximumY, y);
        }
    check(minimumX == 0 && minimumY == 0 && maximumX == title::Width - 1 &&
              maximumY == title::Height - 1,
          "Actual logo bounds differ from geometry");
    for (int percent : {100, 125, 150, 200}) {
      double scale = percent / 100.;
      int width = xs::NativeWidth * percent / 100, height = 568 * percent / 100;
      editor.resize(width, height);
      HDC metrics = CreateCompatibleDC(nullptr);
      auto measuredFont =
          CreateFontW(-int(std::round(ui::PresetFontPixels * scale)), 0, 0, 0, FW_NORMAL, FALSE,
                      FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                      NONANTIALIASED_QUALITY, DEFAULT_PITCH, L"Tahoma");
      auto previousFont = SelectObject(metrics, measuredFont);
      SIZE extent{};
      const char *longest = "Long Portamento (turn mono on)";
      GetTextExtentPoint32A(metrics, longest, int(std::strlen(longest)), &extent);
      int left = int(std::round((ui::PresetField.x + ui::PresetTextLeft) * scale)),
          right = int(
              std::round((ui::PresetField.x + ui::PresetField.w - ui::PresetTextRight) * scale));
      check(extent.cx <= right - left, "Longest factory name clips at production font/scale");
      std::cout << "Header font " << ui::PresetFontPixels << " px scale " << percent
                << "%: title=" << extent.cx << " px, available=" << right - left << " px\n";
      SelectObject(metrics, previousFont);
      DeleteObject(measuredFont);
      DeleteDC(metrics);
      int step = std::max(1, int(std::floor(scale))),
          pad = int(std::round(ui::PlaquePadding * scale));
      auto plaque = ui::plaque(width, scale), art = ui::aboutLogo(width, scale);
      check(art.x - plaque.x == pad && art.y - plaque.y == pad &&
                plaque.x + plaque.w - art.x - art.w == pad &&
                plaque.y + plaque.h - art.y - art.h == pad,
            "Unequal title padding");
      check(2 * plaque.x + plaque.w == width, "Plaque not centered");
      check(plaque.y + plaque.h < int(std::round(ui::CreditY[0] * scale)),
            "Plaque overlaps credits");
      for (unsigned theme = 0; theme < theme::Count; ++theme) {
        editor.setTheme(theme::Id(theme));
        auto closed = capture(child, width, height);
        // Compare the production title glyph mask against untruncated GDI text.
        // An ellipsis, clipped letter, wrong font, or wrong baseline changes it.
        verifyHeaderText(closed, width, height, scale, theme::Id(theme),
                         "Long Portamento (turn mono on)");

        click(child, 60, 24, scale);
        auto opened = capture(child, width, height);
        int mainLeft = std::max(0, (int(std::round(48 * scale)) - title::Height * step) / 2);
        for (int y = 0; y < title::Height; ++y)
          for (int x = 0; x < title::Width; ++x) {
            bool ink = title::Pixels[y * title::Width + x] >> 24;
            bool shadow = x && y && (title::Pixels[(y - 1) * title::Width + x - 1] >> 24);
            if (ink || shadow)
              for (int dy = 0; dy < step; ++dy)
                for (int dx = 0; dx < step; ++dx)
                  check(closed[(mainLeft + y * step + dy) * width + mainLeft + x * step + dx] ==
                            opened[(art.y + y * step + dy) * width + art.x + x * step + dx],
                        "About logo differs from main rendering");
          }
        // Fixed-size Tahoma button text fits even the longest material name.
        HDC dc = CreateCompatibleDC(nullptr);
        auto font = CreateFontW(-int(std::round(10 * scale)), 0, 0, 0, FW_NORMAL, FALSE, FALSE,
                                FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                NONANTIALIASED_QUALITY, DEFAULT_PITCH, L"Tahoma");
        auto old = SelectObject(dc, font);
        std::string text = std::string("THEME: ") + theme::Names[theme];
        SIZE size{};
        GetTextExtentPoint32A(dc, text.c_str(), int(text.size()), &size);
        check(size.cx < int(std::round((ui::ThemeButton.w - 12) * scale)), "Theme text clipping");
        SelectObject(dc, old);
        DeleteObject(font);
        DeleteDC(dc);
        auto before = state;
        int beforeEdits = edits;
        click(child, xs::ui::ThemeButton.x+xs::ui::ThemeButton.w/2, xs::ui::ThemeButton.y+xs::ui::ThemeButton.h/2, scale);
        check(unsigned(editor.currentTheme()) == (theme + 1) % theme::Count && state == before &&
                  edits == beforeEdits,
              "Theme button changes sound or failed cycle");
        click(child,60,24,scale); // Theme must leave About open.

      }
    }
    editor.close();
    DestroyWindow(parent);
    std::cout << "PASS: actual grouped menu/names, all 50 next selections and wrap; fixed header "
                 "geometry; equal-padding plaque; main/About logo pixel equality; six themes at "
                 "100/125/150/200%; button width/placement/cycling and patch isolation\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
