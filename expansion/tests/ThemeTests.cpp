// GPL-2.0-or-later. Theme rendering, UI-only persistence and input isolation.
#include "Editor.h"
#include "Layout.h"
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
void check(bool ok,const char *message){if(!ok)throw std::runtime_error(message);}
std::vector<uint32_t> capture(HWND window) {
 BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
 info.bmiHeader.biWidth=xs::NativeWidth;info.bmiHeader.biHeight=-xs::NativeHeight;
 info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;
 HDC dc=CreateCompatibleDC(nullptr);void *data{};
 auto image=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&data,nullptr,0);
 check(dc && image && data,"Capture failed");auto old=SelectObject(dc,image);
 SendMessageW(window,WM_PRINTCLIENT,reinterpret_cast<WPARAM>(dc),PRF_CLIENT);GdiFlush();
 auto start=static_cast<uint32_t*>(data);
 std::vector<uint32_t> result(start,start+xs::NativeWidth*xs::NativeHeight);
 SelectObject(dc,old);DeleteObject(image);DeleteDC(dc);return result;
}
int main(){
 struct PreferenceSandbox {
  std::wstring key=L"Software\\Phaos\\XsynthExpansionDev\\ThemeTest-"+
    std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount64());
  ~PreferenceSandbox(){RegDeleteKeyW(HKEY_CURRENT_USER,key.c_str());}
 } sandbox;
 try {
  check(xs::theme::load(sandbox.key.c_str())==xs::theme::Id::Steel,"Missing preference did not default to STEEL");
  HKEY probe{};auto access=RegCreateKeyExW(HKEY_CURRENT_USER,sandbox.key.c_str(),0,nullptr,0,KEY_SET_VALUE,nullptr,&probe,nullptr);
  if(access!=ERROR_SUCCESS)throw std::runtime_error("Isolated theme preference registry access failed: "+std::to_string(access));
  RegCloseKey(probe);
  check(xs::theme::save(sandbox.key.c_str(),xs::theme::Id::Steel),"Cannot write isolated theme preference");
  auto parent=CreateWindowW(L"STATIC",L"Theme test",WS_OVERLAPPED,0,0,1000,650,nullptr,nullptr,nullptr,nullptr);
  check(parent!=nullptr,"Parent creation failed");
  xs::State state=xs::defaults();int edits=0,gestures=0;
  const auto patchBefore=xs::encode(state);
  xs::Editor a(sandbox.key.c_str());
  a.read=[&]{return state;};a.change=[&](int id,double value){++edits;state[id]=value;};
  a.begin=a.end=[&](int){++gestures;};
  auto window=a.open(parent);check(window!=nullptr,"Editor creation failed");
  auto click=[&](int x,int y){SendMessageW(window,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(x,y));SendMessageW(window,WM_LBUTTONUP,0,MAKELPARAM(x,y));};
  const auto steel=capture(window);
  std::vector<std::vector<uint32_t>> images{steel};
  for(unsigned n=1;n<=xs::theme::Count;++n){
   click(60,24); // Logo opens About; theme only exists in this modal.
   click(480,492);
   auto expected=xs::theme::Id(n%xs::theme::Count);
   check(a.currentTheme()==expected,"About selector did not select next theme");
   check(xs::theme::load(sandbox.key.c_str())==expected,"Theme preference was not persisted");
   click(60,24); // Theme action keeps About open.
   auto image=capture(window);
   check(image==capture(window),"Theme rendering is not deterministic");
   if(n<xs::theme::Count){for(const auto &prior:images)check(prior!=image,"Themes render identically");images.push_back(image);}
   else check(image==steel,"Returning to STEEL changed its pixels");
   check(xs::encode(state)==patchBefore && edits==0 && gestures==0,"Theme selection changed patch state or host gestures");
   const auto &p=xs::theme::style(expected);
   // Exact rendered label/readout glyph masks must match STEEL in fixed boxes.
   for(const auto &c:xs::mainControls)if(c.kind==xs::ControlKind::Knob){
    for(int kind=0;kind<2;++kind){
     int x0=kind?c.x+15-c.valueWidth/2:c.x-35;
     int y0=c.y+48+(kind?49:35),w=kind?c.valueWidth:100,h=kind?12:13;
     uint32_t base=kind?0x7fd7d8:0xe2e2d4,ink=kind?p.readoutText:p.primaryText;
     for(int y=y0;y<y0+h;++y)for(int x=x0;x<x0+w;++x)if(x>=0 && x<xs::NativeWidth)
      check(((steel[y*xs::NativeWidth+x]&0xffffff)==base)==((image[y*xs::NativeWidth+x]&0xffffff)==ink),"Theme changed label/readout glyph geometry");
    }
   }
  }
  check(xs::theme::save(sandbox.key.c_str(),xs::theme::Id::Sand),"Cannot save isolated preference");
  a.close();window=a.open(parent);
  check(a.currentTheme()==xs::theme::Id::Sand,"Reopened editor did not recall theme");
  xs::Editor b(sandbox.key.c_str());check(b.currentTheme()==xs::theme::Id::Sand,"New instance did not recall theme");
  a.setTheme(xs::theme::Id::Win98);
  check(xs::theme::load(sandbox.key.c_str())==xs::theme::Id::Sand,"Preview override wrote preferences");
  check(!xs::theme::save(sandbox.key.c_str(),xs::theme::Id(99)),"Invalid theme was saved");
  HKEY key{};check(RegOpenKeyExW(HKEY_CURRENT_USER,sandbox.key.c_str(),0,KEY_SET_VALUE,&key)==ERROR_SUCCESS,"Open isolated preference failed");
  DWORD invalid=99;RegSetValueExW(key,L"Theme",0,REG_DWORD,reinterpret_cast<BYTE*>(&invalid),sizeof invalid);RegCloseKey(key);
  check(xs::theme::load(sandbox.key.c_str())==xs::theme::Id::Steel,"Invalid preference did not default to STEEL");
  // The same native hit location must edit the same control for every material.
  for(unsigned n=0;n<xs::theme::Count;++n){
   a.setTheme(xs::theme::Id(n));state=xs::defaults();state[4]=.4;
   const auto before=state;
   SendMessageW(window,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(36,247));
   SendMessageW(window,WM_MOUSEMOVE,MK_LBUTTON,MAKELPARAM(36,237));
   SendMessageW(window,WM_LBUTTONUP,0,MAKELPARAM(36,237));
   check(state[4]>.4,"Theme changed knob hit/drag behavior");state[4]=before[4];
   check(state==before,"Theme redirected input to another parameter");
  }
  a.close();DestroyWindow(parent);
  std::cout<<"PASS: six distinct deterministic themes, exact text masks, unchanged patch bytes/gestures, STEEL round-trip, same hitboxes, preference recall across reopen/instances, invalid preference fallback\n";
  return 0;
 }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}
}

