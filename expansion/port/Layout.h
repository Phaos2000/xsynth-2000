// GPL-2.0-or-later. Single-page expansion layout, coordinates below 48px header.
// Geometry frozen after the 2026-10-17 optical pass. Change only when required
// by a later functional requirement or an explicit user revision.
#pragma once
#include "Core.h"
namespace xs {
enum class ControlKind { Knob, Selector, Amount, Toggle };
struct Control { int id,x,y,w; const char *label; ControlKind kind; int valueWidth=54; };
inline constexpr int MiddleRowBottom=370, MatrixFirstRow=MiddleRowBottom+32;
struct Bounds { int x,y,w,h; };
struct Panel { int x,y,w,h; const char *label; };
namespace faceGrid {
inline constexpr int HeaderHeight=48, TopHeight=154, MiddleWidth=240;
inline constexpr int ShellInset=1, SurfaceInset=2, HeaderInset=12, HeaderY=4, HeaderH=14;
inline constexpr int KnobCenter=15, KnobDiameter=36, LabelHeight=13, AmountHeight=22;
constexpr Bounds shell(const Panel &p) {return {p.x+ShellInset,p.y+ShellInset,p.w-2*ShellInset,p.h-2*ShellInset};}
constexpr Bounds header(const Panel &p) {return {p.x+HeaderInset,p.y+HeaderY,p.w-2*HeaderInset,HeaderH};}
}
inline constexpr Panel panels[]={
 {0,0,170,faceGrid::TopHeight,"VCO1 >"},{170,0,138,faceGrid::TopHeight,"XMOD"},{308,0,170,faceGrid::TopHeight,"< VCO2"},
 {478,0,104,faceGrid::TopHeight,"MIXER >"},{582,0,252,faceGrid::TopHeight,"VCF >"},{834,0,126,faceGrid::TopHeight,"AMP"},
 {0,faceGrid::TopHeight,240,MiddleRowBottom-faceGrid::TopHeight,"LFO"},
 {240,faceGrid::TopHeight,240,MiddleRowBottom-faceGrid::TopHeight,"EG1 (VCA)"},
 {480,faceGrid::TopHeight,240,MiddleRowBottom-faceGrid::TopHeight,"EG2"},
 {720,faceGrid::TopHeight,240,MiddleRowBottom-faceGrid::TopHeight,"VOICE"},
 {0,MiddleRowBottom,960,150,"MODULATION MATRIX"}
};
// Shared middle-module grid. Coordinates refer to the existing knob's centre
// (x + 15); readouts on 3/4-knob rows share the selector content edges.
namespace moduleGrid {
inline constexpr int Margin=12, ContentWidth=216, SelectorGap=12, ValueWidth=48;
inline constexpr int TopKnobs=184, LowerKnobs=258;
inline constexpr int KnobLabelOffset=35, KnobValueOffset=49, ValueHeight=12;
inline constexpr int SelectorHeight=18, SelectorTextInset=11;
inline constexpr int SelectorStep=36, MiddleSelectors=LowerKnobs+KnobValueOffset;
inline constexpr int FirstSelectors=MiddleSelectors-SelectorStep, LastSelectors=MiddleSelectors+SelectorStep;
constexpr Control knob(int origin,int count,int column,int y,int id,const char *label) {
 const int centre=count==2 ? Margin+(ContentWidth-SelectorGap)/4+column*(ContentWidth+SelectorGap)/2
                          : Margin+ValueWidth/2+column*(ContentWidth-ValueWidth)/(count-1);
 return {id,origin+centre-faceGrid::KnobCenter,y,40,label,ControlKind::Knob,ValueWidth};
}
constexpr Control pair(int origin,int column,int y,int id,const char *label) {
 constexpr int width=(ContentWidth-SelectorGap)/2;
 return {id,origin+Margin+column*(width+SelectorGap),y,width,label,ControlKind::Selector};
}
constexpr Control full(int origin,int y,int id,const char *label) {
 return {id,origin+Margin,y,ContentWidth,label,ControlKind::Selector};
}
constexpr Control centred(int origin,int y,int id,const char *label) {
 auto field=pair(origin,0,y,id,label);
 field.x=origin+Margin+(ContentWidth-field.w)/2;
 return field;
}
} // namespace moduleGrid
namespace topGrid {
inline constexpr int Knobs=30, Selectors=114, OscMargin=12, OscFieldWidth=68, OscGap=10;
constexpr int oscFieldX(int origin,int column) {return origin+OscMargin+column*(OscFieldWidth+OscGap);}
constexpr Control knob(int center,int id,const char *label) {
 return {id,center-faceGrid::KnobCenter,Knobs,40,label,ControlKind::Knob};
}
constexpr Control oscKnob(int origin,int column,int id,const char *label) {
 return knob(oscFieldX(origin,column)+OscFieldWidth/2,id,label);
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
 topGrid::knob(217,XDepth,"Depth"),S(XMode,180,topGrid::Selectors,54,"Mode"),S(XDirection,242,topGrid::Selectors,56,"Direction"),{27,266,34,28,"Hard Sync",ControlKind::Toggle},
 topGrid::oscKnob(308,0,2,"Pitch"),topGrid::oscKnob(308,1,3,"Shape / PW"),
 topGrid::oscSelector(308,0,25,"Wave"),topGrid::oscSelector(308,1,Engine2,"Engine"),
 topGrid::knob(478+104/2,7,"Balance"),S(MixerMode,486,topGrid::Selectors,88,"Mode"),
 topGrid::knob(582+252/2-84,21,"Cutoff"),topGrid::knob(582+252/2,22,"Resonance"),topGrid::knob(582+252/2+84,Keytrack,"Keytrack"),
 S(FilterType,594,topGrid::Selectors,100,"Type"),S(28,706,topGrid::Selectors,116,"Slope"),
 topGrid::knob(834+126/2,23,"Volume"),S(29,844,topGrid::Selectors,106,"Power"),
 moduleGrid::knob(0,3,0,moduleGrid::TopKnobs,4,"Rate"),
 moduleGrid::knob(0,3,1,moduleGrid::TopKnobs,5,"Pitch Depth"),
 moduleGrid::knob(0,3,2,moduleGrid::TopKnobs,6,"Filter Depth"),
 moduleGrid::pair(0,0,moduleGrid::FirstSelectors,26,"Wave"),
 moduleGrid::pair(0,1,moduleGrid::FirstSelectors,LfoClock,"Clock"),
 moduleGrid::pair(0,0,moduleGrid::MiddleSelectors,LfoPhase,"Phase"),
 moduleGrid::pair(0,1,moduleGrid::MiddleSelectors,LfoPolarity,"Polarity"),
 moduleGrid::knob(240,4,0,moduleGrid::TopKnobs,9,"Attack"),
 moduleGrid::knob(240,4,1,moduleGrid::TopKnobs,10,"Decay"),
 moduleGrid::knob(240,4,2,moduleGrid::TopKnobs,11,"Sustain"),
 moduleGrid::knob(240,4,3,moduleGrid::TopKnobs,12,"Release"),
 moduleGrid::knob(240,3,0,moduleGrid::LowerKnobs,Velocity1,"Velocity"),
 moduleGrid::knob(240,3,1,moduleGrid::LowerKnobs,13,"Pitch Depth"),
 moduleGrid::knob(240,3,2,moduleGrid::LowerKnobs,14,"Filter Depth"),
 moduleGrid::centred(240,moduleGrid::LastSelectors,Curve1,"Curve"),
 moduleGrid::knob(480,4,0,moduleGrid::TopKnobs,15,"Attack"),
 moduleGrid::knob(480,4,1,moduleGrid::TopKnobs,16,"Decay"),
 moduleGrid::knob(480,4,2,moduleGrid::TopKnobs,17,"Sustain"),
 moduleGrid::knob(480,4,3,moduleGrid::TopKnobs,18,"Release"),
 moduleGrid::knob(480,3,0,moduleGrid::LowerKnobs,Velocity2,"Velocity"),
 moduleGrid::knob(480,3,1,moduleGrid::LowerKnobs,19,"Pitch Depth"),
 moduleGrid::knob(480,3,2,moduleGrid::LowerKnobs,20,"Filter Depth"),
 moduleGrid::pair(480,0,moduleGrid::LastSelectors,Curve2,"Curve"),
 moduleGrid::pair(480,1,moduleGrid::LastSelectors,Loop2,"Loop"),
 moduleGrid::knob(720,2,0,moduleGrid::TopKnobs,8,"Glide Time"),
 moduleGrid::knob(720,2,1,moduleGrid::TopKnobs,MasterTune,"Master Tune"),
 moduleGrid::full(720,moduleGrid::FirstSelectors,VoiceMode,"Voice Mode"),
 moduleGrid::pair(720,0,moduleGrid::MiddleSelectors,Voices,"Voices"),
 moduleGrid::pair(720,1,moduleGrid::MiddleSelectors,GlideMode,"Glide Mode"),
 moduleGrid::full(720,moduleGrid::LastSelectors,BendRange,"Bend Range"),
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
inline constexpr int HalfWidth=480, RowStep=40;
inline constexpr int FieldX[]={30,150,366}, FieldWidth[]={96,202,102};
constexpr int origin(int slot) {return slot<3?0:HalfWidth;}
constexpr int row(int slot) {return MatrixFirstRow+(slot%3)*RowStep;}
}
inline constexpr std::array<Control,18> matrixControls() {
 std::array<Control,18> result{};
 for(int slot=0;slot<6;++slot) {
   int x=matrixGrid::origin(slot),y=matrixGrid::row(slot),id=MatrixFirst+slot*3;
   for(int column=0;column<3;++column)
    result[slot*3+column]={id+column,x+matrixGrid::FieldX[column],y,matrixGrid::FieldWidth[column],column==0?"Source":column==1?"Destination":"Amount",column==2?ControlKind::Amount:ControlKind::Selector};
 }
 return result;
}
inline int effectiveId(const Control &c,const State &s) { return c.id==4 && s[LfoClock]>=.5?Division:c.id; }
inline constexpr int DestinationParams[]={0,1,-1,2,3,-2,XDepth,7,21,22,4,5,9,10,12,15,16,18};
inline unsigned modulationRoutes(const State &s,int id) {
 unsigned mask=0;
 for(int slot=0;slot<6;++slot) {
  int k=MatrixFirst+slot*3;
  int destination=discrete(k+1,s[k+1]);
  if(discrete(k,s[k]) && s[k+2]!=.5 &&
      (DestinationParams[destination]==(id==Division?4:id) || (destination==11 && id==6)))mask|=1u<<slot;
 }
 return mask;
}
inline bool modulated(const State &s,int id) {return modulationRoutes(s,id)!=0;}
} // namespace xs
