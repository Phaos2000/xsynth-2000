// GPL-2.0-or-later. Stable expansion IDs. 0..33 retain their previous meanings.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
namespace xs {
enum Param : int {
  Engine1=34, Engine2, XMode, XDirection, XDepth, MixerMode, FilterType, Keytrack,
  LfoClock, LfoPhase, LfoPolarity, LfoRandom, Curve1, Velocity1, Curve2, Velocity2,
  Loop2, VoiceMode, Voices, GlideMode, BendRange, MasterTune, Pressure, Division,
  MatrixFirst, Count=MatrixFirst+18
};
constexpr int LegacyCount=32, AllNotesOff=32, AllSoundsOff=33;
constexpr int NativeWidth=960, NativeHeight=568;
constexpr int SourceCount=11, LegacyDestinationCount=18, DestinationCount=41;
inline constexpr const char *Sources[]={"OFF","LFO","EG1","EG2","Velocity","Key","Mod Wheel","Pressure","Pitch Bend","Note Random","Constant"};
inline constexpr const char *Destinations[]={"VCO1 Pitch","VCO1 Shape","VCO1 Level","VCO2 Pitch","VCO2 Shape","VCO2 Level","XMOD Depth","Mixer Balance","VCF Cutoff","VCF Resonance","LFO Rate","LFO Depth (Legacy Both)","EG1 Attack","EG1 Decay","EG1 Release","EG2 Attack","EG2 Decay","EG2 Release",
 "VCO1 Wave","VCO2 Wave","XMOD Mode","XMOD Direction","Mixer Mode","VCF Type","VCF Slope","VCF Keytrack","LFO Phase","LFO Polarity","LFO Pitch Depth","LFO Filter Depth","EG1 Pitch Depth","EG1 Filter Depth","EG1 Velocity","EG1 Curve","EG2 Pitch Depth","EG2 Filter Depth","EG2 Velocity","EG2 Curve","EG2 Loop","Glide Time","Master Tune"};
inline constexpr int DestinationParams[]={0,1,-1,2,3,-1,XDepth,7,21,22,4,5,9,10,12,15,16,18,
 24,25,XMode,XDirection,MixerMode,FilterType,28,Keytrack,LfoPhase,LfoPolarity,5,6,13,14,Velocity1,Curve1,19,20,Velocity2,Curve2,Loop2,8,MasterTune};
inline constexpr int VoiceCounts[]={1,2,4,8,16,32};
inline constexpr int BendRanges[]={0,1,2,3,4,5,7,12,24,48};
inline constexpr double DivisionBeats[]={32,16,8,4,2,1,.5,.25,.125,.0625};
inline constexpr const char *DivisionNames[]={"8/1","4/1","2/1","1/1","1/2","1/4","1/8","1/16","1/32","1/64"};
struct ExtraSpec { const char *name; int steps; double initial; const char *choices; };
inline constexpr ExtraSpec extraSpecs[]={
 {"VCO1 engine",1,0,"CLASSIC|CLEAN"},{"VCO2 engine",1,0,"CLASSIC|CLEAN"},
 {"XMOD mode",2,0,"OFF|FM|PM"},{"XMOD direction",1,0,"1 > 2|2 > 1"},
 {"XMOD depth",0,0,nullptr},{"Mixer mode",5,0,"BAL|SUM|RING|AM|XOR|DIFF"},
 {"VCF type",3,0,"LP|HP|BP|NOTCH"},{"VCF keytrack",0,1,nullptr},
 {"LFO clock",1,0,"Hz|Sync"},{"LFO phase",2,0,"FREE|KEY|HOST"},
 {"LFO polarity",1,0,"BI|UNI"},{"LFO digital wave",2,0,"CLASSIC|S&H|SMOOTH"},
 {"EG1 curve",2,.5,"LINEAR|EXP|LOG"},{"EG1 velocity",0,0,nullptr},
 {"EG2 curve",2,.5,"LINEAR|EXP|LOG"},{"EG2 velocity",0,0,nullptr},
 {"EG2 loop",2,0,"OFF|A-D|A-D-R"},{"Voice mode",2,0,"MONO RETRIG|MONO LEGATO|POLY"},
 {"Voice count",5,.6,"1|2|4|8|16|32"},{"Glide mode",3,1./3,"OFF|ALWAYS|LEGATO|NON-LEGATO"},
 {"Pitch bend range",9,7./9,"0|1|2|3|4|5|7|12|24|48"},
 {"Master tune",0,.5,nullptr},{"Channel pressure",0,0,nullptr},
 {"LFO division",29,15./29,nullptr}
};
inline int stepsFor(int id) {
 if(id>=24 && id<27) return 5;
 if(id>=27 && id<30) return 1;
 if(id>=34 && id<MatrixFirst) return extraSpecs[id-34].steps;
 if(id>=MatrixFirst && id<Count) return (id-MatrixFirst)%3==0 ? SourceCount-1 : (id-MatrixFirst)%3==1 ? DestinationCount-1 : 0;
 return 0;
}
inline double extraDefault(int id) {
 if(id>=34 && id<MatrixFirst) return extraSpecs[id-34].initial;
 return id>=MatrixFirst && (id-MatrixFirst)%3==2 ? .5 : 0.;
}
inline int discrete(int id,double n) { return int(std::round(std::clamp(n,0.,1.)*stepsFor(id))); }
inline void choiceText(const char *choices,int index,char *out,size_t size) {
 const char *p=choices;
 while(index-- && p && *p) { auto next=std::strchr(p,'|'); p=next?next+1:p+std::strlen(p); }
 if(!p) { if(size) out[0]=0; return; }
 auto end=std::strchr(p,'|'); auto n=std::min(size-1,end?size_t(end-p):std::strlen(p));
 std::memcpy(out,p,n); out[n]=0;
}
inline double divisionBeats(double n) {
 int k=int(std::round(n*29));
 return DivisionBeats[k/3]*(k%3==1?1.5:k%3==2?2./3:1.);
}
inline void extraText(int id,double n,char *out,size_t size) {
 int k=discrete(id,n);
 if(id==Division) { int d=int(std::round(n*29)); std::snprintf(out,size,"%s%s",DivisionNames[d/3],d%3==1?"D":d%3==2?"T":""); }
 else if(id>=34 && id<MatrixFirst && extraSpecs[id-34].choices) choiceText(extraSpecs[id-34].choices,k,out,size);
 else if(id>=MatrixFirst && (id-MatrixFirst)%3==0) std::snprintf(out,size,"%s",Sources[k]);
 else if(id>=MatrixFirst && (id-MatrixFirst)%3==1) std::snprintf(out,size,"%s",Destinations[k]);
 else if(id>=MatrixFirst) std::snprintf(out,size,"%+.1f",(n*2-1)*100);
 else if(id==MasterTune) std::snprintf(out,size,"%+.1f ct",(n*2-1)*100);
 else std::snprintf(out,size,"%.3g",n);
}
} // namespace xs
