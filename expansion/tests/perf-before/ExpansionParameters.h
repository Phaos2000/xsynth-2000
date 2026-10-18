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
  MatrixFirst, PreviousCount=MatrixFirst+18, ExpandedDestinationFirst=PreviousCount, PreUnisonCount=ExpandedDestinationFirst+6, Unison=PreUnisonCount, Detune, DetuneRouteFirst, PreFourLfoCount=DetuneRouteFirst+6, ExtraLfoFirst=PreFourLfoCount, NewMatrixFirst=ExtraLfoFirst+27, NewDestinationFirst=NewMatrixFirst+18, SourceExtensionFirst=NewDestinationFirst, Count=SourceExtensionFirst+12
};
constexpr int LegacyCount=32, AllNotesOff=32, AllSoundsOff=33;
constexpr int NativeWidth=960, NativeHeight=568;
constexpr int LegacySourceCount=11, SourceCount=14, MatrixSlots=12, LegacyDestinationCount=18, EstablishedDestinationCount=41, DestinationCount=42;
inline constexpr const char *Sources[]={"OFF","LFO 1","EG1","EG2","Velocity","Key","Mod Wheel","Pressure","Pitch Bend","Note Random","Constant","LFO 2","LFO 3","LFO 4"};
inline constexpr const char *Destinations[]={"VCO1 Pitch","VCO1 Shape","VCO1 Level","VCO2 Pitch","VCO2 Shape","VCO2 Level","XMOD Depth","Mixer Balance","VCF Cutoff","VCF Resonance","LFO Rate","LFO Depth (Legacy Both)","EG1 Attack","EG1 Decay","EG1 Release","EG2 Attack","EG2 Decay","EG2 Release",
 "VCO1 Wave","VCO2 Wave","XMOD Mode","XMOD Direction","Mixer Mode","VCF Type","VCF Slope","VCF Keytrack","LFO Phase","LFO Polarity","LFO Pitch Depth","LFO Filter Depth","EG1 Pitch Depth","EG1 Filter Depth","EG1 Velocity","EG1 Curve","EG2 Pitch Depth","EG2 Filter Depth","EG2 Velocity","EG2 Curve","EG2 Loop","Glide Time","Master Tune","Unison Detune"};
inline constexpr int DestinationParams[]={0,1,-1,2,3,-1,XDepth,7,21,22,4,5,9,10,12,15,16,18,
 24,25,XMode,XDirection,MixerMode,FilterType,28,Keytrack,LfoPhase,LfoPolarity,5,6,13,14,Velocity1,Curve1,19,20,Velocity2,Curve2,Loop2,8,MasterTune,Detune};
