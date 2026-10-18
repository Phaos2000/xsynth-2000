// GPL-2.0-or-later. Native editor routing and render isolation checks.
#include "Editor.h"
#include "Layout.h"
#include <iostream>
#include <stdexcept>
#include <vector>
#include <fstream>

void check(bool value, const char *message) {
  if (!value) throw std::runtime_error(message);
}
std::vector<uint32_t> capture(HWND window, int width, int height) {
  BITMAPINFO info{};
  info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  info.bmiHeader.biWidth = width;
  info.bmiHeader.biHeight = -height;
  info.bmiHeader.biPlanes = 1;
  info.bmiHeader.biBitCount = 32;
  HDC dc = CreateCompatibleDC(nullptr);
  void *data{};
  auto bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &data, nullptr, 0);
  check(dc && bitmap && data, "Capture allocation failed");
  auto old = SelectObject(dc, bitmap);
  SendMessageW(window, WM_PRINTCLIENT, reinterpret_cast<WPARAM>(dc), PRF_CLIENT);
  GdiFlush();
  auto pixels = static_cast<uint32_t *>(data);
  std::vector<uint32_t> result(pixels, pixels + width * height);
  SelectObject(dc, old);
  DeleteObject(bitmap);
  DeleteDC(dc);
  return result;
}
// Shared production geometry is inspected here; no separate screenshot layout.
const xs::Control& control(int id) {
 for(const auto& c:xs::mainControls)if(c.id==id)return c;
 throw std::runtime_error("Missing layout control");
}
void geometryChecks(const char* csvPath) {
 using namespace xs;
 auto same=[](Bounds a,Bounds b,int dx=0){return a.x+dx==b.x&&a.y==b.y&&a.w==b.w&&a.h==b.h;};
 const int first[]={9,10,11,12,Velocity1,13,14},second[]={15,16,17,18,Velocity2,19,20};
 for(int i=0;i<7;++i){const auto&a=control(first[i]);const auto&b=control(second[i]);
  check(a.x+240==b.x&&a.y==b.y&&a.valueWidth==b.valueWidth,"EG knob templates differ");
  check(same(fieldBounds(a),fieldBounds(b),240)&&same(labelBounds(a),labelBounds(b),240),"EG field/label templates differ");}
 for(int id:std::initializer_list<int>{14,20,Voices,Unison,LfoPhase,LfoPolarity}){
  check(fieldBounds(control(id)).y+faceGrid::HeaderHeight==355,"Middle shared outer top drift");
  check(labelBounds(control(id)).y+faceGrid::HeaderHeight==341,"Middle shared label row drift");}
 for(int column=0;column<3;++column){int ids[]={8,Detune,MasterTune};auto expected=moduleGrid::knob(720,3,column,moduleGrid::TopKnobs,ids[column],"");const auto &c=control(ids[column]);check(c.x==expected.x&&c.y==expected.y&&c.valueWidth==expected.valueWidth,"VOICE three-knob shared grid");}
 for(int column=0;column<2;++column){int ids[]={GlideMode,BendRange};auto expected=moduleGrid::pair(720,column,moduleGrid::LastSelectors,ids[column],"");const auto &c=control(ids[column]);check(c.x==expected.x&&c.y==expected.y&&c.w==expected.w,"VOICE lower paired grid");}
 check(control(Curve1).x*2+control(Curve1).w==720,"EG1 Curve is not centered on x=360");
 check(labelBounds(control(Curve1)).x*2+labelBounds(control(Curve1)).w==720,"EG1 Curve label not centered");
 for(int left:std::initializer_list<int>{26,LfoPhase,Curve2,Voices}){
  int right=left==26?LfoClock:left==LfoPhase?LfoPolarity:left==Curve2?Loop2:Unison;
  const auto&a=control(left);const auto&b=control(right);int origin=(a.x/240)*240;
  check(a.x-origin==12&&origin+240-b.x-b.w==12&&b.x-a.x-a.w==12,"Paired margins or central gap drift");
  check(a.w==102&&b.w==102&&a.y==b.y,"Paired selector geometry drift");
  check(labelBounds(a).y==labelBounds(b).y,"Paired label baseline drift");}
 for(int id:{VoiceMode})check(control(id).x==732&&control(id).w==216,"Full VOICE margins drift");
 for(int i=0;i<6;++i){auto p=panels[i];auto h=faceGrid::header(p);check(p.y==0&&p.h==154&&h.y==4&&h.h==14,"Top panel shell/header drift");if(i<5)check(p.x+p.w==panels[i+1].x,"Top module gutter drift");}
 for(int i=6;i<10;++i)check(panels[i].x==(i-6)*240&&panels[i].y==154&&panels[i].w==240&&panels[i].h==216,"Middle panel geometry drift");
 for(auto&c:mainControls)if(c.y<154){
  if(c.kind==ControlKind::Knob){check(c.y+15+48==93&&fieldBounds(c).y+48==127&&fieldBounds(c).h==12,"Top knob/value row drift");check(labelBounds(c).y+48==113,"Top knob label drift");}
  if(c.kind==ControlKind::Selector)check(c.y+48==162&&fieldBounds(c).h==18&&labelBounds(c).y+48==147,"Top selector row drift");}
 for(int i=0;i<2;++i){int origin=i?308:0;for(int col=0;col<2;++col){auto&k=control(i*2+col);auto&s=control(col?Engine1+i:24+i);check(2*(k.x+15)==2*s.x+s.w,"Oscillator knob/selector column centers differ");}check(control(i*2).x+15+control(i*2+1).x+15==2*(origin+85),"Oscillator group off center");}
 check(control(7).x+15==530,"MIXER knob off center");
 check(control(21).x+15==624&&control(22).x+15==708&&control(Keytrack).x+15==792,"VCF row off center");
 auto routes=matrixControls();
 check(matrixGrid::LabelY==389&&MatrixFirstRow==404,"Shared matrix spacing revision drift");
 check(matrixGrid::LabelY==faceGrid::header(panels[10]).y+faceGrid::HeaderH+matrixGrid::HeaderClearance,"Matrix headings do not clear title");
 for(int row=0;row<6;++row)for(int column=0;column<3;++column){auto a=fieldBounds(routes[row*3+column]),b=fieldBounds(routes[(row+6)*3+column]);check(same(a,b,480),"Matrix halves differ");check(a.y+48==452+row*19&&a.h==18,"Matrix row spacing drift");check(a.x==matrixGrid::FieldX[column]&&a.w==matrixGrid::FieldWidth[column],"Matrix horizontal geometry drift");check(a.y+a.h+48<=565,"Matrix bottom padding lost");}
 HDC dc=CreateCompatibleDC(nullptr);HFONT font=CreateFontW(-10,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,NONANTIALIASED_QUALITY,DEFAULT_PITCH,L"Tahoma");auto previous=SelectObject(dc,font);TEXTMETRIC tm{};GetTextMetrics(dc,&tm);
 auto baseline=[&](Bounds r){return r.y+faceGrid::HeaderHeight+(r.h-tm.tmHeight)/2+tm.tmAscent;};
 std::cout<<"GEOMETRY: native Tahoma height="<<tm.tmHeight<<" ascent="<<tm.tmAscent<<"; EG Filter Depth/Voices outer tops=355, label baseline="<<baseline(labelBounds(control(14)))<<"\n";
 check(baseline(labelBounds(control(14)))==baseline(labelBounds(control(Voices))),"Label baseline mismatch");
 if(csvPath){std::ofstream csv(csvPath);csv<<"id,label,knob_center_x,knob_center_y,field_x,field_y,field_w,field_h,label_x,label_y,label_w,label_h,label_baseline\n";
  auto emit=[&](const Control&c){bool main=false;for(const auto &m:mainControls)main|=m.id==c.id;auto f=fieldBounds(c);auto l=main?labelBounds(c):Bounds{-1,-49,-1,-1};csv<<c.id<<','<<c.label<<','<<(c.kind==ControlKind::Knob?c.x+15:-1)<<','<<(c.kind==ControlKind::Knob?c.y+15+48:-1)<<','<<f.x<<','<<f.y+48<<','<<f.w<<','<<f.h<<','<<l.x<<','<<l.y+48<<','<<l.w<<','<<l.h<<','<<(main?baseline(l):-1)<<'\n';};for(auto&c:mainControls)emit(c);for(auto&c:routes)emit(c);check(bool(csv),"Geometry CSV write failed");}
 SelectObject(dc,previous);DeleteObject(font);DeleteDC(dc);
 std::cout<<"PASS: exact shell, knob, label, field, paired/centered selector and mirrored matrix geometry\n";
}

