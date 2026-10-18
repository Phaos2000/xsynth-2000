// GPL-2.0-or-later. One menu builder shared by the actual editor and UI tests.
#pragma once
#include "FactoryData.h"
#include "InitState.h"
#include <string>
#include <windows.h>
namespace xs::factory {
inline HMENU createMenu(int current) {
  HMENU menu = CreatePopupMenu();
  AppendMenuW(menu,MF_STRING|(current==Init?MF_CHECKED:0),InitCommand,L"INIT");
  AppendMenuW(menu,MF_SEPARATOR,0,nullptr);
  for (const auto &bank : Banks) {
    HMENU items = CreatePopupMenu();
    for (int i = bank.first; i < bank.first + bank.count; ++i) {
      std::string name = Presets[i].name;
      for (size_t at = 0; (at = name.find('&', at)) != std::string::npos; at += 2)
        name.insert(at, 1, '&');
      AppendMenuA(items, MF_STRING | (current == i ? MF_CHECKED : 0), 1000 + i, name.c_str());
    }
    AppendMenuA(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(items), bank.name);
  }
  return menu;
}
} // namespace xs::factory
