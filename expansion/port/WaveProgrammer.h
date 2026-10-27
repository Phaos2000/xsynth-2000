// GPL-2.0-or-later. First browser/import window; no destructive editor features.
#pragma once
#include "Editor.h"
#include "WaveImport.h"
#include "WaveImportDialog.h"
#include "FactoryFamilies.h"
#include "RomMenu.h"
#include "SelectorGeometry.h"
#include <commdlg.h>
#include <filesystem>
#include <fstream>
#include <future>
#include <windowsx.h>
namespace xs {
class WaveProgrammer {
  Editor&owner;HWND window{},list{},target{},source{},slot{},sample{},frame{},position{},status{};
  HINSTANCE instance{};
  HFONT font{};HBRUSH background{};int vco=0;
  std::future<std::shared_ptr<const table::UserTable>>job;
  int importSlot=0,importVco=0;uint64_t importRevision=0;
  std::vector<int> listIds;
  HWND secondTarget{},frameReadout{};
  std::shared_ptr<const table::UserTable> imported;
  struct ChildLayout{HWND window;int x,y,w,h;bool combo;};
  std::vector<ChildLayout> children;
  int dpi=96;
  int z(int n)const{return MulDiv(n,dpi,96);}
  void change(int id,double value){owner.begin(id);owner.change(id,value);owner.end(id);}
  void tableSource(int kind){owner.lastTableSource[vco]=kind;if(owner.read()[oscParam(vco,0)]!=0)change(oscParam(vco,0),kind/2.);}
  HWND child(const wchar_t*kind,const wchar_t*text,int id,int x,int y,int w,int h,DWORD style=0){
    HWND c=CreateWindowExW(0,kind,text,WS_CHILD|WS_VISIBLE|style|(!std::wcscmp(kind,L"STATIC")?0:WS_TABSTOP)|(!std::wcscmp(kind,L"BUTTON")?BS_OWNERDRAW:0)|(!std::wcscmp(kind,L"COMBOBOX")?(CBS_OWNERDRAWFIXED|CBS_HASSTRINGS):0),z(x),z(y),z(w),z(h),window,HMENU(INT_PTR(id)),nullptr,nullptr);SendMessageW(c,WM_SETFONT,WPARAM(font),TRUE);
    if(!std::wcscmp(kind,L"COMBOBOX")){SendMessageW(c,CB_SETITEMHEIGHT,WPARAM(-1),z(14));for(int retry=0;retry<3;++retry){RECT r;GetWindowRect(c,&r);int height=int(SendMessageW(c,CB_GETITEMHEIGHT,WPARAM(-1),0));if(r.bottom-r.top==z(18))break;SendMessageW(c,CB_SETITEMHEIGHT,WPARAM(-1),std::max(1,height+z(18)-int(r.bottom-r.top)));}}
    children.push_back({c,x,y,w,h,!std::wcscmp(kind,L"COMBOBOX")});
    if(!std::wcscmp(kind,L"COMBOBOX")){SetPropW(c,L"XsynthComboOwner",this);auto previous=SetWindowLongPtrW(c,GWLP_WNDPROC,LONG_PTR(&comboProc));SetPropW(c,L"XsynthComboProc",HANDLE(previous));}
    HMODULE ux=LoadLibraryW(L"uxtheme.dll");if(ux){auto disable=reinterpret_cast<HRESULT(WINAPI*)(HWND,LPCWSTR,LPCWSTR)>(GetProcAddress(ux,"SetWindowTheme"));if(disable)disable(c,L"",L"");FreeLibrary(ux);}return c;
  }
  void drawCombo(HWND control,HDC dc){RECT r;GetClientRect(control,&r);const auto&style=theme::style(owner.currentTheme());auto color=[](uint32_t rgb){auto c=theme::unpack(rgb);return RGB(int(c.r),int(c.g),int(c.b));};HBRUSH brush=CreateSolidBrush(color(style.fieldFace));FillRect(dc,&r,brush);DeleteObject(brush);HPEN edge=CreatePen(PS_SOLID,std::max(1,z(1)),color(style.bevelShadow));auto oldPen=SelectObject(dc,edge);auto oldBrush=SelectObject(dc,GetStockObject(NULL_BRUSH));Rectangle(dc,r.left,r.top,r.right,r.bottom);SelectObject(dc,oldBrush);SelectObject(dc,oldPen);DeleteObject(edge);int index=int(SendMessageW(control,CB_GETCURSEL,0,0));wchar_t text[512]{};if(index>=0)SendMessageW(control,CB_GETLBTEXT,index,LPARAM(text));SetTextColor(dc,color(style.readoutText));SetBkMode(dc,TRANSPARENT);auto oldFont=SelectObject(dc,font);auto label=selector::textRect(r,dpi/96.);DrawTextW(dc,text,-1,&label,DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS|DT_NOPREFIX);SelectObject(dc,oldFont);selector::drawArrow(dc,r,dpi/96.,color(style.readoutText),color(style.bevelSecondary));}
  static LRESULT CALLBACK comboProc(HWND c,UINT msg,WPARAM wp,LPARAM lp){auto*self=static_cast<WaveProgrammer*>(GetPropW(c,L"XsynthComboOwner"));auto previous=reinterpret_cast<WNDPROC>(GetPropW(c,L"XsynthComboProc"));if(msg==WM_PAINT){PAINTSTRUCT ps;auto dc=BeginPaint(c,&ps);self->drawCombo(c,dc);EndPaint(c,&ps);return 0;}if(msg==WM_PRINT||msg==WM_PRINTCLIENT){self->drawCombo(c,HDC(wp));return 0;}if(msg==WM_ERASEBKGND)return 1;auto result=CallWindowProcW(previous,c,msg,wp,lp);if(msg==CB_SETCURSEL||msg==CB_SHOWDROPDOWN||msg==WM_ENABLE||msg==WM_SETFOCUS||msg==WM_KILLFOCUS||msg==WM_LBUTTONDOWN||msg==WM_LBUTTONUP||msg==WM_LBUTTONDBLCLK||msg==WM_MOUSEWHEEL||msg==WM_KEYDOWN||msg==WM_KEYUP){InvalidateRect(c,nullptr,FALSE);UpdateWindow(c);}if(msg==WM_NCDESTROY){RemovePropW(c,L"XsynthComboOwner");RemovePropW(c,L"XsynthComboProc");}return result;}
  int selectedSource()const{return SendMessageW(source,CB_GETCURSEL,0,0)==std::size(table::FamilyNames)?1:0;}
  int selectedSlot()const{return int(SendMessageW(slot,CB_GETCURSEL,0,0));}
  void describe(){
    auto s=owner.read();auto users=owner.readAssets?owner.readAssets():table::Document{};
    int which=selectedSource();const table::Table*t=nullptr;std::string name;
    int index=int(SendMessageW(list,CB_GETCURSEL,0,0));
    if(which==0&&index>=0&&index<int(table::factoryBank().tables.size())){t=&table::factoryBank().tables[listIds[index]];name=t->name.data();}
    if(which==1&&index>=0&&index<16&&users.slots[index]){t=&users.slots[index]->playback;name=users.slots[index]->name;}
    char text[384]{};
    if(t){std::snprintf(text,sizeof text,"%s | %d samples x %d frames | rootBin %d",which?"USER | FLOAT32":"FACTORY ROM | 8-BIT PCM",t->length,t->frames,t->rootBin);
      auto f=table::chooseFrames(*t,float(s[oscParam(vco,3)]),table::Frame(discrete(oscParam(vco,5),s[oscParam(vco,5)])));char frameText[96];
      if(s[oscParam(vco,5)]>=.5)std::snprintf(frameText,sizeof frameText,"%d -> %d / %d  (%.1f%%)",f.a+1,f.b+1,t->frames,f.blend*100);else std::snprintf(frameText,sizeof frameText,"%d / %d",f.a+1,t->frames);SetWindowTextA(frameReadout,frameText);
    }else{std::snprintf(text,sizeof text,"EMPTY USER SLOT | SILENT");SetWindowTextA(frameReadout,"EMPTY");}
    SetWindowTextA(status,text);InvalidateRect(window,nullptr,FALSE);
  }
  static std::wstring wide(const std::string&s){int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.c_str(),int(s.size()),nullptr,0);std::wstring result(n,L' ' );if(n)MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.c_str(),int(s.size()),result.data(),n);return result;}
  void populate(){
    auto s=owner.read();auto users=owner.readAssets?owner.readAssets():table::Document{};
    SendMessageW(list,CB_RESETCONTENT,0,0);
    listIds.clear();int bankIndex=int(SendMessageW(source,CB_GETCURSEL,0,0));
    if(selectedSource()==0){for(int i:romMenu::order(table::factoryBank())){const auto&t=table::factoryBank().tables[i];if(t.familyId==bankIndex){listIds.push_back(i);SendMessageA(list,CB_ADDSTRING,0,LPARAM(t.name.data()));}}}
    else for(int i=0;i<16;++i){std::string name=std::to_string(i+1)+" "+(users.slots[i]?users.slots[i]->name:"EMPTY");listIds.push_back(i);auto unicode=wide(name);SendMessageW(list,CB_ADDSTRING,0,LPARAM(unicode.c_str()));}
    int selection=0;for(size_t i=0;i<listIds.size();++i){int identity=selectedSource()==0?table::factoryBank().tables[listIds[i]].id:listIds[i];if(identity==discrete(oscParam(vco,selectedSource()==0?1:2),s[oscParam(vco,selectedSource()==0?1:2)]))selection=int(i);}
    SendMessageW(target,BM_SETSTATE,vco==0,0);SendMessageW(secondTarget,BM_SETSTATE,vco==1,0);
    SendMessageW(list,CB_SETCURSEL,selection,0);SendMessageW(slot,CB_SETCURSEL,discrete(oscParam(vco,2),s[oscParam(vco,2)]),0);
    SendMessageW(sample,CB_SETCURSEL,discrete(oscParam(vco,4),s[oscParam(vco,4)]),0);SendMessageW(frame,CB_SETCURSEL,discrete(oscParam(vco,5),s[oscParam(vco,5)]),0);
    wchar_t text[32];swprintf_s(text,L"%.3f",s[oscParam(vco,3)]*100);SetWindowTextW(position,text);describe();
  }
  void load(bool single){
    if(job.valid()){MessageBoxW(window,L"An import is already being prepared.",L"WAVE PROGRAMMER",MB_OK);return;}
    importRevision=owner.readAssetsRevision?owner.readAssetsRevision():0;
    wchar_t path[32768]{};OPENFILENAMEW dialog{sizeof dialog};dialog.hwndOwner=window;dialog.lpstrFilter=L"WAVE files\0*.wav\0\0";dialog.lpstrFile=path;dialog.nMaxFile=32768;dialog.Flags=OFN_NOCHANGEDIR|OFN_FILEMUSTEXIST;
    if(!GetOpenFileNameW(&dialog))return;
    std::filesystem::path file(path);std::pair<std::vector<uint8_t>,table::WaveData>input;
    try{auto read=std::async(std::launch::async,[file]{std::ifstream f(file,std::ios::binary|std::ios::ate);if(!f||f.tellg()<0||f.tellg()>32*1024*1024)throw std::runtime_error("Input must be at most32MiB");std::vector<uint8_t>bytes(size_t(f.tellg()));f.seekg(0);f.read(reinterpret_cast<char*>(bytes.data()),bytes.size());if(!f)throw std::runtime_error("Incomplete WAV read");auto info=table::decodeWave(bytes);return std::make_pair(std::move(bytes),std::move(info));});input=read.get();}
    catch(const std::exception&e){MessageBoxA(window,e.what(),"IMPORT",MB_ICONERROR);return;}
    WaveImportChoices choices;choices.info=std::move(input.second);choices.single=single;if(!choices.show(window,instance))return;
    importSlot=selectedSlot();importVco=vco;int n=choices.length,ch=choices.channel;bool remove=choices.dc,norm=choices.normalize;
    job=std::async(std::launch::async,[bytes=std::move(input.first),file,n,ch,remove,norm]()mutable{
      auto wave=table::decodeWave(bytes,ch);return table::importWave(std::move(wave),n,file.stem().u8string(),remove,norm);
    });
    SetWindowTextW(status,L"Preparing immutable USER data off the audio thread...");SetTimer(window,1,40,nullptr);
  }
  void finished(){
    if(!job.valid()||job.wait_for(std::chrono::seconds(0))!=std::future_status::ready)return;
    KillTimer(window,1);
    try{imported=job.get();if(owner.readAssetsRevision&&owner.readAssetsRevision()!=importRevision)throw std::runtime_error("Patch changed while importing; import cancelled");auto doc=owner.readAssets?owner.readAssets():table::Document{};doc.slots[importSlot]=imported;
      auto state=owner.read();if(state[oscParam(importVco,0)]!=0)state[oscParam(importVco,0)]=1;state[oscParam(importVco,2)]=importSlot/15.;
      auto validated=encodeDocument(state,doc); // Includes aggregate/dedup bounds before publication.
      if(!owner.restoreDocument||!owner.restoreDocument(state,doc))throw std::runtime_error("Document handoff busy or unsupported; retry after current transition");
      owner.lastTableSource[importVco]=2;
      if(vco==importVco)SendMessageW(source,CB_SETCURSEL,std::size(table::FamilyNames),0);populate();
      const auto&t=imported->playback;bool seam=false;for(int f=0;f<t.frames;++f)seam|=std::abs(t.pcm[size_t(f)*t.length]-t.pcm[size_t(f+1)*t.length-1])>.05f;
      if(seam)MessageBoxW(window,L"Imported. A cycle seam is present; samples were preserved.",L"WAVE PROGRAMMER",MB_OK);
    }catch(const std::exception&e){MessageBoxA(window,e.what(),"WAVE PROGRAMMER",MB_ICONERROR);describe();}
    imported.reset();
  }
  void paint(HDC targetDC=nullptr){
    PAINTSTRUCT ps{};HDC dc=targetDC?targetDC:BeginPaint(window,&ps);RECT client;GetClientRect(window,&client);
    const auto&style=theme::style(owner.currentTheme());
    for(int y=0;y<384;++y){double mix=std::floor(y/383.*31)/31;auto c=theme::blend(theme::unpack(style.panelTop),theme::unpack(style.panelBottom),mix);HBRUSH brush=CreateSolidBrush(RGB(int(c.r),int(c.g),int(c.b)));RECT row{0,z(y),z(480),z(y+1)};FillRect(dc,&row,brush);DeleteObject(brush);}
    RECT shell{z(1),z(1),z(479),z(383)};DrawEdge(dc,&shell,EDGE_RAISED,BF_RECT);
    auto state=owner.read();auto users=owner.readAssets?owner.readAssets():table::Document{};int index=int(SendMessageW(list,CB_GETCURSEL,0,0));const table::Table*t=nullptr;
    if(selectedSource()==0&&index>=0&&index<int(table::factoryBank().tables.size()))t=&table::factoryBank().tables[listIds[index]];
    if(selectedSource()==1&&index>=0&&index<16&&users.slots[index])t=&users.slots[index]->playback;
    RECT panel{z(12),z(100),z(468),z(212)};DrawEdge(dc,&panel,EDGE_SUNKEN,BF_RECT);auto aqua=theme::unpack(style.activePrimary);HPEN pen=CreatePen(PS_SOLID,1,RGB(int(aqua.r),int(aqua.g),int(aqua.b)));auto old=SelectObject(dc,pen);
    if(t){float p=float(state[oscParam(vco,3)]);auto frames=table::chooseFrames(*t,p,table::Frame(discrete(oscParam(vco,5),state[oscParam(vco,5)])));
      for(int x=0;x<448;++x){float y=table::read(*t,x/448.,p,table::Sample(discrete(oscParam(vco,4),state[oscParam(vco,4)])),table::Frame(discrete(oscParam(vco,5),state[oscParam(vco,5)])),130.81278/t->rootBin,48000);int xx=z(16+x),yy=z(156-int(y*50));if(x)LineTo(dc,xx,yy);else MoveToEx(dc,xx,yy,nullptr);}}
    SelectObject(dc,old);DeleteObject(pen);
    auto ink=[&](uint32_t steel){auto color=theme::unpack(theme::rgb(steel,owner.currentTheme()));return RGB(int(color.r),int(color.g),int(color.b));};
    for(int x:{6,474})for(int y:{6,378}){HBRUSH brush=CreateSolidBrush(ink(0x4a737b));HPEN edge=CreatePen(PS_SOLID,std::max(1,z(1)),ink(0x081b21));auto oldBrush=SelectObject(dc,brush),oldPen=SelectObject(dc,edge);Ellipse(dc,z(x-3),z(y-3),z(x+3),z(y+3));SelectObject(dc,oldBrush);SelectObject(dc,oldPen);DeleteObject(brush);DeleteObject(edge);
      for(int line=0;line<2;++line){HPEN slit=CreatePen(PS_SOLID,std::max(1,z(1)),ink(line?0x0c1f24:0xacc7c3));auto old=SelectObject(dc,slit);MoveToEx(dc,z(x-1),z(y+(line?1:-2)),nullptr);LineTo(dc,z(x+1),z(y+(line?-1:-2)));SelectObject(dc,old);DeleteObject(slit);}
    }
    if(!targetDC)EndPaint(window,&ps);
  }
  static LRESULT CALLBACK proc(HWND w,UINT msg,WPARAM wp,LPARAM lp){
    auto*self=reinterpret_cast<WaveProgrammer*>(GetWindowLongPtrW(w,GWLP_USERDATA));if(msg==WM_NCCREATE){self=static_cast<WaveProgrammer*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);self->window=w;SetWindowLongPtrW(w,GWLP_USERDATA,LONG_PTR(self));}
    if(!self)return DefWindowProcW(w,msg,wp,lp);
    if(msg==WM_MEASUREITEM){auto item=reinterpret_cast<MEASUREITEMSTRUCT*>(lp);if(item&&item->CtlType==ODT_COMBOBOX){item->itemHeight=self->z(14);return TRUE;}}
    if(msg==WM_DPICHANGED){self->dpi=HIWORD(wp);HFONT font=CreateFontW(-self->z(10),0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,NONANTIALIASED_QUALITY,0,L"Tahoma");auto previous=self->font;self->font=font;for(const auto&c:self->children){SendMessageW(c.window,WM_SETFONT,WPARAM(font),TRUE);SetWindowPos(c.window,nullptr,self->z(c.x),self->z(c.y),self->z(c.w),self->z(c.h),SWP_NOZORDER|SWP_NOACTIVATE);if(c.combo)SendMessageW(c.window,CB_SETITEMHEIGHT,WPARAM(-1),self->z(14));}DeleteObject(previous);auto r=reinterpret_cast<RECT*>(lp);SetWindowPos(w,nullptr,r->left,r->top,r->right-r->left,r->bottom-r->top,SWP_NOZORDER|SWP_NOACTIVATE);InvalidateRect(w,nullptr,TRUE);return 0;}
    if(msg==WM_DRAWITEM){auto item=reinterpret_cast<DRAWITEMSTRUCT*>(lp);
      if(item&&item->CtlType==ODT_COMBOBOX){const auto&style=theme::style(self->owner.currentTheme());auto bg=theme::unpack(style.fieldFace),fg=theme::unpack(style.readoutText);HBRUSH brush=CreateSolidBrush(RGB(int(bg.r),int(bg.g),int(bg.b)));FillRect(item->hDC,&item->rcItem,brush);DeleteObject(brush);if(item->itemID!=UINT(-1)){wchar_t text[512]{};SendMessageW(item->hwndItem,CB_GETLBTEXT,item->itemID,LPARAM(text));SetTextColor(item->hDC,RGB(int(fg.r),int(fg.g),int(fg.b)));SetBkMode(item->hDC,TRANSPARENT);auto old=SelectObject(item->hDC,self->font);RECT r=item->rcItem;r.left+=self->z(4);DrawTextW(item->hDC,text,-1,&r,DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS);SelectObject(item->hDC,old);}return TRUE;}
      if(item&&item->CtlType==ODT_BUTTON){
      const auto&style=theme::style(self->owner.currentTheme());bool down=(item->itemState&ODS_SELECTED)||(item->CtlID==1&&self->vco==0)||(item->CtlID==13&&self->vco==1);RECT r=item->rcItem;
      for(int y=r.top;y<r.bottom;++y){auto color=theme::blend(theme::unpack(style.selectorTop),theme::unpack(style.selectorBottom),double(y-r.top)/std::max(1L,r.bottom-r.top-1));HBRUSH brush=CreateSolidBrush(RGB(int(color.r),int(color.g),int(color.b)));RECT row{r.left,y,r.right,y+1};FillRect(item->hDC,&row,brush);DeleteObject(brush);}DrawEdge(item->hDC,&r,down?EDGE_SUNKEN:EDGE_RAISED,BF_RECT);
      wchar_t text[256];GetWindowTextW(item->hwndItem,text,256);auto color=theme::unpack(down?style.activePrimary:style.primaryText);SetTextColor(item->hDC,RGB(int(color.r),int(color.g),int(color.b)));SetBkMode(item->hDC,TRANSPARENT);auto old=SelectObject(item->hDC,self->font);if(down)OffsetRect(&r,0,1);DrawTextW(item->hDC,text,-1,&r,DT_CENTER|DT_VCENTER|DT_SINGLELINE);SelectObject(item->hDC,old);return TRUE;
    }}
    if(msg==WM_PAINT){self->paint();return 0;}
    if(msg==WM_PRINTCLIENT){self->paint(HDC(wp));return 0;}
    if(msg==WM_TIMER){self->finished();return 0;}
    if(msg==WM_CTLCOLORSTATIC||msg==WM_CTLCOLORLISTBOX||msg==WM_CTLCOLOREDIT){auto a=theme::unpack(theme::style(self->owner.currentTheme()).primaryText),b=theme::unpack(theme::style(self->owner.currentTheme()).panelBottom);SetTextColor(HDC(wp),RGB(int(a.r),int(a.g),int(a.b)));SetBkColor(HDC(wp),RGB(int(b.r),int(b.g),int(b.b)));if(msg==WM_CTLCOLORSTATIC){SetBkMode(HDC(wp),TRANSPARENT);return LRESULT(GetStockObject(NULL_BRUSH));}return LRESULT(self->background);}
    if(msg==WM_CLOSE){ShowWindow(w,SW_HIDE);return 0;}
    if(msg==WM_COMMAND){int id=LOWORD(wp),code=HIWORD(wp);
      if(id==1||id==13){self->vco=id==1?0:1;self->selectBank();self->populate();}
      if(id==2&&code==CBN_SELCHANGE)self->populate();
      if(id==3&&code==CBN_SELENDOK){int index=int(SendMessageW(self->list,CB_GETCURSEL,0,0));if(index>=0){int which=self->selectedSource();self->change(oscParam(self->vco,which?2:1),(which?index:table::factoryBank().tables[self->listIds[index]].id)/double(which?15:65535));self->tableSource(which?2:1);}self->describe();}
      if(id==4&&code==CBN_SELCHANGE){self->change(oscParam(self->vco,4),SendMessageW(self->sample,CB_GETCURSEL,0,0)/2.);self->describe();}
      if(id==5&&code==CBN_SELCHANGE){self->change(oscParam(self->vco,5),double(SendMessageW(self->frame,CB_GETCURSEL,0,0)));self->describe();}
      if((id==6&&code==EN_KILLFOCUS)||id==14){wchar_t text[32]{};GetWindowTextW(self->position,text,32);wchar_t*end=nullptr;double value=wcstod(text,&end);if(end!=text&&std::isfinite(value))self->change(oscParam(self->vco,3),std::clamp(value/100,0.,1.));self->describe();}
      if(id==7&&code==CBN_SELENDOK){int index=self->selectedSlot();auto assets=self->owner.readAssets?self->owner.readAssets():table::Document{};if(!assets.slots[index]){SetWindowTextW(self->status,L"EMPTY SLOT SELECTED AS IMPORT TARGET | PLAYBACK UNCHANGED");return 0;}self->change(oscParam(self->vco,2),index/15.);self->tableSource(2);SendMessageW(self->source,CB_SETCURSEL,std::size(table::FamilyNames),0);self->populate();}
      if(id==10||id==11)self->load(id==10);
      if(id==12)ShowWindow(w,SW_HIDE);return 0;
    }
    return DefWindowProcW(w,msg,wp,lp);
  }
