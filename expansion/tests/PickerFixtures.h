// GPL-2.0-or-later. Test-process-only picker fixtures; production DLL unchanged on disk.
// Real editor menus, hidden importer controls and file I/O still execute.
struct DialogRun {
  int operation = 0, expectedCount = 0, selected = 0;
  std::wstring path;
  bool cancel = false;
};
inline DialogRun *dialogRun = nullptr;
inline bool fixtureHeld=false;
inline SHORT WINAPI fixtureAsync(int key){return key==VK_LBUTTON&&fixtureHeld?SHORT(0x8000):0;}
inline int popupChoice=-999,popupCalls=0;
inline int expectedSourcePosition=-1,sourceMenuChecks=0;
inline int expectedLfoShape=-1,lfoShapeMenuChecks=0;
inline int expectedDestination=-1,destinationMenuChecks=0;
inline bool expectedDestinationOff=false;
inline void verifyDestinationMenu(HMENU menu,int current){
  current=xs::shapePositionIdentity(current);
  const char* roots[]={"VCO1","XMOD","VCO2","Mixer","Filter","LFO","EG1 (VCA)","EG2","Voice"};
  check(GetMenuItemCount(menu)==10,"Destination module category count");
  char offLabel[16]{};GetMenuStringA(menu,0,offLabel,sizeof offLabel,MF_BYPOSITION);
  check(std::string(offLabel)=="Off"&&GetMenuItemID(menu,0)==xs::DestinationCount+1,"Destination Off clear command");
  check(bool(GetMenuState(menu,0,MF_BYPOSITION)&MF_CHECKED)==expectedDestinationOff,"Destination Off checkmark");
  bool seen[xs::DestinationCount]{};int leaves=0,checked=0;
  auto items=[&](HMENU child,int root,bool filterB){
    check(child&&GetMenuItemCount(child)>0,"Empty destination module");
    for(int i=0;i<GetMenuItemCount(child);++i){int id=int(GetMenuItemID(child,i))-1;char label[96]{};
      check(id>=0&&id<xs::DestinationCount&&id!=42&&id!=43&&!seen[id]&&!GetSubMenu(child,i),"Destination missing/duplicate/invalid command");
      seen[id]=true;++leaves;GetMenuStringA(child,i,label,sizeof label,MF_BYPOSITION);
      check(std::string(label)==xs::Destinations[id],"Destination label/identity changed");
      bool selected=bool(GetMenuState(child,i,MF_BYPOSITION)&MF_CHECKED);checked+=selected;
      check(selected==(!expectedDestinationOff&&id==current),"Grouped destination checkmark mismatch");
      std::string text=label;int category=text.rfind("VCO1 ",0)==0?0:text.rfind("XMOD ",0)==0?1:text.rfind("VCO2 ",0)==0?2:text.rfind("Mixer ",0)==0?3:text.rfind("VCF ",0)==0?4:text.rfind("LFO ",0)==0?5:text.rfind("EG1 ",0)==0?6:text.rfind("EG2 ",0)==0?7:8;
      check(category==root,"Destination is in the wrong module category");
      if(root==4)check((text.rfind("VCF B ",0)==0)==filterB,"Filter A/B category mismatch");
    }
  };
  for(int root=0;root<9;++root){char label[96]{};GetMenuStringA(menu,root+1,label,sizeof label,MF_BYPOSITION);
    check(std::string(label)==roots[root],"Destination module label/order");auto child=GetSubMenu(menu,root+1);
    if(root==4){check(child&&GetMenuItemCount(child)==2,"Filter A/B submenu count");
      for(int f=0;f<2;++f){GetMenuStringA(child,f,label,sizeof label,MF_BYPOSITION);check(std::string(label)==(f?"B":"A"),"Filter A/B submenu label");items(GetSubMenu(child,f),root,f!=0);}
    }else items(child,root,false);
  }
  for(int id=0;id<xs::DestinationCount;++id)check(seen[id]==(id!=42&&id!=43),"Unified destination visibility");
  check(leaves==xs::DestinationCount-2&&checked==(expectedDestinationOff?0:1),"Grouped destinations not exhaustive/unique");++destinationMenuChecks;
}
inline std::vector<std::string> expectedVcoLabels;
inline bool expectFileError=false;
inline int fileErrors=0;
inline bool injectPointer=false;
inline double pointerScale=1;
inline int factorySelection = -1;
inline int expectedContext=-1, contextMenus=0;
inline int linkLaunches = 0, mailLaunches = 0;
inline HINSTANCE WINAPI fixtureLink(HWND, LPCWSTR operation, LPCWSTR target, LPCWSTR, LPCWSTR,
                                    int) {
  check(operation && std::wstring(operation) == L"open" && target &&
            (std::wstring(target) == L"https://phaos.neocities.org/" ||
             std::wstring(target) == L"mailto:phaos2000@pm.me"),
        "About link target");
  if (std::wstring(target) == L"mailto:phaos2000@pm.me")
    ++mailLaunches;
  else
    ++linkLaunches;
  return reinterpret_cast<HINSTANCE>(33);
}
inline bool contextProbeActive=false;
inline void contextProbe(HWND owner){if(!owner||contextProbeActive)return;contextProbeActive=true;for(auto p:{POINT{50,90},POINT{350,90},POINT{280,245},POINT{540,245}})SendMessageW(owner,WM_RBUTTONUP,0,MAKELPARAM(p.x,p.y));contextProbeActive=false;}
inline int WINAPI fixtureMessage(HWND,LPCWSTR,LPCWSTR,UINT){check(expectFileError,"Unexpected product error dialog");++fileErrors;return IDOK;}
inline BOOL WINAPI fixtureFile(LPOPENFILENAMEW file) {
  contextProbe(file->hwndOwner);
  if(dialogRun&&dialogRun->operation==2&&dialogRun->cancel)return FALSE;
  check(dialogRun != nullptr, "Unexpected file dialog");
  check(dialogRun->path.size() + 1 < file->nMaxFile, "Fixture path too long");
  wcscpy_s(file->lpstrFile, file->nMaxFile, dialogRun->path.c_str());
  return TRUE;
}
inline BOOL WINAPI fixtureMenu(HMENU menu, UINT flags, int, int, int, HWND owner, const RECT *) {
  contextProbe(owner);
  if(popupChoice!=-999){++popupCalls;
    if(expectedDestination>=0)verifyDestinationMenu(menu,expectedDestination);
    if(!expectedVcoLabels.empty()){
      std::vector<std::string> leaves;std::vector<UINT>commands;
      if(expectedVcoLabels.size()==69){
        const char*folders[]={"SPECTRAL","PULSE","FORMANT","DIGITAL","MODULATION","INSTRUMENTS","PAD","HARMONY","SFX"};
        check(GetMenuItemCount(menu)==9,"ROM folder inventory");
        const int expectedCounts[]={9,3,3,5,6,13,11,14,5};
        for(int group=0;group<9;++group){char folder[128]{};GetMenuStringA(menu,group,folder,sizeof folder,MF_BYPOSITION);check(std::string(folder)==folders[group],"ROM folder label/order");
          auto child=GetSubMenu(menu,group);check(child&&GetMenuItemCount(child)==expectedCounts[group],"ROM folder leaf count");
          for(int i=0;i<GetMenuItemCount(child);++i){char label[128]{};GetMenuStringA(child,i,label,sizeof label,MF_BYPOSITION);leaves.emplace_back(label);commands.push_back(GetMenuItemID(child,i));}
        }
      }else for(int i=0;i<GetMenuItemCount(menu);++i){char label[128]{};GetMenuStringA(menu,i,label,sizeof label,MF_BYPOSITION);leaves.emplace_back(label);commands.push_back(GetMenuItemID(menu,i));}
      check(leaves==expectedVcoLabels,"VCO menu lost or reordered source-family identities");
      for(size_t i=0;i<commands.size();++i)check(commands[i]==UINT(i+1),"VCO nested leaf command mapping");
    }
    if(expectedLfoShape>=0){
      const char *labels[]={"SINE","TRI","SAW UP","SAW DN","SQUARE","PULSE","S&H","SMOOTH"};
      check(GetMenuItemCount(menu)==8,"LFO shape menu count");
      for(int i=0;i<8;++i){char label[96]{};GetMenuStringA(menu,i,label,sizeof label,MF_BYPOSITION);
        check(std::string(label)==labels[i]&&GetMenuItemID(menu,i)==UINT(i+1),"LFO shape order/command");
        check(bool(GetMenuState(menu,i,MF_BYPOSITION)&MF_CHECKED)==(i==expectedLfoShape),"LFO shape checked item");}
      ++lfoShapeMenuChecks;
    }
    if(expectedSourcePosition>=0){
      const char*labels[]={"Off","LFO 1","LFO 2","LFO 3","LFO 4","EG1","EG2","Velocity","Key","Mod Wheel","Pressure","Pitch Bend","Note Random","Constant"};
      check(GetMenuItemCount(menu)==14,"Source menu count");
      for(int i=0;i<14;++i){char label[96]{};GetMenuStringA(menu,i,label,sizeof(label),MF_BYPOSITION);
        check(std::string(label)==labels[i]&&GetMenuItemID(menu,i)==UINT(i+1),"Source menu displayed order/command");
        check(bool(GetMenuState(menu,i,MF_BYPOSITION)&MF_CHECKED)==(i==expectedSourcePosition),"Source menu checked item mapping");}
      ++sourceMenuChecks;
    }
    if(injectPointer){
      auto spot=[&](int x,int y){return MAKELPARAM(int(x*pointerScale),int(y*pointerScale));};
      SendMessageW(owner,WM_LBUTTONDOWN,MK_LBUTTON,spot(684,247));
      SendMessageW(owner,WM_LBUTTONDBLCLK,MK_LBUTTON,spot(684,247));
      SendMessageW(owner,WM_MOUSEMOVE,MK_LBUTTON,spot(684,210));
      SendMessageW(owner,WM_MOUSEWHEEL,MAKEWPARAM(0,WHEEL_DELTA),spot(684,247));
      SendMessageW(owner,WM_LBUTTONUP,0,spot(684,210));
      PostMessageW(owner,WM_LBUTTONDOWN,MK_LBUTTON,spot(684,247));
      PostMessageW(owner,WM_LBUTTONUP,0,spot(684,247));
    }
    return popupChoice;
  }
  if(expectedContext>=0){check((flags&TPM_RIGHTBUTTON)&&GetMenuItemCount(menu)==(expectedContext<2?8:7),"Linked wrong context model");++contextMenus;return 0;}
  if (factorySelection != -1) {
    check(GetMenuItemCount(menu) == 5, "Linked factory grouping");
    check(GetMenuItemID(menu,0)==xs::factory::InitCommand && (GetMenuState(menu,1,MF_BYPOSITION)&MF_SEPARATOR),"INIT is not first/separate");
    for (int bank = 0; bank < 3; ++bank) {
      char label[256]{};
      GetMenuStringA(menu, bank+2, label, 256, MF_BYPOSITION);
      check(std::string(label) == xs::factory::Banks[bank].name, "Linked bank name");
      auto child = GetSubMenu(menu, bank+2);
      check(GetMenuItemCount(child) == xs::factory::Banks[bank].count, "Linked bank count");
      for (int k = 0; k < GetMenuItemCount(child); ++k) {
        int id = xs::factory::Banks[bank].first + k;
        GetMenuStringA(child, k, label, 256, MF_BYPOSITION);
        check(std::string(label) == xs::factory::Presets[id].name &&
                  GetMenuItemID(child, k) == 1000 + id,
              "Linked factory name/order");
      }
    }
    return factorySelection==xs::factory::Init?xs::factory::InitCommand:1000+factorySelection;
  }
  check(dialogRun != nullptr && GetMenuItemCount(menu) == 2, "Linked Load menu");
  return dialogRun->operation + 1;
}
inline INT_PTR WINAPI fixtureChooser(HINSTANCE h, LPCDLGTEMPLATE data, HWND owner, DLGPROC proc,
                                     LPARAM argument) {
  contextProbe(owner);
  check(dialogRun != nullptr, "Unexpected chooser");
  auto &r = *dialogRun;
  auto *choice = reinterpret_cast<xs::dssi::Choice *>(argument);
  HWND window = CreateDialogIndirectParamW(h, data, owner, proc, argument);
  check(window != nullptr && !IsWindowVisible(window), "Hidden real import chooser");
  HWND list = GetDlgItem(window, 100);
  int count = int(SendMessageW(list, LB_GETCOUNT, 0, 0));
  check(count == r.expectedCount, "Linked bank entry count");
  for (int k = 0; k < count; ++k) {
    int length = int(SendMessageW(list, LB_GETTEXTLEN, k, 0));
    std::wstring text(size_t(length) + 1, L'\0');
    SendMessageW(list, LB_GETTEXT, k, reinterpret_cast<LPARAM>(text.data()));
    auto expected =
        xs::dssi::wide(std::to_string(k + 1) + ". " + choice->bank->entries[k].patch.name);
    check(text.c_str() == expected, "Linked chooser patch name");
    SendMessageW(list, LB_SETCURSEL, k, 0);
    SendMessageW(window, WM_COMMAND, MAKEWPARAM(100, LBN_SELCHANGE), 0);
    check(IsWindowEnabled(GetDlgItem(window, IDOK)), "Linked entry disabled");
  }
  SendMessageW(list, LB_SETCURSEL, r.selected, 0);
  SendMessageW(window, WM_COMMAND, MAKEWPARAM(100, LBN_SELCHANGE), 0);
  wchar_t notes[4096]{};
  GetDlgItemTextW(window, 101, notes, 4096);
  check(wcsstr(notes, L"CLEAN is the nearest") != nullptr, "Conversion notes missing");
  choice->selected = r.cancel ? -1 : r.selected;
  DestroyWindow(window);
  return r.cancel ? IDCANCEL : IDOK;
}
class PickerFixtures {
  struct Entry {
    ULONG_PTR *slot;
    ULONG_PTR original;
  };
  std::vector<Entry> entries;

public:
  explicit PickerFixtures(HMODULE module) {
    auto *base = reinterpret_cast<BYTE *>(module);
    auto *dos = reinterpret_cast<IMAGE_DOS_HEADER *>(base);
    auto *nt = reinterpret_cast<IMAGE_NT_HEADERS *>(base + dos->e_lfanew);
    auto directory = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    auto *imports = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR *>(base + directory.VirtualAddress);
    for (; imports->Name; ++imports) {
      if (!imports->OriginalFirstThunk)
        continue;
      auto *names = reinterpret_cast<IMAGE_THUNK_DATA *>(base + imports->OriginalFirstThunk);
      auto *slots = reinterpret_cast<IMAGE_THUNK_DATA *>(base + imports->FirstThunk);
      for (; names->u1.AddressOfData; ++names, ++slots) {
        if (IMAGE_SNAP_BY_ORDINAL(names->u1.Ordinal))
          continue;
        auto *name = reinterpret_cast<IMAGE_IMPORT_BY_NAME *>(base + names->u1.AddressOfData);
        ULONG_PTR replacement = 0;
        if (!strcmp((char *)name->Name, "GetOpenFileNameW") ||
            !strcmp((char *)name->Name, "GetSaveFileNameW"))
          replacement = reinterpret_cast<ULONG_PTR>(fixtureFile);
        else if (!strcmp((char *)name->Name, "TrackPopupMenu"))
          replacement = reinterpret_cast<ULONG_PTR>(fixtureMenu);
        else if (!strcmp((char *)name->Name, "DialogBoxIndirectParamW"))
          replacement = reinterpret_cast<ULONG_PTR>(fixtureChooser);
        if (!strcmp((char *)name->Name, "ShellExecuteW"))
          replacement = reinterpret_cast<ULONG_PTR>(fixtureLink);
        if (!strcmp((char*)name->Name,"MessageBoxW"))replacement=reinterpret_cast<ULONG_PTR>(fixtureMessage);
        if (!strcmp((char*)name->Name,"GetAsyncKeyState"))replacement=reinterpret_cast<ULONG_PTR>(fixtureAsync);
        if (replacement) {
          DWORD old{};
          auto *slot = &slots->u1.Function;
          check(VirtualProtect(slot, sizeof(*slot), PAGE_READWRITE, &old), "Fixture slot access");
          entries.push_back({slot, *slot});
          *slot = replacement;
          DWORD ignored{};
          VirtualProtect(slot, sizeof(*slot), old, &ignored);
        }
      }
    }
    check(entries.size() == 7, "All OS picker imports replaced in test process");
  }
  ~PickerFixtures() {
    for (auto &e : entries) {
      DWORD old{}, ignored{};
      VirtualProtect(e.slot, sizeof(*e.slot), PAGE_READWRITE, &old);
      *e.slot = e.original;
      VirtualProtect(e.slot, sizeof(*e.slot), old, &ignored);
    }
  }
};
inline void fileAction(Instance &i, DialogRun &run) {
  dialogRun = &run;
  click(i.child, run.operation == 2 ? 880 : 770, 28);
  dialogRun = nullptr;
}