int main(int argc,char** argv) {
  try {
    geometryChecks(argc>1?argv[1]:nullptr);
    auto parent = CreateWindowW(L"STATIC", L"Xsynth editor test", WS_OVERLAPPED,
                                0, 0, 1200, 1300, nullptr, nullptr, nullptr, nullptr);
    check(parent != nullptr, "Test parent creation failed");
    xs::State state = xs::defaults();
    const auto original = state;
    int gestures = 0, edits = 0;
    xs::Editor a(nullptr), b(nullptr);
    for (auto *editor : {&a, &b}) {
      editor->read = [&] { return state; };
      editor->begin = editor->end = [&](int) { ++gestures; };
      editor->change = [&](int id, double value) { ++edits; state[id] = value; };
    }
    auto first = a.open(parent), second = b.open(parent);
    check(first && second, "Editor creation failed");
    for (int percent : {100, 125, 150, 200}) {
      const double scale = percent / 100.;
      const int width = xs::NativeWidth * percent / 100, height = xs::NativeHeight * percent / 100;
      a.resize(width, height);
      b.resize(width, height);
      auto point = [&](int x, int y) { return MAKELPARAM(int(x * scale), int(y * scale)); };
      auto click = [&](int x, int y) {
        SendMessageW(first, WM_LBUTTONDOWN, MK_LBUTTON, point(x, y));
        SendMessageW(first, WM_LBUTTONUP, 0, point(x, y));
      };
      const auto closed = capture(first, width, height);
      for(int id:std::initializer_list<int>{14,20,xs::Voices,xs::Unison}) {
        const auto field=xs::fieldBounds(control(id));
        int px=int(std::round((field.x+2)*scale)),py=int(std::round(355*scale));
        check((closed[py*width+px]&0xffffff)==(control(id).kind==xs::ControlKind::Knob?0x041216:0xa5c3c2),"Rendered shared-band outer top drift");
      }
      check(closed == capture(second, width, height), "Instances initially differ");
      click(60, 24);
      const auto opened = capture(first, width, height);
      check(opened != closed, "Logo did not open credits");
      check(closed == capture(second, width, height), "Credits leaked to another instance");
      for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x)
          if (x < int(std::round(262 * scale)) || x >= int(std::round(698 * scale)) ||
              y < int(std::round(100 * scale)) || y >= int(std::round(516 * scale)))
            check(closed[y * width + x] == opened[y * width + x], "Overlay changed exterior pixels");
      click(280,347); // Anywhere on the panel dismisses without editing below.
      check(capture(first,width,height)==closed,"Panel click did not dismiss");
      check(gestures==0&&edits==0&&state==original,"Dismissal clicked through to synth");
      click(60,24);check(capture(first,width,height)==opened,"Logo did not reopen About");
      click(60,24);check(capture(first,width,height)==closed,"Logo did not dismiss About");
      click(60, 24);
      click(670, 120);
      check(capture(first, width, height) == closed, "Former close-control region did not dismiss");
      click(60, 24);
      click(39, 95); // Outside the panel, over the oscillator pitch knob.
      check(capture(first, width, height) == closed, "Outside click failed to dismiss");
      check(gestures == 0 && edits == 0 && state == original, "Dismissal clicked through");
      std::cout << "PASS: credits render, isolation and input routing at " << percent << "%\n";
    }
    SendMessageW(first, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(120, 48));
    SendMessageW(first, WM_LBUTTONUP, 0, MAKELPARAM(120, 48));
    a.resize(625, 453);
    b.resize(625, 453);
    check(capture(first, 625, 453) != capture(second, 625, 453), "Resize lost open panel");
    a.close();
    first = a.open(parent, 625, 453);
    check(capture(first, 625, 453) == capture(second, 625, 453), "Credits persisted across editor close");
    SendMessageW(first, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(25, 61));
    SendMessageW(first, WM_LBUTTONUP, 0, MAKELPARAM(25, 61));
    check(gestures == 2, "Normal knob interaction failed after dismissing credits");
    a.resize(xs::NativeWidth,xs::NativeHeight);
    state[xs::MatrixFirst]=.1;
    const int amount=xs::MatrixFirst+2;
    const int amountY=xs::MatrixFirstRow+48+9;
    SendMessageW(first,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(400,amountY));
    SendMessageW(first,WM_MOUSEMOVE,MK_LBUTTON,MAKELPARAM(420,amountY));
    SendMessageW(first,WM_LBUTTONUP,0,MAKELPARAM(420,amountY));
    check(std::abs(state[amount]-.6)<1.e-10,"Bipolar amount did not drag horizontally");
    SendMessageW(first,WM_LBUTTONDBLCLK,MK_LBUTTON,MAKELPARAM(400,amountY));
    check(state[amount]==.5,"Bipolar amount did not reset to zero");
    SendMessageW(first,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(280,93));
    SendMessageW(first,WM_LBUTTONUP,0,MAKELPARAM(280,93));
    check(state[27]==1 && GetCapture()!=first,"Hard Sync button failed or captured a knob drag");
    SendMessageW(first,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(280,93));
    SendMessageW(first,WM_LBUTTONUP,0,MAKELPARAM(280,93));
    check(state[27]==0,"Hard Sync button did not toggle off");
    state[amount]=.6;
    const auto beforeHover=state;
    const int beforeGestures=gestures,beforeEdits=edits;
    const auto unhighlighted=capture(first,xs::NativeWidth,xs::NativeHeight);
    POINT marker{-1,-1};
    for(int y=115;y<125;++y)for(int x=46;x<66;++x)
      if((unhighlighted[y*xs::NativeWidth+x]&0xffffff)==0x42b8d0)marker={x,y};
    check(marker.x>=0,"Active modulation marker missing");
    SendMessageW(first,WM_MOUSEMOVE,0,MAKELPARAM(marker.x,marker.y));
    const auto highlighted=capture(first,xs::NativeWidth,xs::NativeHeight);
    check(highlighted!=unhighlighted,"Marker hover did not highlight its route");
    for(int y=0;y<xs::NativeHeight;++y)for(int x=0;x<xs::NativeWidth;++x)
      if(highlighted[y*xs::NativeWidth+x]!=unhighlighted[y*xs::NativeWidth+x])
        check(x>=28 && x<=470 && y>=xs::MatrixFirstRow+46 && y<=xs::MatrixFirstRow+72,
              "Marker hover changed pixels outside its matrix row");
    SendMessageW(first,WM_MOUSELEAVE,0,0);
    check(capture(first,xs::NativeWidth,xs::NativeHeight)==unhighlighted,"Hover exit did not restore GUI exactly");
    check(state==beforeHover && gestures==beforeGestures && edits==beforeEdits,"Hover edited a parameter");
    std::cout << "PASS: modulation marker highlights only its route; leave restores pixels without edits\n";
    a.resize(xs::NativeWidth,xs::NativeHeight);
    // Compare each actual readout's glyph mask with a complete, unclipped Tahoma
    // rendering of its expected semitone text, through all six materials.
    HDC maskDC=CreateCompatibleDC(nullptr);BITMAPINFO maskInfo{};
    maskInfo.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);maskInfo.bmiHeader.biWidth=54;maskInfo.bmiHeader.biHeight=-12;
    maskInfo.bmiHeader.biPlanes=1;maskInfo.bmiHeader.biBitCount=32;void* maskData{};
    auto maskBitmap=CreateDIBSection(maskDC,&maskInfo,DIB_RGB_COLORS,&maskData,nullptr,0);auto maskOld=SelectObject(maskDC,maskBitmap);
    auto pitchFont=CreateFontW(-10,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,NONANTIALIASED_QUALITY,DEFAULT_PITCH,L"Tahoma");
    auto fontOld=SelectObject(maskDC,pitchFont);SetBkMode(maskDC,TRANSPARENT);SetTextColor(maskDC,RGB(255,255,255));
    const char* pitchText[]={"-24.0 st","-12.0 st","+0.0 st","+12.0 st","+24.0 st"};
    for(unsigned theme=0;theme<xs::theme::Count;++theme){a.setTheme(xs::theme::Id(theme));
      for(int k=0;k<5;++k){state[0]=state[2]=k/4.;auto image=capture(first,xs::NativeWidth,xs::NativeHeight);
        PatBlt(maskDC,0,0,54,12,BLACKNESS);RECT rect{0,0,54,12};SIZE extent{};GetTextExtentPoint32A(maskDC,pitchText[k],int(std::strlen(pitchText[k])),&extent);
        check(extent.cx<=52&&extent.cy<=12,"Extended Pitch text would clip");DrawTextA(maskDC,pitchText[k],-1,&rect,DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_NOPREFIX);GdiFlush();
        for(int id:{0,2}){auto box=xs::fieldBounds(control(id));for(int y=0;y<12;++y)for(int x=0;x<54;++x)
          check(((image[(box.y+48+y)*xs::NativeWidth+box.x+x]&0xffffff)==xs::theme::style(xs::theme::Id(theme)).readoutText)==((static_cast<uint32_t*>(maskData)[y*54+x]&0xffffff)==0xffffff),"Extended Pitch readout glyph mismatch");}
      }
    }
    SelectObject(maskDC,fontOld);DeleteObject(pitchFont);SelectObject(maskDC,maskOld);DeleteObject(maskBitmap);DeleteDC(maskDC);
    std::cout<<"PASS: -24/-12/0/+12/+24 semitone readouts complete and unclipped in all six themes\n";
    // Real editor events: coarse manual Pitch steps; all other paths retain
    // fractional normalized values. Exercise each pixel, direction and DPI.
    for(int percent : {100,125,150,200}) {
      double scale=percent/100.;a.resize(960*percent/100,568*percent/100);
      for(int id : {0,2}) {
        auto c=control(id);int x=int((c.x+15)*scale),y=int((c.y+15+48)*scale);
        auto move=[&](int dy,WPARAM flags){SendMessageW(first,WM_MOUSEMOVE,flags,MAKELPARAM(x,y-dy));};
        for(bool upward : {true,false}) {
          state[id]=upward?0.:1.;double last=state[id];int visited=1;
          SendMessageW(first,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(x,y));
          for(int pixel=0;pixel<=int(std::ceil(150*scale))+2;++pixel) {
            move(upward?pixel:-pixel,MK_LBUTTON);
            double now=state[id];
            check(std::abs(now*48-std::round(now*48))<1.e-10,"Pitch coarse drag not semitone");
            check(upward?now>=last:now<=last,"Pitch drag jitter");
            if(now!=last){check(std::abs(std::abs(now-last)-1./48)<1.e-10,"Pitch skipped semitone");++visited;}
            last=now;
          }
          SendMessageW(first,WM_LBUTTONUP,0,MAKELPARAM(x,y));
          check(visited==49&&state[id]==(upward?1.:0.),"Pitch full-range drag missed steps");
        }
        for(double n : {0.,.25,.5,.75,1.}) {
          state[id]=n;SendMessageW(first,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(x,y));
          move(0,MK_LBUTTON);SendMessageW(first,WM_LBUTTONUP,0,MAKELPARAM(x,y));
          check(state[id]==n,"Pitch octave anchor changed");
        }
        state[id]=.513579;
        auto bytes=xs::encode(state);xs::State loaded{};
        check(xs::decode(bytes.data(),bytes.size(),loaded)&&loaded[id]==state[id],"Fine pitch state rounded");
        capture(first,960*percent/100,568*percent/100);
        check(state[id]==.513579,"Rendering rounded loaded fine pitch");
        SendMessageW(first,WM_LBUTTONDOWN,MK_LBUTTON|MK_SHIFT,MAKELPARAM(x,y));
        check(state[id]==.513579,"Mouse down rounded loaded fine pitch");
        move(1,MK_LBUTTON|MK_SHIFT);
        check(std::abs(state[id]-(.513579+1/(1500*scale)))<1.e-12,"Shift fine drag changed sensitivity");
        check(std::abs(state[id]*48-std::round(state[id]*48))>.001,"Shift pitch snapped");
        SendMessageW(first,WM_LBUTTONUP,0,MAKELPARAM(x,y));
        state[id]=.513579;SendMessageW(first,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(x,y));
        move(1,MK_LBUTTON);check(std::abs(state[id]*48-std::round(state[id]*48))<1.e-10,"Fine pitch did not snap on coarse edit");
        SendMessageW(first,WM_LBUTTONUP,0,MAKELPARAM(x,y));
        check(GetCapture()!=first,"Pitch drag leaked capture");
      }
    }
    std::cout<<"PASS: VCO1/VCO2 coarse 49-semitone sweeps both directions, octave anchors, fine state and Shift continuous at 4 scales\n";
    a.close(); b.close();
    DestroyWindow(parent);
    std::cout << "PASS: resize/reopen and normal knob input; no browser launched\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}