public:
  explicit WaveProgrammer(Editor&o):owner(o){}
  ~WaveProgrammer(){if(window)DestroyWindow(window);if(instance)UnregisterClassW(L"Xsynth2000WaveProgrammer",instance);if(font)DeleteObject(font);if(background)DeleteObject(background);}

  void refreshTheme(){if(background)DeleteObject(background);auto color=theme::unpack(theme::style(owner.currentTheme()).panelBottom);background=CreateSolidBrush(RGB(int(color.r),int(color.g),int(color.b)));if(window){InvalidateRect(window,nullptr,TRUE);for(HWND c=GetWindow(window,GW_CHILD);c;c=GetWindow(c,GW_HWNDNEXT))InvalidateRect(c,nullptr,TRUE);}}
  void selectBank(){auto s=owner.read();int family=0;if(s[oscParam(vco,0)]>=.75)family=int(std::size(table::FamilyNames));else{int id=discrete(oscParam(vco,1),s[oscParam(vco,1)]);for(const auto&t:table::factoryBank().tables)if(t.id==id)family=t.familyId;}SendMessageW(source,CB_SETCURSEL,family,0);}
  void open(HWND parent,int oscillator,bool visible=true){
    vco=oscillator;if(parent&&!IsWindowVisible(parent))visible=false;
    if(!window){dpi=parent?int(GetDpiForWindow(parent)):96;if(dpi<96)dpi=96;
      font=CreateFontW(-MulDiv(10,dpi,96),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,NONANTIALIASED_QUALITY,0,L"Tahoma");
      auto color=theme::unpack(theme::style(owner.currentTheme()).panelBottom);background=CreateSolidBrush(RGB(int(color.r),int(color.g),int(color.b)));
      WNDCLASSW cls{};cls.lpfnWndProc=proc;GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<LPCWSTR>(&proc),&instance);cls.hInstance=instance;cls.lpszClassName=L"Xsynth2000WaveProgrammer";cls.hCursor=LoadCursor(nullptr,IDC_ARROW);RegisterClassW(&cls);
      RECT r{0,0,z(480),z(384)};AdjustWindowRectEx(&r,WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU,FALSE,WS_EX_TOOLWINDOW);
      window=CreateWindowExW(WS_EX_TOOLWINDOW,cls.lpszClassName,L"WAVE PROGRAMMER",WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU,100,100,r.right-r.left,r.bottom-r.top,parent,nullptr,instance,this);
      child(L"STATIC",L"WAVE PROGRAMMER",0,12,8,456,14,SS_CENTER);
      target=child(L"BUTTON",L"VCO1",1,12,30,222,18);secondTarget=child(L"BUTTON",L"VCO2",13,246,30,222,18);
      child(L"STATIC",L"BANK",0,12,56,222,13);child(L"STATIC",L"TABLE",0,246,56,222,13);
      source=child(L"COMBOBOX",L"",2,12,70,222,220,CBS_DROPDOWNLIST|WS_VSCROLL);for(auto name:table::FamilyNames)SendMessageA(source,CB_ADDSTRING,0,LPARAM(name));SendMessageW(source,CB_ADDSTRING,0,LPARAM(L"USER"));
      list=child(L"COMBOBOX",L"",3,246,70,222,220,CBS_DROPDOWNLIST|WS_VSCROLL);
      child(L"STATIC",L"POSITION (%)",0,12,220,222,13);child(L"STATIC",L"FRAME / BLEND",0,246,220,222,13);
      position=child(L"EDIT",L"0",6,12,234,222,18,WS_BORDER);frameReadout=child(L"STATIC",L"",0,246,234,222,18,SS_CENTER|WS_BORDER);
      child(L"STATIC",L"SAMPLE",0,12,256,222,13);child(L"STATIC",L"FRAME",0,246,256,222,13);
      sample=child(L"COMBOBOX",L"",4,12,270,222,100,CBS_DROPDOWNLIST);for(auto x:{L"HOLD",L"LINEAR",L"BAND"})SendMessageW(sample,CB_ADDSTRING,0,LPARAM(x));
      frame=child(L"COMBOBOX",L"",5,246,270,222,100,CBS_DROPDOWNLIST);for(auto x:{L"STEP",L"MORPH"})SendMessageW(frame,CB_ADDSTRING,0,LPARAM(x));
      child(L"BUTTON",L"LOAD SINGLE CYCLE...",10,12,300,222,18);child(L"BUTTON",L"LOAD WAVETABLE...",11,246,300,222,18);
      child(L"STATIC",L"USER SLOT / IMPORT TARGET",0,12,324,222,13);slot=child(L"COMBOBOX",L"",7,12,338,222,180,CBS_DROPDOWNLIST|WS_VSCROLL);for(int i=1;i<=16;++i){std::wstring name=L"USER "+std::to_wstring(i);SendMessageW(slot,CB_ADDSTRING,0,LPARAM(name.c_str()));}
      child(L"BUTTON",L"CLOSE",12,246,338,222,18);status=child(L"STATIC",L"",0,12,362,456,12,SS_CENTER);
    }
    selectBank();populate();ShowWindow(window,visible?SW_SHOW:SW_HIDE);if(visible)SetForegroundWindow(window);
  }
  HBITMAP capture(){RECT r;GetClientRect(window,&r);HDC dc=CreateCompatibleDC(nullptr);HDC screen=GetDC(nullptr);HBITMAP bitmap=CreateCompatibleBitmap(screen,r.right,r.bottom);ReleaseDC(nullptr,screen);auto old=SelectObject(dc,bitmap);paint(dc);
    for(HWND child=GetWindow(window,GW_CHILD);child;child=GetWindow(child,GW_HWNDNEXT)){RECT bounds;GetWindowRect(child,&bounds);MapWindowPoints(nullptr,window,reinterpret_cast<POINT*>(&bounds),2);int saved=SaveDC(dc);SetViewportOrgEx(dc,bounds.left,bounds.top,nullptr);SendMessageW(child,WM_PRINT,WPARAM(dc),PRF_CLIENT|PRF_NONCLIENT|PRF_CHILDREN|PRF_ERASEBKGND);
      // Hidden native combo/edit controls omit their client text in WM_PRINT.
      // Ask the same production owner-draw handler to paint the selected item.
      wchar_t kind[32]{};GetClassNameW(child,kind,32);

      if(!_wcsicmp(kind,L"EDIT")){wchar_t text[256]{};GetWindowTextW(child,text,256);RECT r;GetClientRect(child,&r);r.left+=z(3);auto color=theme::unpack(theme::style(owner.currentTheme()).primaryText);SetTextColor(dc,RGB(int(color.r),int(color.g),int(color.b)));SetBkMode(dc,TRANSPARENT);auto oldFont=SelectObject(dc,font);DrawTextW(dc,text,-1,&r,DT_SINGLELINE|DT_VCENTER);SelectObject(dc,oldFont);}
      RestoreDC(dc,saved);}
    GdiFlush();SelectObject(dc,old);DeleteDC(dc);return bitmap;}
};
}
