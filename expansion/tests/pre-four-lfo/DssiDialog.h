// GPL-2.0-or-later. UI-thread-only historical bank chooser.
#pragma once
#include "DssiImport.h"
#include <windows.h>

namespace xs::dssi {
inline std::wstring wide(const std::string &s) {
  UINT page = CP_UTF8;
  DWORD flags = MB_ERR_INVALID_CHARS;
  int length = MultiByteToWideChar(page, flags, s.data(), int(s.size()), nullptr, 0);
  if (!length) {
    page = 28591;
    flags = 0;
    length = MultiByteToWideChar(page, flags, s.data(), int(s.size()), nullptr, 0);
  }
  std::wstring out(length, L'\0');
  if (length)
    MultiByteToWideChar(page, flags, s.data(), int(s.size()), out.data(), length);
  return out;
}
struct Choice {
  const Bank *bank = nullptr;
  int selected = -1;
};
inline void selection(HWND window, Choice &choice) {
  int index = int(SendDlgItemMessageW(window, 100, LB_GETCURSEL, 0, 0));
  bool valid = index >= 0 && size_t(index) < choice.bank->entries.size();
  EnableWindow(GetDlgItem(window, IDOK), valid && choice.bank->entries[index].converted);
  std::string detail;
  if (valid) {
    const auto &e = choice.bank->entries[index];
    detail = e.patch.name + " -- format " + std::to_string(e.patch.format) + ", line " +
             std::to_string(e.patch.line) + "\r\n";
    if (!e.error.empty())
      detail += e.error + "\r\n";
    for (auto &warning : e.warnings)
      detail += warning + "\r\n";
  }
  SetDlgItemTextW(window, 101, wide(detail).c_str());
}
inline INT_PTR CALLBACK dialogProc(HWND window, UINT message, WPARAM wp, LPARAM lp) {
  auto *choice = reinterpret_cast<Choice *>(GetWindowLongPtrW(window, DWLP_USER));
  if (message == WM_INITDIALOG) {
    choice = reinterpret_cast<Choice *>(lp);
    SetWindowLongPtrW(window, DWLP_USER, lp);
    auto &bank = *choice->bank;
    for (size_t i = 0; i < bank.entries.size(); ++i) {
      auto &e = bank.entries[i];
      std::string label =
          std::to_string(i + 1) + ". " + e.patch.name + (e.converted ? "" : " [unavailable]");
      SendDlgItemMessageW(window, 100, LB_ADDSTRING, 0,
                          reinterpret_cast<LPARAM>(wide(label).c_str()));
    }
    auto summary = std::to_string(bank.entries.size()) + " patches; " +
                   std::to_string(bank.converted) + " importable. Review conversion notes below.";
    SetDlgItemTextW(window, 102, wide(summary).c_str());
    SendDlgItemMessageW(window, 100, LB_SETCURSEL, 0, 0);
    selection(window, *choice);
    RECT owner{}, panel{};
    GetWindowRect(GetParent(window), &owner);
    GetWindowRect(window, &panel);
    SetWindowPos(window, nullptr,
                 std::max(0L, (owner.left + owner.right - panel.right + panel.left) / 2),
                 std::max(0L, (owner.top + owner.bottom - panel.bottom + panel.top) / 2), 0, 0,
                 SWP_NOSIZE | SWP_NOZORDER);
    return TRUE;
  }
  if (!choice)
    return FALSE;
  if (message == WM_COMMAND) {
    if (LOWORD(wp) == 100 && HIWORD(wp) == LBN_SELCHANGE) {
      selection(window, *choice);
      return TRUE;
    }
    if (LOWORD(wp) == IDCANCEL) {
      EndDialog(window, IDCANCEL);
      return TRUE;
    }
    if (LOWORD(wp) == IDOK) {
      int index = int(SendDlgItemMessageW(window, 100, LB_GETCURSEL, 0, 0));
      if (index >= 0 && size_t(index) < choice->bank->entries.size() &&
          choice->bank->entries[index].converted) {
        choice->selected = index;
        EndDialog(window, IDOK);
      }
      return TRUE;
    }
  }
  if (message == WM_CLOSE) {
    EndDialog(window, IDCANCEL);
    return TRUE;
  }
  return FALSE;
}
// An ordinary Windows dialog using Tahoma and square, beveled system controls.
// No resources added to the frozen main-panel layout or theme renderer.
inline std::vector<WORD> dialogTemplate() {
  std::vector<WORD> data;
  auto word = [&](WORD v) { data.push_back(v); };
  auto dword = [&](DWORD v) {
    word(WORD(v));
    word(WORD(v >> 16));
  };
  auto text = [&](const wchar_t *s) {
    do {
      word(WORD(*s));
    } while (*s++);
  };
  dword(WS_POPUP | WS_CAPTION | WS_SYSMENU | DS_MODALFRAME | DS_SETFONT);
  dword(0);
  word(5);
  word(0);
  word(0);
  word(360);
  word(270);
  word(0);
  word(0);
  text(L"Import Xsynth-DSSI Bank - Xsynth 2000");
  word(8);
  text(L"Tahoma");
  auto control = [&](DWORD style, short x, short y, short w, short h, WORD id, WORD type,
                     const wchar_t *label) {
    while (data.size() % 2)
      word(0);
    dword(style | WS_CHILD | WS_VISIBLE);
    dword(0);
    word(x);
    word(y);
    word(w);
    word(h);
    word(id);
    word(0xffff);
    word(type);
    text(label);
    word(0);
  };
  control(SS_LEFT, 8, 8, 344, 14, 102, 0x82, L"");
  control(WS_BORDER | WS_VSCROLL | WS_TABSTOP | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT, 8, 24, 344, 94,
          100, 0x83, L"");
  control(WS_BORDER | WS_VSCROLL | WS_TABSTOP | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL, 8, 124,
          344, 112, 101, 0x81, L"");
  control(WS_TABSTOP | BS_DEFPUSHBUTTON, 232, 246, 58, 16, IDOK, 0x80, L"Import");
  control(WS_TABSTOP | BS_PUSHBUTTON, 296, 246, 56, 16, IDCANCEL, 0x80, L"Cancel");
  return data;
}
inline int choose(HWND owner, const Bank &bank) {
  auto data = dialogTemplate();
  Choice choice{&bank};
  auto result = DialogBoxIndirectParamW(GetModuleHandleW(nullptr),
                                        reinterpret_cast<DLGTEMPLATE *>(data.data()), owner,
                                        dialogProc, reinterpret_cast<LPARAM>(&choice));
  return result == IDOK ? choice.selected : -1;
}
} // namespace xs::dssi
