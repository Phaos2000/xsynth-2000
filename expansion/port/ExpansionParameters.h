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
  MatrixFirst, PreviousCount=MatrixFirst+18, ExpandedDestinationFirst=PreviousCount, PreUnisonCount=ExpandedDestinationFirst+6, Unison=PreUnisonCount, Detune, DetuneRouteFirst, PreFourLfoCount=DetuneRouteFirst+6, ExtraLfoFirst=PreFourLfoCount, NewMatrixFirst=ExtraLfoFirst+27, NewDestinationFirst=NewMatrixFirst+18, SourceExtensionFirst=NewDestinationFirst, PreWavetableCount=SourceExtensionFirst+12, OscSourceFirst=PreWavetableCount, PositionRouteFirst=OscSourceFirst+12, RasterFirst=PositionRouteFirst+12, PreWidthCount=RasterFirst+2, Width=PreWidthCount, PrePedalCount=Width+1, Sustain=PrePedalCount, Sostenuto, PreDualCount=Sostenuto+1, Eg2Sync=PreDualCount, Eg2Rate, VcfBCutoff, VcfBResonance, VcfBKeytrack, VcfBType, VcfBSlope, VcfRoute, BFilterRouteFirst, PreDistortionCount=BFilterRouteFirst+12, DistMode=PreDistortionCount, DistDrive, DistMix, DistPlace, PreVoicesCount, VoicesExtension=PreVoicesCount, PreDriveCount, DriveExtension=PreDriveCount, Count
};
constexpr int oscParam(int oscillator,int field){return OscSourceFirst+oscillator*6+field;}
static_assert(PreDistortionCount==196&&DistMode==196&&DistPlace==199&&PreVoicesCount==200&&VoicesExtension==200&&PreDriveCount==201&&DriveExtension==201&&Count==202,"appended IDs preserve all existing encodings");
constexpr int LegacyCount=32, AllNotesOff=32, AllSoundsOff=33;
constexpr int NativeWidth=992, NativeHeight=568;
constexpr int LegacySourceCount=11, SourceCount=14, MatrixSlots=12, LegacyDestinationCount=18, EstablishedDestinationCount=41, DestinationCount=46;
inline constexpr const char *Sources[]={"OFF","LFO 1","EG1","EG2","Velocity","Key","Mod Wheel","Pressure","Pitch Bend","Note Random","Constant","LFO 2","LFO 3","LFO 4"};
inline constexpr const char *Destinations[]={"VCO1 Pitch","VCO1 Shape / Position","VCO1 Level","VCO2 Pitch","VCO2 Shape / Position","VCO2 Level","XMOD Depth","Mixer Balance","VCF Cutoff","VCF Resonance","LFO Rate","LFO Depth (Legacy Both)","EG1 Attack","EG1 Decay","EG1 Release","EG2 Attack","EG2 Decay","EG2 Release",
 "VCO1 Wave","VCO2 Wave","XMOD Mode","XMOD Direction","Mixer Mode","VCF Type","VCF Slope","VCF Keytrack","LFO Phase","LFO Polarity","LFO Pitch Depth","LFO Filter Depth","EG1 Pitch Depth","EG1 Filter Depth","EG1 Velocity","EG1 Curve","EG2 Pitch Depth","EG2 Filter Depth","EG2 Velocity","EG2 Curve","EG2 Loop","Glide Time","Master Tune","Unison Detune","VCO1 Shape / Position","VCO2 Shape / Position","VCF B Cutoff","VCF B Resonance"};
// Stable saved identities1/4 and historical aliases42/43 share one visible
// source-aware destination. Never renumber the published destination space.
constexpr int shapePositionIdentity(int destination){return destination==42?1:destination==43?4:destination;}
template<class S>inline int renderDestination(const S&s,int destination){
 int unified=shapePositionIdentity(destination);
 if(unified==1||unified==4){int oscillator=unified==1?0:1;return s[oscParam(oscillator,0)]>=.25?42+oscillator:unified;}
 return destination;
}
inline constexpr int DestinationParams[]={0,1,-1,2,3,-1,XDepth,7,21,22,4,5,9,10,12,15,16,18,
 24,25,XMode,XDirection,MixerMode,FilterType,28,Keytrack,LfoPhase,LfoPolarity,5,6,13,14,Velocity1,Curve1,19,20,Velocity2,Curve2,Loop2,8,MasterTune,Detune,OscSourceFirst+3,OscSourceFirst+9,VcfBCutoff,VcfBResonance};
