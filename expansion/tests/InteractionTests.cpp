// GPL-2.0-or-later. Production editor input, including Windows' real double-click sequence.
#include "Editor.h"
#include "PresetBrowser.h"
#include "SelectorKind.h"
#include "UiGeometry.h"
#include "DssiDialog.h"
#include "ContextRegions.h"
#include "SourceMenu.h"
#include <iomanip>
#include <commdlg.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
void check(bool b, const char *m) {
  if (!b)
    throw std::runtime_error(m);
}
void click(HWND w, int x, int y, bool rapid = false) {
  SendMessageW(w, rapid ? WM_LBUTTONDBLCLK : WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(x, y));
  SendMessageW(w, WM_LBUTTONUP, 0, MAKELPARAM(x, y));
}
struct Instance {
  HWND child;
};
#include "PickerFixtures.h"
std::vector<uint32_t> capture(HWND w, int width, int height) {
  BITMAPINFO info{};
  info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  info.bmiHeader.biWidth = width;
  info.bmiHeader.biHeight = -height;
  info.bmiHeader.biPlanes = 1;
  info.bmiHeader.biBitCount = 32;
  HDC dc = CreateCompatibleDC(nullptr);
  void *data{};
  auto bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &data, nullptr, 0);
  auto old = SelectObject(dc, bitmap);
  SendMessageW(w, WM_PRINTCLIENT, reinterpret_cast<WPARAM>(dc), PRF_CLIENT);
  GdiFlush();
  auto *p = static_cast<uint32_t *>(data);
  std::vector<uint32_t> out(p, p + width * height);
  SelectObject(dc, old);
  DeleteObject(bitmap);
  DeleteDC(dc);
  return out;
}
#include "HeaderRenderCheck.h"
int main(int argc, char **argv) {
  try {
    using namespace xs;
    namespace fs = std::filesystem;
    bool menusOnly=argc==3&&std::string(argv[2])=="--menus";
    check(argc==2||menusOnly,"Expected test output folder [--menus]");
    fs::path art = fs::absolute(argv[1]);
    fs::create_directories(art);
    PickerFixtures fixtures(GetModuleHandleW(nullptr));
    State state = defaults();
    std::vector<int> changed;
    int gestures = 0;
    HWND parent = CreateWindowW(L"STATIC", L"Interaction test", WS_POPUP, 0, 0, 1920, 1136, nullptr,
                                nullptr, nullptr, nullptr);
    check(parent != nullptr, "Hidden parent");
    Editor editor(nullptr);
    editor.read = [&] { return state; };
    editor.change = [&](int id, double n) {
      state[id] = n;
      changed.push_back(id);
    };
    editor.begin = editor.end = [&](int) { ++gestures; };
    HWND w = editor.open(parent);
    Instance instance{w};
    auto pump = [&] {
      MSG m{};
      while (PeekMessageW(&m, w, 0, 0, PM_REMOVE)) {
        TranslateMessage(&m);
        DispatchMessageW(&m);
      }
    };
    auto factory = [&](int index) {
      factorySelection = index;
      click(w, 550, 28);
      factorySelection = -1;
    };
    // Canonical generated INIT state, audio stability, and one-oscillator proof.
    auto init = factory::initState();
    std::ofstream initTable(art/"INIT-normalized.csv");initTable<<"parameter_id,normalized\n"<<std::setprecision(17);for(int id=0;id<Count;++id)initTable<<id<<','<<init[id]<<'\n';
    auto encoded=encode(init);State restored{};check(decode(encoded.data(),encoded.size(),restored)&&restored==init,"INIT v2 round trip");
    std::ofstream kinds(art/"selector-classification.csv");kinds<<"module,label,parameter_id,kind,x,y,width,height\n";
    auto classify=[&](const Control&c){if(c.kind!=ControlKind::Selector&&c.kind!=ControlKind::Toggle)return;auto field=fieldBounds(c);int panel=context::panelAt(c.x+c.w/2,c.y+48+8,960,568);kinds<<(panel>=0?panels[panel].label:"NONE")<<','<<c.label<<','<<c.id<<','<<(isDropdown(c,state)?"DROPDOWN":"BUTTON")<<','<<field.x<<','<<field.y+48<<','<<field.w<<','<<(c.kind==ControlKind::Toggle?22:field.h)<<'\n';};for(auto c:mainControls)classify(c);for(auto c:matrixControls())classify(c);

    check(init[24] == .4 && init[7] == 0 && init[XMode] == 0 && init[XDepth] == 0 && init[27] == 0,
          "INIT oscillator path");
    check(physical(0, init[0]) == 1 && physical(2, init[2]) == 1 && init[21] == 1 &&
              init[22] == 0 && init[FilterType] == 0 && init[28] == 0 && init[Keytrack] == 0,
          "INIT tuning/filter");
    check(init[11] == 1 && init[17] == 1 && init[5] == 0 && init[6] == 0 && init[13] == 0 &&
              init[14] == 0 && init[19] == 0 && init[20] == 0 && init[VoiceMode] == 1 &&
              init[MasterTune] == .5,
          "INIT modulation/voice");
    for (int slot = 0; slot < 6; ++slot)
      check(init[MatrixFirst + slot * 3] == 0 && init[MatrixFirst + slot * 3 + 2] == .5,
            "INIT route not off");
    Engine a, b;
    a.state = init;
    b.state = init;
    b.state[2] = 1;
    b.state[25] = 1;
    b.state[3] = .1;
    a.reset(44100);
    b.reset(44100);
    a.note(60, true, .8);
    b.note(60, true, .8);
    double energy = 0;
    for (int k = 0; k < 22050; ++k) {
      float x = a.next(), y = b.next();
      check(std::isfinite(x) && std::abs(x) <= 1 && x == y, "INIT unstable or VCO2 audible");
      energy += x * x;
    }
    check(energy > .1, "INIT silent");
    a.note(60, false, 0);
    for (int k = 0; k < 44100; ++k)
      check(std::isfinite(a.next()), "INIT note off invalid");
    check(a.next() == 0, "INIT release stuck");
    factory(factory::Init);
    check(factory::sameSound(state, init), "INIT actual menu load");
    verifyHeaderText(capture(w, 960, 568), 960, 568, 1, theme::Id::Steel, "INIT");
    click(w, ui::PresetNext.x + 12, 28);
    check(factory::sameSound(state, factory::Presets[0].state), "INIT next");
    click(w, ui::PresetNext.x + 12, 28, true);
    check(factory::sameSound(state, factory::Presets[1].state), "Rapid next lost");
    click(w, ui::PresetPrevious.x + 12, 28, true);
    check(factory::sameSound(state, factory::Presets[0].state), "Rapid previous lost");
    int dropdownTests = 0, buttonTests = 0;
    for (int percent : {100, 125, 150, 200}) {
      double scale = percent / 100.;
      pointerScale=scale;
      int width = 960 * percent / 100, height = 568 * percent / 100;
      editor.resize(width, height);
      auto scaledClick = [&](int x, int y, bool fast = false) {
        click(w, int(x * scale), int(y * scale), fast);
      };
      for (unsigned material = 0; material < (menusOnly?0:theme::Count); ++material) {
        editor.setTheme(theme::Id(material));
        state = defaults();
        auto closed = capture(w, width, height);
        scaledClick(60, 24);
        check(capture(w, width, height) != closed, "About first click failed");
        auto start = editor.currentTheme();
        for (int n = 0; n < 6; ++n) {
          scaledClick(480, 492, n % 2);
          check(unsigned(editor.currentTheme()) == (unsigned(start) + n + 1) % 6,
                "THEME rapid click lost or doubled");
          check(GetCapture() != w, "Theme retained capture");
        }
        int links = linkLaunches, mails = mailLaunches;
        scaledClick(480, 250);
        scaledClick(480, 268, true);
        check(linkLaunches == links + 1 && mailLaunches == mails + 1, "Link activation lost");
        scaledClick(60, 24, true);
        check(capture(w, width, height) == closed, "About rapid close failed");
        for (const auto &c : mainControls)
          if (c.kind == ControlKind::Selector || c.kind == ControlKind::Toggle) {
            if (isDropdown(c, state))
              continue;
            state = defaults();
            int id = effectiveId(c, state), count = selectorCount(c, state);
            for (int n = 0; n < 2; ++n) {
              auto before = state;
              int calls = popupCalls;
              changed.clear();
              int x = int((c.x + c.w / 2) * scale), y = int((c.y + 48 + 8) * scale);
              SendMessageW(w, n ? WM_LBUTTONDBLCLK : WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(x, y));
              auto pressed = capture(w, width, height);
              SendMessageW(w, WM_LBUTTONUP, 0, MAKELPARAM(x, y));
              auto released = capture(w, width, height);
              check(pressed != released, "Button pressed feedback missing");
              check(popupCalls == calls, "Cycling button opened popup");
              check(state[id] == double((discrete(id, before[id]) + 1) % count) / (count - 1),
                    "Cycling button did not change once");
              for (int p : changed)
                check(p == id, "Button edited unrelated parameter");
              check(GetCapture() != w, "Button capture stale");
              SendMessageW(w,WM_RBUTTONUP,0,MAKELPARAM(int((c.x+c.w/2)*scale),int((c.y+48+8)*scale)));
              check(state==before,"Right-click reverse did not restore previous discrete value");
              check(popupCalls==calls,"Reverse button also opened a menu");
              int ax = int(std::round((c.x + c.w - 5) * scale)),
                  ay = int(std::round((c.y + 48 + 10) * scale));
              if (c.kind == ControlKind::Selector)
                check((released[ay * width + ax] & 0xffffff) !=
                          theme::style(theme::Id(material)).secondaryText,
                      "Fake dropdown arrow on cycling button");
              ++buttonTests;
            }
          }
      }
      editor.setTheme(theme::Id::Steel);
      std::vector<Control> selectors;
      for (auto c : mainControls)
        if (isDropdown(c, state))
          selectors.push_back(c);
      for (auto c : matrixControls())
        if (isDropdown(c, state))
          selectors.push_back(c);
      for (const auto &c : selectors) {
        int count = selectorCount(c, state);
        for (int choice : {0, 1, 1 + count / 2, count}) {
          auto before = state;
          changed.clear();
          int calls = popupCalls;
          popupChoice = choice;
          injectPointer = true;
          scaledClick(c.x + c.w / 2, c.y + 48 + 8, choice % 2);
          injectPointer = false;
          popupChoice = -999;
          pump();
          check(popupCalls == calls + 1, "Popup open required extra click");
          check(state[18] == before[18], "EG2 Release click-through");
          for (int id = 0; id < Count; ++id)
            if (id != c.id && !(legacyDestination(c.id) && (id==ExpandedDestinationFirst+(c.id-MatrixFirst)/3 || id==DetuneRouteFirst+(c.id-MatrixFirst)/3)) && !(c.id == 26 && id == LfoRandom) && !(matrixSource(c.id)&&id==SourceExtensionFirst+matrixSlot(c.id)))
              check(state[id] == before[id], "Popup affected unrelated control");
          for (int p : changed)
            check(p == c.id || (legacyDestination(c.id) && (p==ExpandedDestinationFirst+(c.id-MatrixFirst)/3 || p==DetuneRouteFirst+(c.id-MatrixFirst)/3)) || (c.id == 26 && p == LfoRandom) || (matrixSource(c.id)&&p==SourceExtensionFirst+matrixSlot(c.id)), "Popup started unrelated gesture");
          if (!choice)
            check(state == before, "Cancel changed state");
          else if(legacyDestination(c.id))check(effectiveDestination(state,(c.id-MatrixFirst)/3)==choice-1,"Unified matrix destination selection");
          check(GetCapture() != w, "Popup retained capture");
          ++dropdownTests;
        }
        // After dismissal the very first knob drag must work.
        state[0] = .5;
        auto before = state[0];
        scaledClick(1, 1); // Empty frame, no action.
        SendMessageW(w, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(int(46 * scale), int(93 * scale)));
        SendMessageW(w, WM_MOUSEMOVE, MK_LBUTTON, MAKELPARAM(int(46 * scale), int(83 * scale)));
        SendMessageW(w, WM_LBUTTONUP, 0, MAKELPARAM(int(46 * scale), int(83 * scale)));
        check(state[0] > before, "First drag lost after menu");
      }
      // Exercise the production menu and wheel from every shape on every LFO.
      Control shape{};for(auto c:mainControls)if(c.id==26)shape=c;
      auto shapeWheel=[&](int delta){POINT point{int((shape.x+shape.w/2)*scale),int((shape.y+48+8)*scale)};ClientToScreen(w,&point);SendMessageW(w,WM_MOUSEWHEEL,MAKEWPARAM(0,short(delta)),MAKELPARAM(point.x,point.y));};
      for(int lfo=0;lfo<4;++lfo){auto tab=lfoTabs::bounds(lfo);scaledClick(tab.x+tab.w/2,tab.y+48+tab.h/2);
        int wave=lfoParam(lfo,3),random=lfoParam(lfo,7);
        auto index=[&]{return state[random]>0?5+discrete(random,state[random]):discrete(wave,state[wave]);};
        auto verify=[&](const State &before,int expected){
          check(index()==expected,"LFO wheel/menu shape mismatch");
          check(state[random]==(expected>=6?(expected-5)/2.:0),"LFO random shape encoding");
          if(expected<6)check(state[wave]==expected/5.,"LFO classic shape encoding");
          for(int p=0;p<Count;++p)if(p!=wave&&p!=random)check(state[p]==before[p],"LFO shape edited unrelated parameter");
        };
        auto menu=[&](int chosen){auto before=state;expectedLfoShape=index();popupChoice=chosen+1;int calls=popupCalls;
          scaledClick(shape.x+shape.w/2,shape.y+48+8);pump();expectedLfoShape=-1;popupChoice=-999;
          check(popupCalls==calls+1,"LFO shape menu missing");verify(before,chosen);
        };
        for(int start=0;start<8;++start){
          menu(start);int expected=start;unsigned visited=1u<<start;
          for(int step=0;step<8;++step){auto before=state;shapeWheel(120);expected=std::min(7,expected+1);verify(before,expected);visited|=1u<<expected;}
          // Wheel-to-menu coherence and checked item after the upper clamp.
          menu(7);
          for(int step=0;step<8;++step){auto before=state;shapeWheel(-120);expected=std::max(0,expected-1);verify(before,expected);visited|=1u<<expected;}
          check(visited==255,"Wheel did not visit every LFO shape including S&H/SMOOTH");
          menu(start);expected=start;
          for(int step=0;step<8;++step){auto before=state;shapeWheel(-120);expected=std::max(0,expected-1);verify(before,expected);}
          for(int step=0;step<8;++step){auto before=state;shapeWheel(120);expected=std::min(7,expected+1);verify(before,expected);}
          menu(start);
        }
      }
      {auto tab=lfoTabs::bounds(0);scaledClick(tab.x+tab.w/2,tab.y+48+tab.h/2);}
      const int identities[]={0,1,11,12,13,2,3,4,5,6,7,8,9,10};
      for(int slot=0;slot<12;++slot)for(int display=0;display<14;++display){
        auto c=matrixControls()[slot*3];int id=c.id,extension=SourceExtensionFirst+slot;
        int current=effectiveSource(state,slot);expectedSourcePosition=0;
        while(identities[expectedSourcePosition]!=current)++expectedSourcePosition;
        auto before=state;popupChoice=display+1;injectPointer=true;
        scaledClick(c.x+c.w/2,c.y+48+8,display%2);pump();injectPointer=false;popupChoice=-999;expectedSourcePosition=-1;
        int source=identities[display];check(effectiveSource(state,slot)==source,"Displayed source selected wrong internal identity");
        if(source<11)check(state[id]==source/10.&&state[extension]==0,"Historical source encoding changed");
        else check(state[extension]==(source-10)/3.,"Extended LFO source encoding changed");
        for(int p=0;p<Count;++p)if(p!=id&&p!=extension)check(state[p]==before[p],"Source menu edited unrelated state");
        auto wheel=[&](int delta){POINT point{int((c.x+c.w/2)*scale),int((c.y+48+8)*scale)};ClientToScreen(w,&point);SendMessageW(w,WM_MOUSEWHEEL,MAKEWPARAM(0,short(delta)),MAKELPARAM(point.x,point.y));};
        wheel(120);check(effectiveSource(state,slot)==identities[std::min(13,display+1)],"Source wheel does not follow displayed order");
        if(display<13){wheel(-120);check(effectiveSource(state,slot)==source,"Source wheel reverse mapping");}
      }
    }
    check(sourceMenuChecks==12*14*4,"Not all source choices/slots/scales inspected");
    check(lfoShapeMenuChecks==4*8*4*4,"Not all LFO shapes/tabs/scales inspected");
    std::cout<<"PASS all 8 LFO shapes: every starting shape, forward/backward clamping, menu/wheel coherence, 4 LFOs x 4 scales; unrelated state preserved\n";
    std::cout<<"PASS exact Source menu order/checkmarks and all 14 identities on 12 slots at 4 scales; stable /10 and LFO extension encoding; forward/reverse wheel mapping\n";
    pointerScale=1;editor.resize(960,568);auto heldState=state;popupChoice=0;fixtureHeld=true;SendMessageW(w,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(70,327));check(GetCapture()==w,"Dismissal did not own held release");SendMessageW(w,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(684,247));fixtureHeld=false;SendMessageW(w,WM_LBUTTONUP,0,MAKELPARAM(684,247));popupChoice=-999;check(state==heldState&&GetCapture()!=w,"Held dismissal clicked through or retained capture");
    editor.resize(960, 568);
    editor.setTheme(theme::Id::Steel);
    // Matrix dropdown opening belongs to completed clicks, not button holds.
    state=defaults();popupChoice=0;int menuCount=popupCalls;
    SendMessageW(w,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(60,MatrixFirstRow+48+9));
    check(popupCalls==menuCount,"Matrix menu opened while opening button held");
    SendMessageW(w,WM_LBUTTONUP,0,MAKELPARAM(60,MatrixFirstRow+48+9));
    check(popupCalls==menuCount+1,"Matrix menu did not persist/open on completed click");popupChoice=-999;
    // Amount only: dead zone, axis lock, both signs and Shift sensitivity.
    auto amountControl=matrixControls()[2];int ax=amountControl.x+amountControl.w/2,ay=amountControl.y+48+10;
    auto dragAmount=[&](int dx,int dy,int laterX,int laterY,bool fine){
      int id=amountControl.id;state[id]=.5;
      SendMessageW(w,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(ax,ay));
      SendMessageW(w,WM_MOUSEMOVE,MK_LBUTTON,MAKELPARAM(ax+1,ay+1));check(state[id]==.5,"Amount dead zone edited value");
      SendMessageW(w,WM_MOUSEMOVE,MK_LBUTTON|(fine?MK_SHIFT:0),MAKELPARAM(ax+dx,ay+dy));
      double first=state[id];
      SendMessageW(w,WM_MOUSEMOVE,MK_LBUTTON|(fine?MK_SHIFT:0),MAKELPARAM(ax+laterX,ay+laterY));
      bool horizontal=std::abs(dx)>=std::abs(dy);double delta=horizontal?laterX:-laterY;
      check(std::abs(state[id]-(.5+delta/(fine?2000.:200.)))<1e-10,"Amount axis lock/sign/fine value");
      SendMessageW(w,WM_LBUTTONUP,0,MAKELPARAM(ax+laterX,ay+laterY));check(GetCapture()!=w,"Amount capture leaked");return first;
    };
    dragAmount(20,2,30,80,false);dragAmount(-20,2,-30,-80,false);
    dragAmount(2,-20,80,-30,false);dragAmount(2,20,-80,30,false);
    dragAmount(10,-10,-20,90,true);dragAmount(1,-10,80,20,true);
    dragAmount(10,5,-20,80,false);
    for(unsigned material=0;material<theme::Count;++material){editor.setTheme(theme::Id(material));state[amountControl.id]=.75;
      auto shown=capture(w,960,568);check((shown[(ay-5)*960+amountControl.x+3]&0xffffff)==0xf5f5eb,"Amount field is not pale/opaque");
      int black=0;for(int y=amountControl.y+49;y<amountControl.y+63;++y)for(int x=amountControl.x+3;x<amountControl.x+amountControl.w-3;++x)black+=(shown[y*960+x]&0xffffff)==0;
      check(black>10,"Amount black text missing");
    }
    editor.setTheme(theme::Id::Steel);
    // Filename is authoritative even when the saved sound exactly equals a factory.
    auto saveNamed = [&](const wchar_t *file, const char *label) {
      DialogRun run;
      run.operation = 2;
      run.path = (art / file).wstring();
      fileAction(instance, run);
      verifyHeaderText(capture(w, 960, 568), 960, 568, 1, theme::Id::Steel, label);
      DialogRun load;
      load.path = run.path;
      fileAction(instance, load);
      verifyHeaderText(capture(w, 960, 568), 960, 568, 1, theme::Id::Steel, label);
    };
    factory(6);
    saveNamed(L"My Strings.XseDev", "My Strings");
    fs::copy_file(art / "My Strings.XseDev", art / "Renamed Strings.XseDev",
                  fs::copy_options::overwrite_existing);
    DialogRun rename;
    rename.path = (art / "Renamed Strings.XseDev").wstring();
    fileAction(instance, rename);
    verifyHeaderText(capture(w, 960, 568), 960, 568, 1, theme::Id::Steel, "Renamed Strings");
    saveNamed(L"Brass \u00e9.XseDev",u8"Brass \u00e9");
    DialogRun import;
    import.operation = 1;
    import.path = fs::absolute("tests/dssi/banks/XsynthBank1").wstring();
    import.expectedCount = 60;
    import.selected = 11;
    fileAction(instance, import);
    saveNamed(L"Converted Lead.XseDev", "Converted Lead");
    auto savedLabel=capture(w,960,568);DialogRun cancel;cancel.operation=2;cancel.cancel=true;cancel.path=(art/"Cancelled.XseDev").wstring();fileAction(instance,cancel);check(capture(w,960,568)==savedLabel,"Cancelled save changed display name");
    fs::create_directories(art/"Unwritable.XseDev");DialogRun fail;fail.operation=2;fail.path=(art/"Unwritable.XseDev").wstring();expectFileError=true;fileAction(instance,fail);expectFileError=false;check(fileErrors==1&&capture(w,960,568)==savedLabel,"Failed save changed name or did not report failure");
    click(w, ui::PresetNext.x + 12, 28);
    check(factory::sameSound(state, init), "External next did not go INIT");
    click(w, ui::PresetNext.x + 12, 28, true);
    check(factory::sameSound(state, factory::Presets[0].state), "First factory after INIT");
    editor.close();
    DestroyWindow(parent);
    std::cout << "PASS: Windows rapid-click sequence; About/THEME/links, INIT menu/audio, "
              << buttonTests << " button activations and " << dropdownTests
              << " popup stress selections/cancels at four scales/six themes; EG2 Release and "
                 "queued/direct click-through isolation; capture cleanup; filename "
                 "Save/Load/rename/DSSI authority\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "FAIL: " << e.what() << '\n';
    return 1;
  }
}
