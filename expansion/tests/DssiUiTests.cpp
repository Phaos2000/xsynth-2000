// GPL-2.0-or-later. Exercise the real native chooser, including modal dismissal.
#include "DssiDialog.h"
#include <iostream>
#include <stdexcept>
void check(bool b, const char *s) {
  if (!b)
    throw std::runtime_error(s);
}
HWND parentWindow{};
int action = 0, ticks = 0;
void CALLBACK tick(HWND, UINT, UINT_PTR timer, DWORD) {
  HWND panel = GetLastActivePopup(parentWindow);
  if (panel == parentWindow || !GetDlgItem(panel, 100)) {
    if (++ticks > 200)
      PostQuitMessage(1);
    return;
  }
  KillTimer(nullptr, timer);
  if (action == 1) {
    SendDlgItemMessageW(panel, 100, LB_SETCURSEL, 1, 0);
    SendMessageW(panel, WM_COMMAND, MAKEWPARAM(100, LBN_SELCHANGE), 0);
  }
  SendMessageW(panel, WM_COMMAND, action == 2 ? IDCANCEL : IDOK, 0);
}
int main() {
  try {
    xs::dssi::Bank bank;
    xs::dssi::Entry a;
    a.patch.name = "Invalid patch";
    a.error = "Unsupported pitch";
    bank.entries.push_back(a);
    a.patch.name = "Readable & escaped name";
    a.error.clear();
    a.parsed = a.converted = true;
    a.warnings = {"Approximate filter conversion."};
    bank.entries.push_back(a);
    bank.converted = bank.parsed = 1;
    parentWindow = CreateWindowW(L"STATIC", L"DSSI chooser test", WS_OVERLAPPED, 0, 0, 960, 568,
                                 nullptr, nullptr, nullptr, nullptr);
    check(parentWindow != nullptr, "Parent creation");
    auto data = xs::dssi::dialogTemplate();
    xs::dssi::Choice choice{&bank};
    HWND panel = CreateDialogIndirectParamW(
        GetModuleHandleW(nullptr), reinterpret_cast<DLGTEMPLATE *>(data.data()), parentWindow,
        xs::dssi::dialogProc, reinterpret_cast<LPARAM>(&choice));
    check(panel != nullptr, "Dialog template failed");
    check(SendDlgItemMessageW(panel, 100, LB_GETCOUNT, 0, 0) == 2, "Patch list incomplete");
    check(!IsWindowEnabled(GetDlgItem(panel, IDOK)), "Unsupported patch can import");
    SendDlgItemMessageW(panel, 100, LB_SETCURSEL, 1, 0);
    SendMessageW(panel, WM_COMMAND, MAKEWPARAM(100, LBN_SELCHANGE), 0);
    check(IsWindowEnabled(GetDlgItem(panel, IDOK)), "Valid patch disabled");
    wchar_t detail[1024]{};
    GetDlgItemTextW(panel, 101, detail, 1024);
    check(wcsstr(detail, L"Approximate filter conversion.") != nullptr, "Warnings not visible");
    DestroyWindow(panel);
    action = 1;
    check(SetTimer(nullptr, 0, 20, tick) != 0, "Timer creation");
    check(xs::dssi::choose(parentWindow, bank) == 1, "Modal import selection");
    action = 2;
    check(SetTimer(nullptr, 0, 20, tick) != 0, "Timer creation");
    check(xs::dssi::choose(parentWindow, bank) == -1, "Cancel must leave selection unset");
    DestroyWindow(parentWindow);
    check(xs::dssi::wide("Test%") == L"Test%", "Name presentation");
    std::cout << "PASS: real chooser creation, names, unavailable-entry blocking, warning display, "
                 "selected import, cancel.\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
