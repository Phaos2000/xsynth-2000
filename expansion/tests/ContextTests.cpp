// GPL-2.0-or-later. Real editor event tests; OS menus/pickers intercepted in test process.
#include "Editor.h"
#include "ContextRegions.h"
#include "UiGeometry.h"
#include <commdlg.h>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>
void check(bool b, const char *m) {
  if (!b)
    throw std::runtime_error(m);
}
int popups = 0, command = 0, expectedSection = -1;
bool inject = false;
HWND editorWindow{};
void right(HWND w, int x, int y) { SendMessageW(w, WM_RBUTTONUP, 0, MAKELPARAM(x, y)); }
void probeModal() {
  int before = popups;
  for (auto p : {POINT{50, 90}, POINT{350, 90}, POINT{280, 245}, POINT{540, 245}})
    right(editorWindow, p.x, p.y);
  check(popups == before, "Popup/modal reentered underlying section menu");
}
BOOL WINAPI popup(HMENU menu, UINT flags, int, int, int, HWND, const RECT *) {
  ++popups;
  if (inject) {
    inject = false;
    probeModal();
    return 0;
  }
  check((flags & TPM_RIGHTBUTTON) != 0 && expectedSection >= 0, "Wrong popup route");
  int count = GetMenuItemCount(menu);
  check(count == (expectedSection < 2 ? 8 : 7), "Wrong section menu model");
  wchar_t label[128]{};
  GetMenuStringW(menu, 2, label, 128, MF_BYPOSITION);
  check(std::wstring(label) ==
            (expectedSection < 2 ? L"Paste to other oscillator" : L"Paste to other envelope"),
        "Wrong Copy/Paste kind");
  auto mutate = GetSubMenu(menu, count - 1);
  check(mutate && GetMenuItemCount(mutate) == 4, "Mutate actions lost");
  for (int i = 0; i < 4; ++i)
    check(GetMenuItemID(mutate, i) == 10 + i, "Mutate IDs changed");
  return command;
}
SHORT WINAPI idleMouse(int){return 0;}
BOOL WINAPI picker(LPOPENFILENAMEW) {
  probeModal();
  return FALSE;
}
INT_PTR WINAPI chooser(HINSTANCE, LPCDLGTEMPLATE, HWND, DLGPROC, LPARAM) {
  probeModal();
  return IDCANCEL;
}
// Patch only this executable's IAT, not any product binary on disk.
struct Imports {
  struct Entry {
    ULONG_PTR *slot;
    ULONG_PTR old;
  };
  std::vector<Entry> entries;
  Imports() {
    auto *base = reinterpret_cast<BYTE *>(GetModuleHandleW(nullptr));
    auto *dos = reinterpret_cast<IMAGE_DOS_HEADER *>(base);
    auto *nt = reinterpret_cast<IMAGE_NT_HEADERS *>(base + dos->e_lfanew);
    auto *imports = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR *>(
        base + nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);
    for (; imports->Name; ++imports) {
      if (!imports->OriginalFirstThunk)
        continue;
      auto *names = reinterpret_cast<IMAGE_THUNK_DATA *>(base + imports->OriginalFirstThunk);
      auto *slots = reinterpret_cast<IMAGE_THUNK_DATA *>(base + imports->FirstThunk);
      for (; names->u1.AddressOfData; ++names, ++slots) {
        if (IMAGE_SNAP_BY_ORDINAL(names->u1.Ordinal))
          continue;
        auto *name = reinterpret_cast<IMAGE_IMPORT_BY_NAME *>(base + names->u1.AddressOfData);
        ULONG_PTR target = 0;
        if (!strcmp((char *)name->Name, "TrackPopupMenu"))
          target = reinterpret_cast<ULONG_PTR>(popup);
        else if (!strcmp((char *)name->Name, "GetOpenFileNameW") ||
                 !strcmp((char *)name->Name, "GetSaveFileNameW"))
          target = reinterpret_cast<ULONG_PTR>(picker);
        else if (!strcmp((char *)name->Name, "DialogBoxIndirectParamW"))
          target = reinterpret_cast<ULONG_PTR>(chooser);
        if(!strcmp((char*)name->Name,"GetAsyncKeyState"))target=reinterpret_cast<ULONG_PTR>(idleMouse);
        if (target) {
          DWORD old{}, ignored{};
          auto *slot = &slots->u1.Function;
          check(VirtualProtect(slot, sizeof(*slot), PAGE_READWRITE, &old), "Fixture IAT access");
          entries.push_back({slot, *slot});
          *slot = target;
          VirtualProtect(slot, sizeof(*slot), old, &ignored);
        }
      }
    }
    check(entries.size() == 5, "Fixture imports missing");
  }
  ~Imports() {
    for (auto &e : entries) {
      DWORD old{}, ignored{};
      VirtualProtect(e.slot, sizeof(*e.slot), PAGE_READWRITE, &old);
      *e.slot = e.old;
      VirtualProtect(e.slot, sizeof(*e.slot), old, &ignored);
    }
  }
};
void click(HWND w, int x, int y) {
  SendMessageW(w, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(x, y));
  SendMessageW(w, WM_LBUTTONUP, 0, MAKELPARAM(x, y));
}
int main(int argc, char **argv) {
  try {
    using namespace xs;
    Imports imports;
    std::ofstream table(argc > 1 ? argv[1] : "context-regions.csv");
    table << "region,x,y,width,height,layout,menu,overlap\n";
    constexpr int menus[] = {0, -1, 1, -1, -1, -1, -1, 2, 3, -1, -1};
    constexpr const char *names[] = {"VCO1", "XMOD", "VCO2", "MIXER", "VCF",   "AMP",
                                     "LFO",  "EG1",  "EG2",  "VOICE", "MATRIX"};
    auto parent = CreateWindowW(L"STATIC", L"Context audit", WS_POPUP, 0, 0, 1920, 1136, nullptr,
                                nullptr, nullptr, nullptr);
    check(parent != nullptr, "Hidden host");
    State state = defaults();
    int edits = 0;
    Editor editor(nullptr);
    editor.read = [&] { return state; };
    editor.change = [&](int id, double n) {
      state[id] = n;
      ++edits;
    };
    editor.begin = editor.end = [](int) {};
    editorWindow = editor.open(parent);
    long long sampled = 0, eventSamples = 0;
    for (int percent : {100, 125, 150, 200}) {
      double scale = percent / 100.;
      int width = NativeWidth * percent / 100, height = NativeHeight * percent / 100;
      editor.resize(width, height);
      std::vector<int> paint(size_t(width) * height, -1);
      for (int i = 0; i < int(std::size(panels)); ++i) {
        const auto &p = panels[i];
        auto shell = faceGrid::shell(p);
        check(shell.x >= p.x && shell.y >= p.y && shell.x + shell.w <= p.x + p.w &&
                  shell.y + shell.h <= p.y + p.h,
              "Containment");
        auto b = context::pixels(p, scale);
        check(b.x >= 0 && b.y >= int(std::round(faceGrid::HeaderHeight * scale)) &&
                  b.x + b.w <= width && b.y + b.h <= height,
              "Region spills client/header");
        if (percent == 100)
          table << names[i] << ',' << b.x << ',' << b.y << ',' << b.w << ',' << b.h
                << ",faceGrid::shell(panels[" << i << "])," << (menus[i] < 0 ? "NONE" : names[i])
                << ",none\n";
        for (int y = b.y; y < b.y + b.h; ++y)
          for (int x = b.x; x < b.x + b.w; ++x) {
            check(paint[y * width + x] == -1, "Positive area or shared-edge ownership overlap");
            paint[y * width + x] = i;
          }
        for (auto point :
             {POINT{b.x, b.y}, POINT{b.x + b.w - 1, b.y}, POINT{b.x, b.y + b.h - 1},
              POINT{b.x + b.w - 1, b.y + b.h - 1}, POINT{b.x + b.w / 2, b.y + b.h / 2}}) {
          check(context::panelAt(point.x, point.y, width, height) == i,
                "Wrong corner/center owner");
        }
      }
      auto dispatch = [&](int x, int y) {
        int owner = (x < 0 || y < 0 || x >= width || y >= height) ? -1 : paint[y * width + x];
        expectedSection = owner < 0 ? -1 : menus[owner];
        int before = popups;
        right(editorWindow, x, y);
        check(popups - before == (expectedSection >= 0 ? 1 : 0),
              "Production right-click dispatch mismatch");
        ++eventSamples;
      };
      for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x) {
          int owner = paint[y * width + x];
          check(context::panelAt(x, y, width, height) == owner, "Full pixel owner mismatch");
          check(context::sectionAt(x, y, width, height) == (owner < 0 ? -1 : menus[owner]),
                "Full pixel section mismatch");
          ++sampled;
          if (y < int(std::round(48 * scale)) || (x % 8 == 0 && y % 8 == 0))
            dispatch(x, y);
        }
      for (const auto &p : panels) {
        auto b = context::pixels(p, scale);
        for (int x = b.x - 1; x <= b.x + b.w; ++x) {
          dispatch(x, b.y - 1);
          dispatch(x, b.y);
          dispatch(x, b.y + b.h - 1);
          dispatch(x, b.y + b.h);
        }
        for (int y = b.y; y < b.y + b.h; ++y) {
          dispatch(b.x - 1, y);
          dispatch(b.x, y);
          dispatch(b.x + b.w - 1, y);
          dispatch(b.x + b.w, y);
        }
      }
      for (auto &c : mainControls) {
        dispatch(int(std::round((c.x + 15) * scale)), int(std::round((c.y + 48 + 10) * scale)));
        auto b = fieldBounds(c);
        dispatch(int(std::round((b.x + b.w / 2) * scale)),
                 int(std::round((b.y + 48 + b.h / 2) * scale)));
      }
      for (auto &c : matrixControls())
        dispatch(int(std::round((c.x + c.w / 2) * scale)), int(std::round((c.y + 48 + 9) * scale)));
      dispatch(-1, -1);
      dispatch(width, height);
      dispatch(-1, height / 2);
      dispatch(width, 0);
      // Each active menu followed by every header pixel: owner must never persist.
      for (int i : {0, 2, 7, 8}) {
        auto b = context::pixels(panels[i], scale);
        dispatch(b.x + b.w / 2, b.y + 12);
        for (int x = 0; x < width; ++x)
          dispatch(x, int(std::round(24 * scale)));
      }
      click(editorWindow, int(60 * scale), int(24 * scale));
      int before = popups;
      auto original = state;
      for (int y = 0; y < height; y += 7)
        for (int x = 0; x < width; x += 7)
          right(editorWindow, x, y);
      check(popups == before && state == original, "About leaked context or edited state");
      click(editorWindow, int(60 * scale), int(24 * scale));
      std::cout << "PASS: " << percent
                << "% full pixel ownership, header/gutters/edges/controls, stale-owner and modal "
                   "About routing\n";
    }
    table << "GLOBAL HEADER,0,0,960,48,header,NO MODULE CONTEXT,none\n";
    editor.resize(960, 568);
    // THEME must never dismiss About, across an entire six-theme cycle.
    click(editorWindow,60,24);int themeMenus=popups;auto themeState=state;
    for(int i=0;i<6;++i){click(editorWindow,480,492);probeModal();check(popups==themeMenus,"THEME dismissed About");}
    check(state==themeState,"THEME changed synth state");click(editorWindow,60,24);
    check(edits == 0, "Cancelled context menus changed parameters");
    // Existing actions identify oscillator/envelope ownership using actual state edits.
    constexpr int ids[4][9] = {{0, 1, 24, Engine1, -1, -1, -1, -1, -1},
                               {2, 3, 25, Engine2, -1, -1, -1, -1, -1},
                               {9, 10, 11, 12, Curve1, Velocity1, 13, 14, -1},
                               {15, 16, 17, 18, Curve2, Velocity2, 19, 20, Loop2}};
    for (int section = 0; section < 4; ++section) {
      int panel = section == 0 ? 0 : section == 1 ? 2 : section == 2 ? 7 : 8;
      auto b = context::pixels(panels[panel], 1);
      expectedSection = section;
      state.fill(.317);
      auto before = state;
      command = 5;
      right(editorWindow, b.x + 20, b.y + 20);
      for (int id = 0; id < Count; ++id) {
        bool included = false;
        for (int p : ids[section])
          if (p == id)
            included = true;
        check(state[id] == (included ? defaults()[id] : before[id]),
              "Reset selected wrong module parameters");
      }
      state = defaults();
      for (int id : ids[section])
        if (id >= 0)
          state[id] = .41;
      command = 1;
      right(editorWindow, b.x + 20, b.y + 20);
      state = defaults();
      command = 2;
      right(editorWindow, b.x + 20, b.y + 20);
      for (int id : ids[section])
        if (id >= 0)
          check(state[id] == stepped(id, .41), "Copy/Paste semantics changed");
      command = 3;
      right(editorWindow, b.x + 20, b.y + 20);
      int other = section ^ 1, count = other < 2 ? 4 : other == 2 ? 8 : 9;
      for (int i = 0; i < count; ++i)
        check(state[ids[other][i]] ==
                  stepped(ids[other][i], ids[section][i] < 0 ? defaults()[ids[other][i]] : .41),
              "Paste-to-other semantics changed");
    }
    command = 0;
    expectedSection = 1;
    inject = true;
    right(editorWindow, 350, 90); // Reentrant menu messages suppressed.
    inject = true;
    click(editorWindow, 550, 28); // Factory popup.
    inject = true;
    click(editorWindow, 70, 459); // Existing long oscillator Wave popup.
    inject = true;
    click(editorWindow, 770, 28); // Load menu.
    click(editorWindow, 880, 28); // Native save picker (returns Cancel).
    check(!inject, "Popup fixture did not execute");
    // Disabled editor and active knob drag must not open section menus either.
    int before = popups;
    EnableWindow(editorWindow, FALSE);
    right(editorWindow, 350, 90);
    EnableWindow(editorWindow, TRUE);
    check(popups == before, "Disabled window leaked context");
    SendMessageW(editorWindow,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(46,93));
    right(editorWindow,350,90);check(popups==before,"Knob drag leaked context menu");
    SendMessageW(editorWindow,WM_LBUTTONUP,0,MAKELPARAM(46,93));
    click(editorWindow, ui::PresetNext.x + 12, 28);
    before = popups;
    right(editorWindow, 550, 28);
    right(editorWindow, 770, 28);
    right(editorWindow, 880, 28);
    check(popups == before, "Preset/header leak after navigation");
    editor.close();
    DestroyWindow(parent);
    std::cout << "PASS: " << sampled << " pixels, " << eventSamples
              << " actual right-click events; no overlap/outside ownership; real menu models and "
                 "Reset/Copy/Paste/other actions; popup/disabled suppression\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "FAIL: " << e.what() << '\n';
    return 1;
  }
}