// The old Drive parameter remains0..100%. Extension adds0..100%, so recorded compound sweeps remain continuous.
template<class S>inline double effectiveDrive(const S&s){return s[DistDrive]+s[DriveExtension];}
template<class S>inline int storedParameterCount(const S&s){return s[DriveExtension]==0?PreDriveCount:Count;}
inline constexpr int UnisonCounts[]={1,2,4,8};
// Keep the six normalized selector positions. Only the maximum physical budget
// is64; stored normalized values and all lower budgets retain their encoding.
// The historical maximum128 position intentionally resolves to64.
inline constexpr int VoiceCounts[]={1,2,4,8,16,64};
inline constexpr int VoiceChoices[]={1,2,4,8,16,32,64};
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
 {"Voice count",5,.6,"1|2|4|8|16|64"},{"Glide mode",3,1./3,"OFF|ALWAYS|LEGATO|NON-LEGATO"},
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
template<class S> inline int routeDestination(const S&s,int slot){int secondary=int(std::round(s[BFilterRouteFirst+slot]*2));if(secondary)return 43+secondary;int position=int(std::round(s[PositionRouteFirst+slot]*2));return position?41+position:slot<6?effectiveDestination(s,slot):int(std::round(s[matrixId(slot,1)]*41));}
constexpr bool sourceExtension(int id){return id>=SourceExtensionFirst&&id<PreWavetableCount;}
inline constexpr int stepsFor(int id) {
 if(id==DriveExtension)return 0;
 if(id==VoicesExtension)return 2;
 if(id==DistMode)return 10;
 if(id==DistPlace)return 1;
 if(id==Eg2Sync)return 3;
 if(id==Eg2Rate)return 9;
 if(id==VcfBType)return 3;
 if(id==VcfBSlope)return 1;
 if(id==VcfRoute)return 4;
 if(id>=BFilterRouteFirst&&id<PreDistortionCount)return 2;
 if(id==Sustain||id==Sostenuto)return 1;
 if(id==Width)return 0;
 if(id>=RasterFirst&&id<PreWidthCount)return 1;
 if(id>=PositionRouteFirst&&id<RasterFirst)return 2;
 if(id>=OscSourceFirst&&id<PositionRouteFirst){constexpr int steps[]={2,65535,15,0,2,1};return steps[(id-OscSourceFirst)%6];}
 if(id>=ExtraLfoFirst&&id<NewMatrixFirst)return stepsFor(lfoBase(id));
 if(sourceExtension(id))return 3;
 if(id>=NewMatrixFirst&&id<NewDestinationFirst)return (id-NewMatrixFirst)%3==0?10:(id-NewMatrixFirst)%3==1?41:0;
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
 if(id==DistDrive)return .5;
 if(id==DistMix||id==DistPlace)return 1;
 if(id==Eg2Rate)return 5./9;
 if(id==VcfBCutoff)return 1;
 if(id>=PreDualCount)return 0;
 if(id>=ExtraLfoFirst&&id<NewMatrixFirst){int base=lfoBase(id);return base>=34?extraSpecs[base-34].initial:0.;}
 if(id>=NewMatrixFirst&&id<NewDestinationFirst)return (id-NewMatrixFirst)%3==2?.5:0.;
 if(id>=34 && id<MatrixFirst) return extraSpecs[id-34].initial;
 return id>=MatrixFirst && id<PreviousCount && (id-MatrixFirst)%3==2 ? .5 : 0.;
}
inline int discrete(int id,double n) { return int(std::round(std::clamp(n,0.,1.)*stepsFor(id))); }
// The historical six-step parameter remains untouched. A live write to it
// releases the extension, including repeated automation values.
template<class S>inline int voiceChoice(const S&s){int extension=discrete(VoicesExtension,s[VoicesExtension]);if(extension)return extension+4;int old=discrete(Voices,s[Voices]);return old==5?6:old;}
template<class S>inline int voiceCount(const S&s){return VoiceChoices[voiceChoice(s)];}
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
 if(id==DriveExtension){std::snprintf(out,size,"%.1f%%",n*100);return;}
 if(id==VoicesExtension){choiceText("Follow voice count|32|64",discrete(id,n),out,size);return;}
 if(id>=PreDistortionCount){if(id==DistMode)choiceText("OFF|SOFT CLIP|SINE FOLD|HARMONIC|RECTIFY DC|SUB|FEEDBACK FOLD|COMB|WRAP|RATE|SCULPT",discrete(id,n),out,size);else if(id==DistPlace)choiceText("PRE|POST",discrete(id,n),out,size);else std::snprintf(out,size,"%.1f%%",n*100);return;}
 if(id>=PreDualCount){int k=discrete(id,n);if(id==Eg2Sync)choiceText("FREE|RETRIG|FIT|LIMIT",k,out,size);else if(id==Eg2Rate)choiceText("4/1|2/1|1/1|1/2|1/4|1/8|1/16|1/32|1/64|1/128",k,out,size);else if(id==VcfRoute)choiceText("A ONLY|B ONLY|A>B|B>A|A+B",k,out,size);else if(id==VcfBType)choiceText("LP|HP|BP|NOTCH",k,out,size);else if(id==VcfBSlope)std::snprintf(out,size,"%d dB/oct",k?24:12);else if(id>=BFilterRouteFirst)choiceText("Follow destination|VCF B Cutoff|VCF B Resonance",k,out,size);else if(id==VcfBCutoff)std::snprintf(out,size,"%.0f Hz",n*22000);else std::snprintf(out,size,"%.1f%%",n*100);return;}
 if(id==Sustain||id==Sostenuto){std::snprintf(out,size,"%s",n>=.5?"ON":"OFF");return;}
 if(id==Width){std::snprintf(out,size,"%.0f%%",n*100);return;}
 int k=discrete(id,n);
 if(id>=RasterFirst){std::snprintf(out,size,"%s",k?"RASTER":"Follow engine");return;}
 if(id>=PositionRouteFirst){std::snprintf(out,size,"%s",k?Destinations[41+k]:"Follow destination");return;}
 if(id>=OscSourceFirst){int f=(id-OscSourceFirst)%6; if(f==0)choiceText("WAVE|ROM|USER",k,out,size);else if(f==4)choiceText("HOLD|LINEAR|BAND",k,out,size);else if(f==5)choiceText("STEP|MORPH",k,out,size);else if(f==3)std::snprintf(out,size,"%.1f%%",n*100);else std::snprintf(out,size,"%d",k);return;}
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
