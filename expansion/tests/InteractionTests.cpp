// GPL-2.0-or-later. Production editor input, including Windows' real double-click sequence.
#include "Editor.h"
#include "PresetBrowser.h"
#include "SelectorKind.h"
#include "UiGeometry.h"
#include "DssiDialog.h"
#include "ContextRegions.h"
#include "SourceMenu.h"
#include "RomMenu.h"
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
    auto classify=[&](const Control&c){if(c.kind!=ControlKind::Selector&&c.kind!=ControlKind::Toggle)return;auto field=fieldBounds(c);int panel=context::panelAt(c.x+c.w/2,c.y+48+8,xs::NativeWidth,568);kinds<<(panel>=0?panels[panel].label:"NONE")<<','<<c.label<<','<<c.id<<','<<(isDropdown(c,state)?"DROPDOWN":"BUTTON")<<','<<field.x<<','<<field.y+48<<','<<field.w<<','<<(c.kind==ControlKind::Toggle?22:field.h)<<'\n';};for(auto c:mainControls)classify(c);for(auto c:matrixControls())classify(c);

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
    verifyHeaderText(capture(w, xs::NativeWidth, 568), xs::NativeWidth, 568, 1, theme::Id::Steel, "INIT");
    click(w, ui::PresetNext.x + 12, 28);
    check(factory::sameSound(state, factory::presetState(0)), "INIT next");
    click(w, ui::PresetNext.x + 12, 28, true);
    check(factory::sameSound(state, factory::presetState(1)), "Rapid next lost");
    click(w, ui::PresetPrevious.x + 12, 28, true);
    check(factory::sameSound(state, factory::presetState(0)), "Rapid previous lost");
    int dropdownTests = 0, buttonTests = 0;
    for (int percent : {100, 125, 150, 200}) {
      double scale = percent / 100.;
      pointerScale=scale;
      int width = xs::NativeWidth * percent / 100, height = 568 * percent / 100;
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
          scaledClick(xs::ui::ThemeButton.x+xs::ui::ThemeButton.w/2, xs::ui::ThemeButton.y+xs::ui::ThemeButton.h/2, n % 2);
          check(unsigned(editor.currentTheme()) == (unsigned(start) + n + 1) % 6,
                "THEME rapid click lost or doubled");
          check(GetCapture() != w, "Theme retained capture");
        }
        int links = linkLaunches, mails = mailLaunches;
        scaledClick(480, xs::ui::CreditY[2]+8);
        scaledClick(480, xs::ui::AboutEmailY+8, true);
        check(linkLaunches == links + 1 && mailLaunches == mails + 1, "Link activation lost");
        scaledClick(60, 24, true);
        check(capture(w, width, height) == closed, "About rapid close failed");
        for (const auto &c : mainControls)
          if ((c.kind == ControlKind::Selector || c.kind == ControlKind::Toggle)&&c.id!=24&&c.id!=25&&c.id!=Engine1&&c.id!=Engine2&&c.id!=Loop2&&c.id!=Eg2Sync) {
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
              check(id==Voices?voiceChoice(state)==(voiceChoice(before)+1)%count:state[id] == double((discrete(id, before[id]) + 1) % count) / (count - 1),
                    "Cycling button did not change once");
              for (int p : changed)
                check(p == id||(id==Voices&&p==VoicesExtension), "Button edited unrelated parameter");
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
          if(matrixDestination(c.id)){expectedDestination=routeDestination(state,matrixSlot(c.id));expectedDestinationOff=false;}
          injectPointer = true;
          scaledClick(c.x + c.w / 2, c.y + 48 + 8, choice % 2);
          injectPointer = false;
          popupChoice = -999;
          expectedDestination=-1;
          pump();
          check(popupCalls == calls + 1, "Popup open required extra click");
          check(state[18] == before[18], "EG2 Release click-through");
          auto related=[&](int id){return id==c.id || (legacyDestination(c.id)&&(id==ExpandedDestinationFirst+matrixSlot(c.id)||id==DetuneRouteFirst+matrixSlot(c.id))) || (matrixDestination(c.id)&&(id==PositionRouteFirst+matrixSlot(c.id)||id==BFilterRouteFirst+matrixSlot(c.id))) || (c.id==26&&id==LfoRandom) || (matrixSource(c.id)&&id==SourceExtensionFirst+matrixSlot(c.id));};
          for (int id = 0; id < Count; ++id)
            if (!related(id))check(state[id] == before[id], "Popup affected unrelated control");
          for (int p : changed)
            check(related(p), "Popup started unrelated gesture");
          if (!choice)
            check(state == before, "Cancel changed state");
          else if(matrixDestination(c.id))check(routeDestination(state,matrixSlot(c.id))==choice-1,"Unified matrix destination selection");
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
          for(int step=0;step<8;++step){auto before=state;shapeWheel(-120);expected=std::min(7,expected+1);verify(before,expected);visited|=1u<<expected;}
          // Wheel-to-menu coherence and checked item after the upper clamp.
          menu(7);
          for(int step=0;step<8;++step){auto before=state;shapeWheel(120);expected=std::max(0,expected-1);verify(before,expected);visited|=1u<<expected;}
          check(visited==255,"Wheel did not visit every LFO shape including S&H/SMOOTH");
          menu(start);expected=start;
          for(int step=0;step<8;++step){auto before=state;shapeWheel(120);expected=std::max(0,expected-1);verify(before,expected);}
          for(int step=0;step<8;++step){auto before=state;shapeWheel(-120);expected=std::min(7,expected+1);verify(before,expected);}
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
        wheel(-120);check(effectiveSource(state,slot)==identities[std::min(13,display+1)],"Source wheel does not follow displayed order");
        if(display<13){wheel(120);check(effectiveSource(state,slot)==source,"Source wheel reverse mapping");}
      }
    }

    int clearChecks=0;
    for(int percent:{100,125,150,200}){double scale=percent/100.;editor.resize(xs::NativeWidth*percent/100,568*percent/100);
      for(int slot=0;slot<12;++slot)for(int source=0;source<SourceCount;++source){
        state=defaults();if(source<LegacySourceCount)state[matrixId(slot)]=source/10.;else state[SourceExtensionFirst+slot]=(source-10)/3.;state[matrixId(slot,2)]=.91;
        auto c=matrixControls()[slot*3+1];auto before=state;auto expected=state;expected[matrixId(slot)]=0;expected[SourceExtensionFirst+slot]=0;
        expectedDestination=routeDestination(state,slot);expectedDestinationOff=false;popupChoice=DestinationCount+1;
        click(w,int((c.x+c.w/2)*scale),int((c.y+48+8)*scale));pump();check(state==expected&&effectiveSource(state,slot)==0,"Destination Off must disable only its source and extension");
        expectedDestinationOff=false;popupChoice=0;click(w,int((c.x+c.w/2)*scale),int((c.y+48+8)*scale));pump();check(state==expected,"Cancelled Off menu changed route");
        expectedDestination=-1;expectedDestinationOff=false;popupChoice=-999;++clearChecks;
      }
    }
    check(clearChecks==12*SourceCount*4,"Missing destination Off clear coverage");
    std::cout<<"PASS "<<clearChecks<<" destination Off clears/cancel/labels across12 routes,14 sources,4 scales; target/amount/state encoding preserved\n";
    check(sourceMenuChecks==12*14*4,"Not all source choices/slots/scales inspected");
    check(lfoShapeMenuChecks==4*8*4*4,"Not all LFO shapes/tabs/scales inspected");
    std::cout<<"PASS all 8 LFO shapes: every starting shape, forward/backward clamping, menu/wheel coherence, 4 LFOs x 4 scales; unrelated state preserved\n";
    std::cout<<"PASS exact Source menu order/checkmarks and all 14 identities on 12 slots at 4 scales; stable /10 and LFO extension encoding; forward/reverse wheel mapping\n";
    // Exhaust every ordinary discrete field and every matrix destination, using
    // the actual editor event path. Painted border pixels are valid targets too.
    long long directionChecks=0;
    for(int percent:{100,125,150,200}){double scale=percent/100.;editor.resize(xs::NativeWidth*percent/100,568*percent/100);
      auto sendWheel=[&](Bounds b,int delta,bool edge){int x=int(std::round((edge?b.x:b.x+b.w/2)*scale));
        int y=int(std::round((b.y+48+(edge?0:b.h/2))*scale));POINT pt{x,y};ClientToScreen(w,&pt);
        SendMessageW(w,WM_MOUSEWHEEL,MAKEWPARAM(0,short(delta)),MAKELPARAM(pt.x,pt.y));};
      for(auto c:mainControls){if(c.kind!=ControlKind::Selector&&c.kind!=ControlKind::Toggle)continue;
        if(c.id==24||c.id==25||c.id==Engine1||c.id==Engine2||lfoBase(c.id)==26||c.id==Loop2||c.id==Eg2Sync)continue;
        state=defaults();int id=effectiveId(c,state),count=selectorCount(c,state);
        for(int start=0;start<count;++start)for(int delta:{-120,120,0})for(bool edge:{false,true}){
          if(id==Voices){state=defaults();if(start==5||start==6)state[VoicesExtension]=(start-4)/2.;else state[Voices]=(start==7?5:start)/5.;}else state[id]=start/double(count-1);auto expected=state;int next=std::clamp(start+(delta<0?1:delta>0?-1:0),0,count-1);
          if(id==Voices){if(delta){if(next==5||next==6)expected[VoicesExtension]=(next-4)/2.;else {expected[Voices]=(next==7?5:next)/5.;expected[VoicesExtension]=0;}}}else expected[id]=next/double(count-1);sendWheel({c.x,c.y,c.w,c.kind==ControlKind::Toggle?22:18},delta,edge);
          check(state==expected,"Shared selector direction/clamp/border/zero-delta mismatch");++directionChecks;
        }
      }
      for(auto c:matrixControls())if(matrixDestination(c.id))for(int start=0;start<DestinationCount;++start)if(start!=42&&start!=43)for(int delta:{-120,120,0}){
        state=defaults();expectedDestination=routeDestination(state,matrixSlot(c.id));expectedDestinationOff=false;popupChoice=start+1;click(w,int((c.x+c.w/2)*scale),int((c.y+48+8)*scale));pump();popupChoice=-999;expectedDestination=-1;
        check(routeDestination(state,matrixSlot(c.id))==start,"Destination menu setup mismatch");
        int step=delta<0?1:delta>0?-1:0,position=start>=44?start-2:start;
        position=std::clamp(position+step,0,DestinationCount-3);int next=position>=42?position+2:position;
        sendWheel({c.x,c.y,c.w,18},delta,true);check(routeDestination(state,matrixSlot(c.id))==next,"Destination wheel/menu order mismatch");
        auto wheeled=state;expectedDestination=next;expectedDestinationOff=false;popupChoice=next+1;click(w,int((c.x+c.w/2)*scale),int((c.y+48+8)*scale));pump();popupChoice=-999;expectedDestination=-1;
        check(state==wheeled,"Destination wheel/menu encoding mismatch");++directionChecks;
      }
    }
    std::cout<<"PASS "<<directionChecks<<" ordinary-selector/destination direction, clamp, border, zero-delta and menu coherence checks\n";
    check(destinationMenuChecks>0,"Grouped native destination menus were not inspected");
    std::cout<<"PASS "<<destinationMenuChecks<<" grouped native destination menus;44 visible leaves and stable historical aliases\n";
    const auto romOrder=romMenu::order(table::factoryBank());
    check(romOrder.size()==69,"Review changed factory menu inventory");
    const int expectedRomOrder[]={0,1,7,36,37,45,46,47,48,2,38,39,3,15,26,4,5,49,50,51,6,40,41,42,43,44,
      8,9,10,11,12,13,14,16,17,52,53,54,55,18,19,20,35,21,22,23,24,25,27,28,29,30,31,32,33,34,56,57,58,59,61,66,67,68,60,62,63,64,65};
    for(int i=0;i<69;++i)check(table::factoryBank().tables[romOrder[i]].id==expectedRomOrder[i],"Consolidated ROM category order/identity");
    // Explicit per-VCO family button is the only main-panel OSC/TABLE route.
    for(int percent:{100,125,150,200}){double scale=percent/100.;editor.resize(xs::NativeWidth*percent/100,568*percent/100);
      auto fieldClick=[&](Bounds b){click(w,int((b.x+b.w/2)*scale),int((b.y+48+b.h/2)*scale));pump();};
      auto wheel=[&](Bounds b,int direction){POINT point{int((b.x+b.w/2)*scale),int((b.y+48+b.h/2)*scale)};ClientToScreen(w,&point);SendMessageW(w,WM_MOUSEWHEEL,MAKEWPARAM(0,short(direction*120)),MAKELPARAM(point.x,point.y));};
      for(int o=0;o<2;++o){state=defaults();expectedVcoLabels={"SINE","TRI","SAW UP","SAW DN","SQUARE","PULSE"};
        for(int start=0;start<6;++start){popupChoice=start+1;fieldClick(vcoGrid::wave(o));check(discrete(24+o,state[24+o])==start&&state[oscParam(o,0)]==0,"Analytic menu/source coherence");
          for(int direction:{1,-1}){state[24+o]=start/5.;int expected=start;for(int step=0;step<8;++step){auto before=state;expected=std::clamp(expected-direction,0,5);wheel(vcoGrid::wave(o),direction);before[24+o]=expected/5.;check(state==before,"Analytic wheel skipped/reordered/crossed source family");}}
        }
        expectedVcoLabels.clear();popupChoice=-999;fieldClick(vcoGrid::sourceMode(o));check(state[oscParam(o,0)]==.5,"Default explicit TABLE selection");expectedVcoLabels={"ROM","USER"};
        for(int chosen:{0,1}){popupChoice=chosen+1;fieldClick(vcoGrid::source(o));check(state[oscParam(o,0)]==(chosen+1)/2.,"Table source menu mapping");for(int i=0;i<5;++i)wheel(vcoGrid::source(o),1);check(state[oscParam(o,0)]==.5,"Table source wheel crossed to OSC");}
        expectedVcoLabels.clear();popupChoice=-999;state[oscParam(o,0)]=.5;state[oscParam(o,1)]=0;for(size_t i=1;i<romOrder.size();++i)wheel(vcoGrid::table(o),-1);check(discrete(oscParam(o,1),state[oscParam(o,1)])==table::factoryBank().tables[romOrder.back()].id&&state[oscParam(o,0)]==.5,"ROM wheel down did not traverse full table bank");for(size_t i=0;i<romOrder.size()+4;++i)wheel(vcoGrid::table(o),1);check(state[oscParam(o,1)]==0&&state[oscParam(o,0)]==.5,"ROM wheel up clamp/source mismatch");
        // Every ROM/USER item follows the list's spatial order. Other wheel
        // controls use the same up=previous/down=next displayed-list convention.
        for(int source:{1,2})for(int start=0;start<(source==1?int(table::factoryBank().tables.size()):16);++start)for(int direction:{1,-1}){
          state[oscParam(o,0)]=source/2.;int id=oscParam(o,source==1?1:2),count=source==1?int(table::factoryBank().tables.size()):16;
          int identity=source==1?table::factoryBank().tables[romOrder[start]].id:start;state[id]=identity/double(source==1?65535:15);auto saved=state;
          wheel(vcoGrid::table(o),direction);int expected=std::clamp(start-direction,0,count-1);saved[id]=(source==1?table::factoryBank().tables[romOrder[expected]].id:expected)/double(source==1?65535:15);
          check(state==saved,"Table-list wheel changed order or unrelated state");
        }
        expectedVcoLabels.clear();for(int index:romOrder)expectedVcoLabels.emplace_back(table::factoryBank().tables[index].name.data());
        for(int chosen=0;chosen<int(romOrder.size());++chosen){state[oscParam(o,0)]=.5;auto saved=state;popupChoice=chosen+1;fieldClick(vcoGrid::table(o));saved[oscParam(o,1)]=table::factoryBank().tables[romOrder[chosen]].id/65535.;check(state==saved,"ROM menu identity/state mismatch");}
        expectedVcoLabels.clear();popupChoice=-999;
        state[oscParam(o,0)]=.5;auto tableField=vcoGrid::table(o);int tx=int((tableField.x+tableField.w/2)*scale),ty=int((tableField.y+48+8)*scale);
        for(bool rapid:{false,true}){
          popupChoice=5;int calls=popupCalls;auto saved=state;
          SendMessageW(w,rapid?WM_LBUTTONDBLCLK:WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(tx,ty));
          check(popupCalls==calls&&state==saved&&GetCapture()==w,"Table menu opened while opening button held");
          SendMessageW(w,WM_LBUTTONUP,0,MAKELPARAM(tx,ty));saved[oscParam(o,1)]=table::factoryBank().tables[romOrder[4]].id/65535.;
          check(popupCalls==calls+1&&state==saved&&GetCapture()!=w,"Completed table click did not open/choose/release capture");
        }
        for(int cancel:{0,1,2,3}){
          popupChoice=0;int calls=popupCalls;auto saved=state;SendMessageW(w,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(tx,ty));
          if(cancel==1)SendMessageW(w,WM_CANCELMODE,0,0);if(cancel==2)ReleaseCapture();
          SendMessageW(w,WM_LBUTTONUP,0,cancel==3?MAKELPARAM(950*percent/100,400*percent/100):MAKELPARAM(tx,ty));
          check(state==saved&&GetCapture()!=w&&popupCalls==calls+(cancel==0?1:0),"Table dismissal/aborted opening changed state or leaked capture");
        }
        popupChoice=-999;
      }
    }
    // Both waveform browsers use the same consolidated taxonomy, with ROM
    // identities unchanged and USER remaining its separate import/storage bank.
    editor.resize(NativeWidth,568);
    int programmerChecks=0;const int categoryCounts[]={9,3,3,5,6,13,11,14,5};
    for(int o=0;o<2;++o)for(const auto&t:table::factoryBank().tables){
      state=defaults();state[oscParam(o,0)]=.5;state[oscParam(o,1)]=t.id/65535.;auto b=vcoGrid::pgm(o);
      click(w,b.x+b.w/2,b.y+48+8);pump();auto pgm=FindWindowW(L"Xsynth2000WaveProgrammer",nullptr);check(pgm!=nullptr,"Programmer missing");
      auto bank=GetDlgItem(pgm,2),list=GetDlgItem(pgm,3);int group=romMenu::category(t);
      check(SendMessageW(bank,CB_GETCOUNT,0,0)==10&&SendMessageW(bank,CB_GETCURSEL,0,0)==group,"Programmer category selection/inventory mismatch");
      char name[96]{};SendMessageA(bank,CB_GETLBTEXT,group,LPARAM(name));check(std::string(name)==romMenu::CategoryNames[group],"Programmer category caption mismatch");
      check(SendMessageW(list,CB_GETCOUNT,0,0)==categoryCounts[group],"Programmer category table count mismatch");
      SendMessageA(list,CB_GETLBTEXT,SendMessageW(list,CB_GETCURSEL,0,0),LPARAM(name));check(std::string(name)==t.name.data(),"Programmer selected wrong ROM identity");++programmerChecks;
    }
    std::cout<<"PASS "<<programmerChecks<<" programmer category/identity checks across both VCOs\n";
    // Destination-first workflow: actual painted text changes with Source OFF,
    // survives state roundtrip, and is identical after enabling that source.
    int pendingDestinationChecks=0;
    for(int percent:{100,200}){double scale=percent/100.;int width=NativeWidth*percent/100,height=568*percent/100;editor.resize(width,height);
      for(int slot=0;slot<MatrixSlots;++slot){state=defaults();auto c=matrixControls()[slot*3+1];
        auto crop=[&](){auto image=capture(w,width,height);std::vector<uint32_t> result;auto b=fieldBounds(c);
          for(int y=int((b.y+48+2)*scale);y<int((b.y+48+b.h-2)*scale);++y)for(int x=int((b.x+3)*scale);x<int((b.x+b.w-15)*scale);++x)result.push_back(image[y*width+x]);return result;};
        auto before=crop();expectedDestination=routeDestination(state,slot);expectedDestinationOff=false;popupChoice=9;
        click(w,int((c.x+c.w/2)*scale),int((c.y+48+8)*scale));pump();popupChoice=-999;expectedDestination=-1;
        check(effectiveSource(state,slot)==0&&routeDestination(state,slot)==8,"Destination-first selection enabled source or lost target");
        auto off=crop();check(before!=off,"Destination-first painted label stayed OFF/default");
        State restored;auto bytes=encode(state);check(decode(bytes.data(),bytes.size(),restored)&&restored==state,"Inactive destination did not survive state roundtrip");
        state[matrixId(slot)]=.1;check(crop()==off,"Destination label depends on source enablement");++pendingDestinationChecks;
      }
    }
    std::cout<<"PASS "<<pendingDestinationChecks<<" destination-first painted labels and saved targets; source remains OFF until explicitly selected\n";
    if(menusOnly){std::cout<<"PASS unified ROM folders, all menu identities/wheels and destination-first workflow\n";editor.close();DestroyWindow(parent);return 0;}
    pointerScale=1;editor.resize(xs::NativeWidth,568);auto heldState=state;popupChoice=0;fixtureHeld=true;SendMessageW(w,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(70,moduleGrid::FirstSelectors+48+8));check(GetCapture()==w,"Dismissal did not own held release");SendMessageW(w,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(684,247));fixtureHeld=false;SendMessageW(w,WM_LBUTTONUP,0,MAKELPARAM(684,247));popupChoice=-999;check(state==heldState&&GetCapture()!=w,"Held dismissal clicked through or retained capture");
    editor.resize(xs::NativeWidth, 568);
    editor.setTheme(theme::Id::Steel);
    // The complete preset field, including its new gutter, remains one target.
    for(int x:{ui::PresetField.x+3,ui::PresetField.x+ui::PresetField.w-2}){popupChoice=0;int before=popupCalls;auto saved=state;click(w,x,ui::PresetField.y+ui::PresetField.h/2);check(popupCalls==before+1&&state==saved,"Preset gutter/text hit mismatch");popupChoice=-999;}
    // Matrix dropdown opening belongs to completed clicks, not button holds.
    state=defaults();popupChoice=0;int menuCount=popupCalls;
    SendMessageW(w,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(60,MatrixFirstRow+48+9));
    check(popupCalls==menuCount,"Matrix menu opened while opening button held");
    SendMessageW(w,WM_LBUTTONUP,0,MAKELPARAM(60,MatrixFirstRow+48+9));
    check(popupCalls==menuCount+1,"Matrix menu did not persist/open on completed click");popupChoice=-999;
    // Finite EG fields retain only legal states, including Shift+wheel. Time
    // knobs remain continuous, with useful finer movement under Shift.
    for(int percent:{100,150,200}){
      const double scale=percent/100.;editor.resize(xs::NativeWidth*percent/100,568*percent/100);
      for(int id:{Curve1,Curve2}){
        const Control*field=nullptr;for(const auto&c:mainControls)if(c.id==id)field=&c;
        check(field!=nullptr,"Finite EG field missing");
        for(bool fine:{false,true})for(int start=0;start<3;++start)for(int direction:{-1,1}){
          state[id]=start/2.;POINT pt{int((field->x+field->w/2)*scale),int((field->y+48+8)*scale)};ClientToScreen(w,&pt);
          SendMessageW(w,WM_MOUSEWHEEL,MAKEWPARAM(fine?MK_SHIFT:0,short(direction*120)),MAKELPARAM(pt.x,pt.y));
          check(state[id]==std::clamp(start-direction,0,2)/2.,"EG Shift/wheel created fractional or wrong finite state");
        }
        // Discrete fields use their existing click/menu grammar, never a drag
        // continuum. A canceled opening and Shift motion must not edit them.
        popupChoice=0;state[id]=.5;int px=int((field->x+field->w/2)*scale),py=int((field->y+48+8)*scale);
        SendMessageW(w,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(px,py));SendMessageW(w,WM_MOUSEMOVE,MK_LBUTTON|MK_SHIFT,MAKELPARAM(px,py-30));SendMessageW(w,WM_LBUTTONUP,0,MAKELPARAM(px,py-30));
        check(state[id]==0||state[id]==.5||state[id]==1,"Finite EG dragging produced intermediate state");popupChoice=-999;
      }
      for(int id:{9,10,12,15,16,18,13,14,19,20}){
        const Control*knob=nullptr;for(const auto&c:mainControls)if(c.id==id)knob=&c;
        int px=int((knob->x+15)*scale),py=int((knob->y+15+48)*scale);double values[2]{};
        for(int fine=0;fine<2;++fine){state[id]=.5;SendMessageW(w,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(px,py));SendMessageW(w,WM_MOUSEMOVE,MK_LBUTTON|(fine?MK_SHIFT:0),MAKELPARAM(px,py-15));values[fine]=state[id];SendMessageW(w,WM_LBUTTONUP,0,MAKELPARAM(px,py-15));}
        check(std::abs((values[0]-.5)-10*(values[1]-.5))<1e-10&&values[1]>.5,"Continuous EG Shift fine drag/detent regression");
      }
      for(int lfo=0;lfo<4;++lfo){auto tab=lfoTabs::bounds(lfo);
        click(w,int((tab.x+tab.w/2)*scale),int((tab.y+48+tab.h/2)*scale));
        for(int field:{1,2}){int id=lfoParam(lfo,field);const Control*knob=nullptr;
          for(const auto&c:mainControls)if(c.id==LfoBaseIds[field])knob=&c;
          check(knob!=nullptr,"LFO depth control missing");
          int px=int((knob->x+15)*scale),py=int((knob->y+15+48)*scale);double values[2]{};
          for(int fine=0;fine<2;++fine){state[id]=.513579;auto saved=state;
            SendMessageW(w,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(px,py));
            check(state==saved,"LFO depth mouse-down rounded a loaded fine value");
            SendMessageW(w,WM_MOUSEMOVE,MK_LBUTTON|(fine?MK_SHIFT:0),MAKELPARAM(px,py-15));values[fine]=state[id];
            SendMessageW(w,WM_LBUTTONUP,0,MAKELPARAM(px,py-15));
            for(int p=0;p<Count;++p)if(p!=id)check(state[p]==saved[p],"LFO depth edit changed another parameter");
          }
          check(std::abs((values[0]-.513579)-10*(values[1]-.513579))<1e-10&&values[1]>.513579,"LFO depth Shift sensitivity/selection mismatch");
        }
      }
    }
    editor.resize(xs::NativeWidth,568);
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
      auto shown=capture(w,xs::NativeWidth,568);check((shown[(ay-5)*xs::NativeWidth+amountControl.x+3]&0xffffff)==0xf5f5eb,"Amount field is not pale/opaque");
      int black=0;for(int y=amountControl.y+49;y<amountControl.y+63;++y)for(int x=amountControl.x+3;x<amountControl.x+amountControl.w-3;++x)black+=(shown[y*xs::NativeWidth+x]&0xffffff)==0;
      check(black>10,"Amount black text missing");
    }
    editor.setTheme(theme::Id::Steel);
    // Filename is authoritative even when the saved sound exactly equals a factory.
    auto saveNamed = [&](const wchar_t *file, const char *label) {
      DialogRun run;
      run.operation = 2;
      run.path = (art / file).wstring();
      fileAction(instance, run);
      verifyHeaderText(capture(w, xs::NativeWidth, 568), xs::NativeWidth, 568, 1, theme::Id::Steel, label);
      DialogRun load;
      load.path = run.path;
      fileAction(instance, load);
      verifyHeaderText(capture(w, xs::NativeWidth, 568), xs::NativeWidth, 568, 1, theme::Id::Steel, label);
    };
    factory(6);
    saveNamed(L"My Strings.XseDev", "My Strings");
    fs::copy_file(art / "My Strings.XseDev", art / "Renamed Strings.XseDev",
                  fs::copy_options::overwrite_existing);
    DialogRun rename;
    rename.path = (art / "Renamed Strings.XseDev").wstring();
    fileAction(instance, rename);
    verifyHeaderText(capture(w, xs::NativeWidth, 568), xs::NativeWidth, 568, 1, theme::Id::Steel, "Renamed Strings");
    saveNamed(L"Brass \u00e9.XseDev",u8"Brass \u00e9");
    DialogRun import;
    import.operation = 1;
    import.path = fs::absolute("tests/dssi/banks/XsynthBank1").wstring();
    import.expectedCount = 60;
    import.selected = 11;
    fileAction(instance, import);
    saveNamed(L"Converted Lead.XseDev", "Converted Lead");
    auto savedLabel=capture(w,xs::NativeWidth,568);DialogRun cancel;cancel.operation=2;cancel.cancel=true;cancel.path=(art/"Cancelled.XseDev").wstring();fileAction(instance,cancel);check(capture(w,xs::NativeWidth,568)==savedLabel,"Cancelled save changed display name");
    fs::create_directories(art/"Unwritable.XseDev");DialogRun fail;fail.operation=2;fail.path=(art/"Unwritable.XseDev").wstring();expectFileError=true;fileAction(instance,fail);expectFileError=false;check(fileErrors==1&&capture(w,xs::NativeWidth,568)==savedLabel,"Failed save changed name or did not report failure");
    click(w, ui::PresetNext.x + 12, 28);
    check(factory::sameSound(state, init), "External next did not go INIT");
    click(w, ui::PresetNext.x + 12, 28, true);
    check(factory::sameSound(state, factory::presetState(0)), "First factory after INIT");
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