inline constexpr int UnisonCounts[]={1,2,4,8};
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
constexpr bool legacyDestination(int id) {return id>=MatrixFirst && id<PreviousCount && (id-MatrixFirst)%3==1;}
constexpr bool expandedDestination(int id) {return id>=ExpandedDestinationFirst && id<PreUnisonCount;}
constexpr int legacyDestinationId(int slot) {return MatrixFirst+slot*3+1;}
// New IDs: 0 follows the historical parameter; (index+1)/41 selects index 0..40.
// Any live write to the historical parameter releases the override, even a repeat.
template<class S> inline int effectiveDestination(const S&s,int slot) {
 if(s[DetuneRouteFirst+slot]>=.5)return EstablishedDestinationCount;
 int extended=int(std::round(s[ExpandedDestinationFirst+slot]*EstablishedDestinationCount));
 return extended?extended-1:int(std::round(s[legacyDestinationId(slot)]*(LegacyDestinationCount-1)));
}
inline constexpr int LfoBaseIds[]={4,5,6,26,LfoClock,LfoPhase,LfoPolarity,LfoRandom,Division};
constexpr int lfoParam(int lfo,int field){return lfo?ExtraLfoFirst+(lfo-1)*9+field:LfoBaseIds[field];}
constexpr int lfoField(int id){for(int f=0;f<9;++f)if(LfoBaseIds[f]==id)return f;return -1;}
constexpr int lfoBase(int id){return id>=ExtraLfoFirst&&id<NewMatrixFirst?LfoBaseIds[(id-ExtraLfoFirst)%9]:id;}
constexpr int matrixId(int slot,int field=0){return (slot<6?MatrixFirst+slot*3:NewMatrixFirst+(slot-6)*3)+field;}
constexpr int matrixSlot(int id){return id>=MatrixFirst&&id<PreviousCount?(id-MatrixFirst)/3:id>=NewMatrixFirst&&id<NewDestinationFirst?6+(id-NewMatrixFirst)/3:-1;}
constexpr bool matrixSource(int id){int slot=matrixSlot(id);return slot>=0&&id==matrixId(slot);}
constexpr bool matrixDestination(int id){int slot=matrixSlot(id);return slot>=0&&id==matrixId(slot,1);}
template<class S> inline int effectiveSource(const S&s,int slot){int ext=int(std::round(s[SourceExtensionFirst+slot]*3));return ext?10+ext:int(std::round(s[matrixId(slot)]*10));}
template<class S> inline int routeDestination(const S&s,int slot){return slot<6?effectiveDestination(s,slot):int(std::round(s[matrixId(slot,1)]*(DestinationCount-1)));}
constexpr bool sourceExtension(int id){return id>=SourceExtensionFirst&&id<Count;}
inline int stepsFor(int id) {
 if(id>=ExtraLfoFirst&&id<NewMatrixFirst)return stepsFor(lfoBase(id));
 if(sourceExtension(id))return 3;
 if(id>=NewMatrixFirst&&id<NewDestinationFirst)return (id-NewMatrixFirst)%3==0?10:(id-NewMatrixFirst)%3==1?DestinationCount-1:0;
 if(id==Unison)return 3;
 if(id>=DetuneRouteFirst&&id<PreFourLfoCount)return 1;
 if(expandedDestination(id))return EstablishedDestinationCount;
 if(id>=24 && id<27) return 5;
 if(id>=27 && id<30) return 1;
 if(id>=34 && id<MatrixFirst) return extraSpecs[id-34].steps;
 if(id>=MatrixFirst && id<PreviousCount) return (id-MatrixFirst)%3==0 ? LegacySourceCount-1 : (id-MatrixFirst)%3==1 ? LegacyDestinationCount-1 : 0;
 return 0;
}
inline double extraDefault(int id) {
 if(id>=ExtraLfoFirst&&id<NewMatrixFirst){int base=lfoBase(id);return base>=34?extraSpecs[base-34].initial:0.;}
 if(id>=NewMatrixFirst&&id<NewDestinationFirst)return (id-NewMatrixFirst)%3==2?.5:0.;
 if(id>=34 && id<MatrixFirst) return extraSpecs[id-34].initial;
 return id>=MatrixFirst && id<PreviousCount && (id-MatrixFirst)%3==2 ? .5 : 0.;
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
 if(id>=ExtraLfoFirst&&id<NewMatrixFirst){int base=lfoBase(id);if(base==4)std::snprintf(out,size,"%.2f Hz",.1*std::pow(10.,2*n));else if(base==5||base==6)std::snprintf(out,size,"%.0f%%",n*100);else if(base==26)choiceText("SINE|TRI|SAW UP|SAW DN|SQUARE|PULSE",k,out,size);else extraText(base,n,out,size);}
 else if(sourceExtension(id))std::snprintf(out,size,"%s",k?Sources[10+k]:"Follow source");
 else if(id>=NewMatrixFirst&&id<NewDestinationFirst){if((id-NewMatrixFirst)%3==0)std::snprintf(out,size,"%s",Sources[k]);else if((id-NewMatrixFirst)%3==1)std::snprintf(out,size,"%s",Destinations[k]);else std::snprintf(out,size,"%+.1f",(n*2-1)*100);}
 else if(id==Unison)choiceText("OFF|2|4|8",k,out,size);
 else if(id==Detune)std::snprintf(out,size,"%.1f ct",n*50);
 else if(id>=DetuneRouteFirst&&id<PreFourLfoCount)std::snprintf(out,size,"%s",k?"Unison Detune":"Follow destination");
 else if(expandedDestination(id)) {std::snprintf(out,size,"%s",k?Destinations[k-1]:"Follow legacy destination");}
 else if(id==Division) { int d=int(std::round(n*29)); std::snprintf(out,size,"%s%s",DivisionNames[d/3],d%3==1?"D":d%3==2?"T":""); }
 else if(id>=34 && id<MatrixFirst && extraSpecs[id-34].choices) choiceText(extraSpecs[id-34].choices,k,out,size);
 else if(id>=MatrixFirst && (id-MatrixFirst)%3==0) std::snprintf(out,size,"%s",Sources[k]);
 else if(id>=MatrixFirst && (id-MatrixFirst)%3==1) std::snprintf(out,size,"%s",Destinations[k]);
 else if(id>=MatrixFirst) std::snprintf(out,size,"%+.1f",(n*2-1)*100);
 else if(id==MasterTune) std::snprintf(out,size,"%+.1f ct",(n*2-1)*100);
 else std::snprintf(out,size,"%.3g",n);
}
} // namespace xs
