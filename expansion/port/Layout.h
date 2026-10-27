// GPL-2.0-or-later. Single-page expansion layout, coordinates below 48px header.
// Geometry frozen after the 2026-10-17 optical pass. Change only when required
// by a later functional requirement or an explicit user revision.
#pragma once
#include "Core.h"
namespace xs {
enum class ControlKind { Knob, Selector, Amount, Toggle };
struct Control { int id,x,y,w; const char *label; ControlKind kind; int valueWidth=54; };
inline constexpr int MiddleRowBottom=357;
struct Bounds { int x,y,w,h; };
struct Panel { int x,y,w,h; const char *label; };
namespace faceGrid {
inline constexpr int HeaderHeight=48, TopHeight=141, MiddleWidth=240;
inline constexpr int ShellInset=1, SurfaceInset=2, HeaderInset=12, HeaderY=4, HeaderH=14;
inline constexpr int KnobCenter=15, KnobDiameter=36, LabelHeight=13, AmountHeight=18;
constexpr Bounds shell(const Panel &p) {return {p.x+ShellInset,p.y+ShellInset,p.w-2*ShellInset,p.h-2*ShellInset};}
constexpr Bounds header(const Panel &p) {return {p.x+HeaderInset,p.y+HeaderY,p.w-2*HeaderInset,HeaderH};}
}
inline constexpr Panel panels[]={
 {0,0,172,faceGrid::TopHeight,"VCO1 >"},{172,0,116,faceGrid::TopHeight,"XMOD"},{288,0,172,faceGrid::TopHeight,"< VCO2"},
 {460,0,78,faceGrid::TopHeight,"MIXER >"},{538,0,198,faceGrid::TopHeight,"VCF >"},{736,0,144,faceGrid::TopHeight,"DISTORTION"},{880,0,112,faceGrid::TopHeight,"AMP"},
 {0,141,224,216,"LFO"},{224,141,240,216,"EG1 (VCA)"},{464,141,240,216,"EG2"},{704,141,288,216,"VOICE"},
 {0,MiddleRowBottom,992,163,"MODULATION MATRIX"}
};
namespace vcfTabs {
inline constexpr int Y=2,Height=18,Width=18;
constexpr Bounds bounds(int index){return {684+index*22,Y,Width,Height};}
constexpr Bounds hit(int index){auto b=bounds(index);return {b.x-2,b.y-2,b.w+4,b.h+4};}
}
// Shared middle-module grid. Coordinates refer to the existing knob's centre
// (x + 15); readouts on 3/4-knob rows share the selector content edges.
namespace moduleGrid {
inline constexpr int Margin=12, ContentWidth=216, SelectorGap=12, ValueWidth=48;
inline constexpr int TopKnobs=171, LowerKnobs=245;
inline constexpr int KnobLabelOffset=35, KnobValueOffset=49, ValueHeight=12;
inline constexpr int SelectorHeight=18, SelectorTextInset=11;
inline constexpr int SelectorStep=36, MiddleSelectors=LowerKnobs+KnobValueOffset;
inline constexpr int FirstSelectors=MiddleSelectors-SelectorStep, LastSelectors=MiddleSelectors+SelectorStep;
constexpr int content(int origin){return origin==0?200:origin==704?264:216;}
constexpr Control knob(int origin,int count,int column,int y,int id,const char *label) {
 const int width=content(origin),centre=count==2?Margin+(width-SelectorGap)/4+column*(width+SelectorGap)/2:Margin+ValueWidth/2+column*(width-ValueWidth)/(count-1);
 return {id,origin+centre-faceGrid::KnobCenter,y,40,label,ControlKind::Knob,ValueWidth};
}
constexpr Control pair(int origin,int column,int y,int id,const char *label){int width=(content(origin)-SelectorGap)/2;return {id,origin+Margin+column*(width+SelectorGap),y,width,label,ControlKind::Selector};}
constexpr Control full(int origin,int y,int id,const char *label){return {id,origin+Margin,y,content(origin),label,ControlKind::Selector};}
constexpr Control centred(int origin,int y,int id,const char *label){auto c=pair(origin,0,y,id,label);c.x=origin+Margin+(content(origin)-c.w)/2;return c;}
} // namespace moduleGrid
namespace lfoTabs {
inline constexpr int Y=moduleGrid::LastSelectors,Height=moduleGrid::SelectorHeight,Width=Height,Gap=2;
inline constexpr int GroupWidth=4*Width+3*Gap,Left=(224-GroupWidth)/2;
constexpr Bounds bounds(int index){return {Left+index*(Width+Gap),Y,Width,Height};}
}
namespace vcoGrid {
constexpr int left(int o){return o*288+13;}
inline constexpr int Row1=94,Row2=114,Height=18,Width=146;
constexpr Bounds wave(int o){return {left(o),Row1,Width,Height};}
// Centered between the unchanged 47/125 px knob columns; same button height.
constexpr Bounds sourceMode(int o){return {left(o)+(Width-34)/2,36,34,Height};}
constexpr Bounds renderer(int o,int mode){return {left(o)+mode*49,Row2,48,Height};}
constexpr Bounds source(int o){return {left(o),Row1,48,Height};}
constexpr Bounds table(int o){return {left(o)+52,Row1,60,Height};}
constexpr Bounds pgm(int o){return {left(o)+116,Row1,30,Height};}
constexpr Bounds sample(int o){return {left(o),Row2,68,Height};}
constexpr Bounds frame(int o){return {left(o)+78,Row2,68,Height};}
constexpr bool contains(Bounds b,int x,int y){return x>=b.x&&x<b.x+b.w&&y>=b.y&&y<b.y+b.h;}
}
namespace topGrid {
inline constexpr int Knobs=30, Selectors=114, OscMargin=13, OscFieldWidth=68, OscGap=10;
constexpr int oscFieldX(int origin,int column) {return origin+OscMargin+column*(OscFieldWidth+OscGap);}
constexpr Control knob(int center,int id,const char *label) {
 return {id,center-faceGrid::KnobCenter,Knobs,40,label,ControlKind::Knob};
}
constexpr Control oscKnob(int origin,int column,int id,const char *label) {
 return knob(origin+(column?133:39),id,label);
}
constexpr Control oscSelector(int origin,int column,int id,const char *label) {
 return {id,oscFieldX(origin,column),Selectors,OscFieldWidth,label,ControlKind::Selector};
}
}
#define K(id,x,y,label) {id,x,y,40,label,ControlKind::Knob}
#define S(id,x,y,w,label) {id,x,y,w,label,ControlKind::Selector}
inline constexpr Control mainControls[]={
 topGrid::oscKnob(0,0,0,"Pitch"),topGrid::oscKnob(0,1,1,"Shape / PW"),
 topGrid::oscSelector(0,0,24,"Wave"),topGrid::oscSelector(0,1,Engine1,"Engine"),
 topGrid::knob(211,XDepth,"Depth"),S(XMode,184,topGrid::Selectors,42,"Mode"),S(XDirection,230,topGrid::Selectors,46,"Direction"),{27,238,36,28,"Hard Sync",ControlKind::Toggle},
 topGrid::oscKnob(288,0,2,"Pitch"),topGrid::oscKnob(288,1,3,"Shape / PW"),
 topGrid::oscSelector(288,0,25,"Wave"),topGrid::oscSelector(288,1,Engine2,"Engine"),
 topGrid::knob(499,7,"Balance"),S(MixerMode,472,topGrid::Selectors,54,"Mode"),
 topGrid::knob(577,21,"Cutoff"),topGrid::knob(637,22,"Resonance"),topGrid::knob(697,Keytrack,"Keytrack"),
 S(FilterType,550,topGrid::Selectors,56,"Type"),S(28,610,topGrid::Selectors,54,"Slope"),S(VcfRoute,668,topGrid::Selectors,56,"Route"),
 topGrid::knob(778,DistDrive,"Drive"),topGrid::knob(838,DistMix,"Mix"),
 S(DistMode,748,94,120,""),S(DistPlace,748,114,120,""),
 topGrid::knob(936,23,"Volume"),S(29,892,topGrid::Selectors,38,"Power"),
 moduleGrid::knob(0,3,0,moduleGrid::TopKnobs,4,"Rate"),
 moduleGrid::knob(0,3,1,moduleGrid::TopKnobs,5,"Pitch Depth"),
 moduleGrid::knob(0,3,2,moduleGrid::TopKnobs,6,"Filter Depth"),
 moduleGrid::pair(0,0,moduleGrid::FirstSelectors,26,"Wave"),
 moduleGrid::pair(0,1,moduleGrid::FirstSelectors,LfoClock,"Clock"),
 moduleGrid::pair(0,0,moduleGrid::MiddleSelectors,LfoPhase,"Phase"),
 moduleGrid::pair(0,1,moduleGrid::MiddleSelectors,LfoPolarity,"Polarity"),
 moduleGrid::knob(224,4,0,moduleGrid::TopKnobs,9,"Attack"),
 moduleGrid::knob(224,4,1,moduleGrid::TopKnobs,10,"Decay"),
 moduleGrid::knob(224,4,2,moduleGrid::TopKnobs,11,"Sustain"),
 moduleGrid::knob(224,4,3,moduleGrid::TopKnobs,12,"Release"),
 moduleGrid::knob(224,3,0,moduleGrid::LowerKnobs,Velocity1,"Velocity"),
 moduleGrid::knob(224,3,1,moduleGrid::LowerKnobs,13,"Pitch Depth"),
 moduleGrid::knob(224,3,2,moduleGrid::LowerKnobs,14,"Filter Depth"),
 moduleGrid::centred(224,moduleGrid::LastSelectors,Curve1,"Curve"),
 moduleGrid::knob(464,4,0,moduleGrid::TopKnobs,15,"Attack"),
 moduleGrid::knob(464,4,1,moduleGrid::TopKnobs,16,"Decay"),
 moduleGrid::knob(464,4,2,moduleGrid::TopKnobs,17,"Sustain"),
 moduleGrid::knob(464,4,3,moduleGrid::TopKnobs,18,"Release"),
 moduleGrid::knob(464,3,0,moduleGrid::LowerKnobs,Velocity2,"Velocity"),
 moduleGrid::knob(464,3,1,moduleGrid::LowerKnobs,19,"Pitch Depth"),
 moduleGrid::knob(464,3,2,moduleGrid::LowerKnobs,20,"Filter Depth"),
 S(Curve2,476,moduleGrid::LastSelectors,44,"Curve"),
 S(Loop2,524,moduleGrid::LastSelectors,44,"Cycle"),
 S(Eg2Sync,572,moduleGrid::LastSelectors,68,"Loop"),
 S(Eg2Rate,644,moduleGrid::LastSelectors,48,"Rate"),
 moduleGrid::knob(704,4,0,moduleGrid::TopKnobs,8,"Glide Time"),
 moduleGrid::knob(704,4,1,moduleGrid::TopKnobs,Detune,"Detune"),
 moduleGrid::knob(704,4,2,moduleGrid::TopKnobs,Width,"Width"),
 moduleGrid::knob(704,4,3,moduleGrid::TopKnobs,MasterTune,"Master Tune"),
 moduleGrid::full(704,moduleGrid::FirstSelectors,VoiceMode,"Voice Mode"),
 moduleGrid::pair(704,0,moduleGrid::MiddleSelectors,Voices,"Voices"),
 moduleGrid::pair(704,1,moduleGrid::MiddleSelectors,Unison,"Unison"),
 moduleGrid::pair(704,0,moduleGrid::LastSelectors,GlideMode,"Glide Mode"),
 moduleGrid::pair(704,1,moduleGrid::LastSelectors,BendRange,"Bend Range"),
};
#undef K
#undef S
constexpr Bounds fieldBounds(const Control &c) {
 return c.kind==ControlKind::Knob ? Bounds{c.x+faceGrid::KnobCenter-c.valueWidth/2,c.y+moduleGrid::KnobValueOffset,c.valueWidth,moduleGrid::ValueHeight}
  : Bounds{c.x,c.y,c.w,c.kind==ControlKind::Selector?moduleGrid::SelectorHeight:faceGrid::AmountHeight};
}
constexpr Bounds labelBounds(const Control &c) {
 if(c.kind==ControlKind::Knob)return {c.x-35,c.y+moduleGrid::KnobLabelOffset,100,faceGrid::LabelHeight};
 if(c.kind==ControlKind::Toggle)return {c.x-14,c.y+31,c.w+28,faceGrid::LabelHeight};
 const int offset=c.y>=faceGrid::TopHeight && c.y<MiddleRowBottom?moduleGrid::KnobValueOffset-moduleGrid::KnobLabelOffset:15;
 return {c.x,c.y-offset,c.w,faceGrid::LabelHeight};
}
namespace matrixGrid {
// Shared spacing revision: clear the module title without changing row rhythm.
inline constexpr int HeaderClearance=3, LabelHeight=12, LabelToFieldGap=3;
inline constexpr int LabelY=MiddleRowBottom+faceGrid::HeaderY+faceGrid::HeaderH+HeaderClearance;
inline constexpr int FirstRow=LabelY+LabelHeight+LabelToFieldGap;
inline constexpr int HalfWidth=496, RowStep=20;
inline constexpr int FieldX[]={30,150,382}, FieldWidth[]={96,218,102};
constexpr int origin(int slot) {return slot<6?0:HalfWidth;}
constexpr int row(int slot) {return FirstRow+(slot%6)*RowStep;}
}
inline constexpr int MatrixFirstRow=matrixGrid::FirstRow;
inline constexpr std::array<Control,36> matrixControls() {
 std::array<Control,36> result{};
 for(int slot=0;slot<MatrixSlots;++slot) {
   int x=matrixGrid::origin(slot),y=matrixGrid::row(slot),id=matrixId(slot);
   for(int column=0;column<3;++column)
    result[slot*3+column]={id+column,x+matrixGrid::FieldX[column],y,matrixGrid::FieldWidth[column],column==0?"Source":column==1?"Destination":"Amount",column==2?ControlKind::Amount:ControlKind::Selector};
 }
 return result;
}
inline int effectiveId(const Control &c,const State &s,int lfo=0) {if(c.id>=ExtraLfoFirst&&c.id<NewMatrixFirst){int l=(c.id-ExtraLfoFirst)/9+1;return lfoBase(c.id)==4&&s[lfoParam(l,4)]>=.5?lfoParam(l,8):c.id;}int f=lfoField(c.id);if(f>=0){if(f==0&&s[lfoParam(lfo,4)]>=.5)f=8;return lfoParam(lfo,f);}if((c.id==1||c.id==3)&&s[oscParam(c.id==3,0)]>=.25)return oscParam(c.id==3,3);return c.id; }
inline unsigned modulationRoutes(const State &s,int id) {
 unsigned mask=0;
 for(int slot=0;slot<MatrixSlots;++slot) {
  int k=matrixId(slot);
  int destination=routeDestination(s,slot);
  if(effectiveSource(s,slot) && s[k+2]!=.5 &&
      (DestinationParams[destination]==(id==Division?4:id) || (destination==11 && id==6)))mask|=1u<<slot;
 }
 return mask;
}
inline bool modulated(const State &s,int id) {return modulationRoutes(s,id)!=0;}
} // namespace xs
