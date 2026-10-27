// GPL-2.0-or-later. Native editor regression for approved tabs and pitch wheel.
#include "Editor.h"
#include "Layout.h"
#include <iostream>
#include <stdexcept>
void check(bool b,const char*m){if(!b)throw std::runtime_error(m);}
int main(){try{using namespace xs;State state=defaults();int edits=0;Editor editor(nullptr);editor.read=[&]{return state;};editor.change=[&](int id,double value){state[id]=value;++edits;};editor.begin=editor.end=[](int){};
 HWND parent=CreateWindowW(L"STATIC",L"Four LFO test",WS_POPUP,0,0,1920,1136,nullptr,nullptr,nullptr,nullptr);HWND w=editor.open(parent);
 for(int scalePercent:{100,125,150,200}){double scale=scalePercent/100.;editor.resize(NativeWidth*scalePercent/100,NativeHeight*scalePercent/100);
  auto click=[&](int x,int y){auto p=MAKELPARAM(int(std::round(x*scale)),int(std::round(y*scale)));SendMessageW(w,WM_LBUTTONDOWN,MK_LBUTTON,p);SendMessageW(w,WM_LBUTTONUP,0,p);};
  for(unsigned theme=0;theme<theme::Count;++theme){editor.setTheme(theme::Id(theme));
   for(int lfo=0;lfo<4;++lfo){auto b=lfoTabs::bounds(lfo);check(b.w==18&&b.h==18&&b.y==moduleGrid::LastSelectors,"Tab shape/baseline");check(lfoTabs::bounds(0).x+lfoTabs::bounds(3).x+18==224,"Tab group centering");auto before=state;int count=edits;click(b.x+9,b.y+48+9);check(state==before&&count==edits,"Tab switch edited synthesis state");
    int rate=lfoParam(lfo,0);state[rate]=.2;state[lfoParam(lfo,4)]=0;
    auto previous=state;SendMessageW(w,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(int(36*scale),int(247*scale)));SendMessageW(w,WM_MOUSEMOVE,MK_LBUTTON,MAKELPARAM(int(36*scale),int(227*scale)));SendMessageW(w,WM_LBUTTONUP,0,MAKELPARAM(int(36*scale),int(227*scale)));check(state[rate]>.2,"Tab did not redirect Rate");previous[rate]=state[rate];check(previous==state,"Hidden LFO state overwritten");
    state[lfoParam(lfo,4)]=1;int division=lfoParam(lfo,8);state[division]=.3;previous=state;double value=state[division];SendMessageW(w,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(int(36*scale),int(247*scale)));SendMessageW(w,WM_MOUSEMOVE,MK_LBUTTON,MAKELPARAM(int(36*scale),int(217*scale)));SendMessageW(w,WM_LBUTTONUP,0,MAKELPARAM(int(36*scale),int(217*scale)));check(state[division]>value,"Tab sync Rate did not redirect Division");previous[division]=state[division];check(previous==state,"Division gesture leaked");state[lfoParam(lfo,4)]=0;
   }
   for(int osc=0;osc<2;++osc){int id=osc?2:0,x=osc?327:39;auto wheel=[&](int delta,int keys=0){POINT p{int(x*scale),int(93*scale)};ClientToScreen(w,&p);SendMessageW(w,WM_MOUSEWHEEL,MAKEWPARAM(keys,short(delta)),MAKELPARAM(p.x,p.y));};state[id]=.517;auto before=state;wheel(120);check(std::abs(state[id]*48-24-2)<1e-10,"Wheel not exact next semitone");wheel(-120);check(std::abs(state[id]*48-24-1)<1e-10,"Wheel direction");double value=state[id];wheel(30);wheel(30);wheel(30);check(state[id]==value,"Partial wheel increment edited pitch");wheel(30);check(std::abs(state[id]*48-24-2)<1e-10,"High-resolution wheel accumulation");wheel(120,MK_SHIFT);check(std::abs((state[id]*48-24)-std::round(state[id]*48-24))>.01,"Fine wheel not fractional");before[id]=state[id];check(state==before,"Pitch wheel leaked");}
  }
 }
 editor.close();DestroyWindow(parent);std::cout<<"PASS approved centered 18px square tabs, four independent editor mappings, sync division, 6 themes x 4 scales, semitone/fine/high-resolution wheels\n";return 0;
 }catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
