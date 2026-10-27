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
  std::function<bool()> readProtect;
  std::function<bool(bool)> changeProtect;
  std::function<double()> readTempo,readSampleRate;
  int selectedFilter=0,lastEg2Cycle=0; // Editor recall only, never audio parameters.
  std::function<table::Document()> readAssets;
  std::function<uint64_t()> readAssetsRevision;
  std::function<bool(const State&,const table::Document&)> restoreDocument;
  std::function<void(int)> begin, end;
  std::function<void(int, double)> change;
  std::function<bool(const State&)> restorePatch; // Optional atomic controller/processor transaction.
  std::function<void()> dpiChanged;
  // UI recall only, not a new audio parameter or document format. The host
  // controller retains this when an editor is closed and opened again.
  std::array<int,2> lastTableSource{1,1};
  explicit Editor(const wchar_t *preferenceKey = theme::PreferenceKey);
  ~Editor();
  HWND open(HWND parent, int width = NativeWidth, int height = NativeHeight);
  void resize(int width, int height);
  bool snapshot(const wchar_t *path, int percent = 100, bool dithering = true,
                bool about = false);
  bool snapshotProgrammer(const wchar_t*path,int oscillator=0);
  void close();
  void setTheme(theme::Id id); // Preview/testing override; does not write preferences.
  theme::Id currentTheme() const;

private:
  struct Impl;
  std::unique_ptr<Impl> impl;
};
} // namespace xs
