// GPL-2.0-or-later. Explicit import choices; no automatic segmentation/repair.
#pragma once
#include "WaveImport.h"
#include "Theme.h"
#include <windows.h>
namespace xs {
struct WaveImportChoices {
  int length=256,channel=2;bool dc=false,normalize=false,accepted=false,single=false,finished=false;
  HWND window{},cycle{},channels{},remove{},gain{};HFONT font{};
  table::WaveData info;
  static LRESULT CALLBACK proc(HWND w,UINT m,WPARAM wp,LPARAM lp){
    auto*self=reinterpret_cast<WaveImportChoices*>(GetWindowLongPtrW(w,GWLP_USERDATA));
    if(m==WM_NCCREATE){self=static_cast<WaveImportChoices*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);self->window=w;SetWindowLongPtrW(w,GWLP_USERDATA,LONG_PTR(self));}
    if(!self)return DefWindowProcW(w,m,wp,lp);
    if(m==WM_CLOSE){self->finished=true;ShowWindow(w,SW_HIDE);return 0;}
    if(m==WM_COMMAND){
      int id=LOWORD(wp);
      if(id==1){wchar_t text[32]{};GetWindowTextW(self->cycle,text,32);self->length=self->single?int(self->info.mono.size()):_wtoi(text);
        if(self->length<16||self->length>4096||self->info.mono.size()%self->length||self->info.mono.size()/self->length>256){MessageBoxW(w,L"Choose 16 to 4096 samples per frame, with 1 to 256 complete frames. A single cycle uses the entire file.",L"IMPORT",MB_ICONERROR);return 0;}
        self->channel=int(SendMessageW(self->channels,CB_GETCURSEL,0,0));
        if(self->channel==2&&self->info.cancellation&&MessageBoxW(w,L"Averaging substantially cancels this stereo file. Keep that intentional cancellation? Choose No to select LEFT/RIGHT.",L"IMPORT",MB_YESNO|MB_DEFBUTTON2|MB_ICONWARNING)!=IDYES)return 0;
        self->dc=SendMessageW(self->remove,BM_GETCHECK,0,0)==BST_CHECKED;self->normalize=SendMessageW(self->gain,BM_GETCHECK,0,0)==BST_CHECKED;
        self->accepted=self->finished=true;ShowWindow(w,SW_HIDE);return 0;
      }
      if(id==2){self->finished=true;ShowWindow(w,SW_HIDE);return 0;}
    }
    return DefWindowProcW(w,m,wp,lp);
  }
  bool show(HWND parent,HINSTANCE instance){
    WNDCLASSW cls{};cls.hInstance=instance;cls.lpfnWndProc=proc;cls.lpszClassName=L"Xsynth2000ImportOptions";cls.hbrBackground=GetSysColorBrush(COLOR_BTNFACE);cls.hCursor=LoadCursor(nullptr,IDC_ARROW);RegisterClassW(&cls);
    int dpi=int(GetDpiForWindow(parent));if(dpi<96)dpi=96;auto z=[&](int x){return MulDiv(x,dpi,96);};
    RECT r{0,0,z(456),z(228)};AdjustWindowRectEx(&r,WS_CAPTION|WS_SYSMENU,FALSE,WS_EX_DLGMODALFRAME);
    window=CreateWindowExW(WS_EX_DLGMODALFRAME,cls.lpszClassName,L"IMPORT USER WAVE",WS_CAPTION|WS_SYSMENU,120,120,r.right-r.left,r.bottom-r.top,parent,nullptr,instance,this);
    font=CreateFontW(-z(11),0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,NONANTIALIASED_QUALITY,0,L"Tahoma");
    auto child=[&](const wchar_t*kind,const wchar_t*label,int id,int x,int y,int width,int height,DWORD style=0){HWND c=CreateWindowExW(0,kind,label,WS_CHILD|WS_VISIBLE|style|(!std::wcscmp(kind,L"STATIC")?0:WS_TABSTOP),z(x),z(y),z(width),z(height),window,HMENU(INT_PTR(id)),instance,nullptr);SendMessageW(c,WM_SETFONT,WPARAM(font),TRUE);return c;};
    wchar_t text[256];swprintf_s(text,L"%u Hz (metadata only) | %d channels | %zu mono samples\r\nWhole file selected. No sample-rate conversion or seam repair.",info.rate,info.channels,info.mono.size());child(L"STATIC",text,0,12,12,432,36);
    child(L"STATIC",L"SAMPLES / FRAME",0,12,60,180,16);std::wstring n=std::to_wstring(single?info.mono.size():info.suggestedLength?info.suggestedLength:256);
    cycle=child(L"EDIT",n.c_str(),0,234,58,210,20,WS_BORDER|ES_NUMBER);EnableWindow(cycle,!single);
    child(L"STATIC",L"MONO CHANNEL POLICY",0,12,92,210,16);channels=child(L"COMBOBOX",L"",0,234,90,210,100,CBS_DROPDOWNLIST);
    for(auto s:{L"LEFT",L"RIGHT",L"AVERAGE"})SendMessageW(channels,CB_ADDSTRING,0,LPARAM(s));SendMessageW(channels,CB_SETCURSEL,2,0);
    remove=child(L"BUTTON",L"REMOVE DC (PER FRAME)",0,12,128,210,18,BS_AUTOCHECKBOX);gain=child(L"BUTTON",L"GLOBAL PEAK NORMALIZE",0,234,128,210,18,BS_AUTOCHECKBOX);
    child(L"STATIC",L"Defaults preserve samples. Peaks above 1 require normalization.\r\nThe frame length must divide the selected sample count exactly.",0,12,158,432,32);
    child(L"BUTTON",L"IMPORT",1,12,198,210,18);child(L"BUTTON",L"CANCEL",2,234,198,210,18);
    EnableWindow(parent,FALSE);if(IsWindowVisible(parent))ShowWindow(window,SW_SHOW);MSG message{};
    while(!finished){int result=GetMessageW(&message,nullptr,0,0);if(result<=0){if(result==0)PostQuitMessage(int(message.wParam));break;}if(!IsDialogMessageW(window,&message)){TranslateMessage(&message);DispatchMessageW(&message);}}
    EnableWindow(parent,TRUE);SetActiveWindow(parent);DestroyWindow(window);DeleteObject(font);UnregisterClassW(cls.lpszClassName,instance);return accepted;
  }
};
}
