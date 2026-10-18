// GPL-2.0-or-later
#pragma once
#include "Core.h"
#include "ThemePreference.h"
#include <functional>
#include <memory>
#include <windows.h>
namespace xs {
class Editor {
public:
  std::function<State()> read;
  std::function<void(int)> begin, end;
  std::function<void(int, double)> change;
  std::function<bool(const State&)> restorePatch; // Optional atomic controller/processor transaction.
  std::function<void()> dpiChanged;
  explicit Editor(const wchar_t *preferenceKey = theme::PreferenceKey);
  ~Editor();
  HWND open(HWND parent, int width = NativeWidth, int height = NativeHeight);
  void resize(int width, int height);
  bool snapshot(const wchar_t *path, int percent = 100, bool dithering = true,
                bool about = false);
  void close();
  void setTheme(theme::Id id); // Preview/testing override; does not write preferences.
  theme::Id currentTheme() const;

private:
  struct Impl;
  std::unique_ptr<Impl> impl;
};
} // namespace xs
