#include "Editor.h"
#include "Version.h"
#include "PresetBrowser.h"
#include "InitState.h"
#include <string>
#include <shellapi.h>
int WINAPI wWinMain(HINSTANCE h, HINSTANCE, LPWSTR, int) {
  xs::State state = xs::newInstanceState();
  xs::table::Document assets;
  xs::Editor editor(nullptr); // Snapshot tooling never reads/writes user preferences.
  bool protect=true;
  editor.readProtect=[&]{return protect;};
  editor.changeProtect=[&](bool on){protect=on;return true;};
  editor.readAssets=[&]{return assets;};
  editor.read = [&] { return state; };
  editor.begin = editor.end = [](int) {};
  editor.change = [&](int i, double n) { state[i] = n; };
  int argc = 0;
  auto argv = CommandLineToArgvW(GetCommandLineW(), &argc);
  for(int i=1;i<argc;++i)if(std::wcscmp(argv[i],L"--theme")==0) {
    if(++i>=argc){LocalFree(argv);return 2;}
    bool found=false;
    for(unsigned n=0;n<xs::theme::Count;++n) {
      const char *name=xs::theme::Names[n];std::wstring wide(name,name+std::strlen(name));
      if(_wcsicmp(argv[i],wide.c_str())==0){editor.setTheme(xs::theme::Id(n));found=true;break;}
    }
    if(!found){LocalFree(argv);return 2;}
  }
  for(int i=1;i<argc;++i)if(std::wcscmp(argv[i],L"--demo")==0) {
    state[xs::Engine1]=state[xs::Engine2]=1;state[24]=.4;state[25]=1;
    state[xs::XMode]=1;state[xs::XDepth]=.28;state[27]=1;state[xs::MixerMode]=.4;
    state[xs::FilterType]=2./3;state[21]=.24;state[22]=.45;state[xs::Keytrack]=.5;
    state[xs::LfoClock]=1;state[xs::Division]=20./29;state[xs::LfoPhase]=1;state[xs::LfoRandom]=1;
    state[xs::Unison]=2./3;state[xs::Detune]=.5;
    state[xs::VoiceMode]=1;state[xs::GlideMode]=2./3;state[8]=.7;
    state[xs::Velocity1]=.6;state[xs::Velocity2]=.4;state[xs::Loop2]=.5;state[xs::Curve2]=1;state[17]=.35;
    const int source[]={1,3,4,6,7,9},destination[]={3,8,14,6,8,1};
    const double amount[]={.5625,.75,.65,.8,.6,.7};
    for(int slot=0;slot<6;++slot){int id=xs::MatrixFirst+slot*3;state[id]=source[slot]/10.;state[id+1]=destination[slot]/double(xs::LegacyDestinationCount-1);state[id+2]=amount[slot];}
  }
  for(int i=1;i<argc;++i)if(std::wcscmp(argv[i],L"--preset")==0&&i+1<argc){int index=_wtoi(argv[++i]);if(index>=0&&index<xs::factory::Count)state=xs::factory::presetState(index);}
  for(int i=1;i<argc;++i)if(std::wcscmp(argv[i],L"--init")==0)state=xs::factory::initState();
  for(int i=1;i<argc;++i)if(std::wcscmp(argv[i],L"--distortion")==0&&i+1<argc){int mode=_wtoi(argv[++i]);if(mode>=0&&mode<=10){state[xs::DistMode]=mode/10.;state[xs::DistDrive]=.65;state[xs::DistMix]=.7;}}
  for(int i=1;i<argc;++i){if(!std::wcscmp(argv[i],L"--limit-invalid")){state[xs::Loop2]=1;state[xs::Eg2Sync]=1;state[xs::Eg2Rate]=1;state[15]=state[16]=state[18]=.7;}if(!std::wcscmp(argv[i],L"--filter-b"))editor.selectedFilter=1;if(!std::wcscmp(argv[i],L"--features")){state[xs::Loop2]=1;state[xs::Eg2Sync]=2./3;state[xs::VcfRoute]=.5;}}
  bool programmer=false;
  for(int i=1;i<argc;++i){if(!std::wcscmp(argv[i],L"--programmer"))programmer=true;
    if(!std::wcscmp(argv[i],L"--wavetable")&&i+1<argc){std::wstring mode=argv[++i];
      if(mode==L"analytic"){state[xs::RasterFirst]=1;state[24]=.4;state[xs::Engine2]=1;}
      else{for(int o=0;o<2;++o){state[xs::oscParam(o,0)]=mode==L"user"?1:.5;state[xs::oscParam(o,1)]=(o?8:32)/65535.;state[xs::oscParam(o,3)]=.35;state[xs::oscParam(o,4)]=1;state[xs::oscParam(o,5)]=1;}
        if(mode==L"mixed")state[xs::oscParam(0,0)]=0;
        if(mode==L"user"){std::vector<float>samples(128*16);for(int f=0;f<16;++f)for(int n=0;n<128;++n)samples[f*128+n]=float(.6*std::sin(2*xs::Pi*n/128)+.2*(f/15.)*std::sin(2*xs::Pi*5*n/128));assets.slots[0]=xs::table::prepareUser(std::move(samples),128,16,"USER DEMO");}
      }
    }
  }
  // Deterministic documentation capture: select a verified factory identity.
  for(int i=1;i+1<argc;++i)if(!std::wcscmp(argv[i],L"--rom-id")||!std::wcscmp(argv[i],L"--rom2-id")){
    int o=!std::wcscmp(argv[i],L"--rom2-id"),id=_wtoi(argv[++i]);bool found=false;
    for(const auto&t:xs::table::factoryBank().tables)found|=t.id==id;
    if(!found){LocalFree(argv);return 2;}
    state[xs::oscParam(o,0)]=.5;state[xs::oscParam(o,1)]=id/65535.;
    state[xs::oscParam(o,3)]=.35;state[xs::oscParam(o,4)]=1;state[xs::oscParam(o,5)]=1;
  }
  if (argc >= 3 && std::wcscmp(argv[1], L"--snapshot") == 0) {
    bool dither = true, about = false;
    for (int i = 4; i < argc; ++i) {
      if (std::wcscmp(argv[i], L"--no-dither") == 0) dither = false;
      if (std::wcscmp(argv[i], L"--about") == 0) about = true;
    }
    bool ok = programmer?editor.snapshotProgrammer(argv[2]):editor.snapshot(
        argv[2], argc > 3 ? _wtoi(argv[3]) : 100, dither, about);
    LocalFree(argv);
    return ok ? 0 : 1;
  }
  LocalFree(argv);
  WNDCLASSW c{};
  c.hInstance = h;
  c.lpszClassName = L"XsynthExpansionDevPreview";
  c.lpfnWndProc = [](HWND w, UINT m, WPARAM a, LPARAM b) -> LRESULT {
    if (m == WM_DESTROY) {
      PostQuitMessage(0);
      return 0;
    }
    return DefWindowProcW(w, m, a, b);
  };
  RegisterClassW(&c);
  RECT r{0, 0, xs::NativeWidth, xs::NativeHeight};
  AdjustWindowRect(&r, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, FALSE);
  auto w = CreateWindowW(c.lpszClassName, XSYNTH_WIDE(XSYNTH_PRODUCT_NAME) L" " XSYNTH_WIDE(XSYNTH_VERSION),
                         WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, CW_USEDEFAULT,
                         CW_USEDEFAULT, r.right - r.left, r.bottom - r.top,
                         nullptr, nullptr, h, nullptr);
  editor.open(w, xs::NativeWidth, xs::NativeHeight);
  ShowWindow(w, SW_SHOW);
  MSG m;
  while (GetMessageW(&m, nullptr, 0, 0) > 0) {
    TranslateMessage(&m);
    DispatchMessageW(&m);
  }
  return 0;
}
