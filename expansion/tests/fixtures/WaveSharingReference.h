// GPL-2.0-or-later. Fixed-pool digital synthesis expansion, no realtime allocation.
#pragma once
#include <array>
#include <memory>
#include "Distortion.h"
#include "TableOscillator.h"
#include "Eg2Sync.h"
#include "DualVcfState.h"
#include "TableBatch.h"
#include "TableHoldBatch.h"
#include "PhaseWrap.h"
#include "DiscreteDiagnostics.h"
#include <emmintrin.h>
#if defined(_MSC_VER)
#include <intrin.h>
#endif

namespace xs {
#ifndef XSYNTH_MEMBER_CAPACITY
#define XSYNTH_MEMBER_CAPACITY 64
#endif
class Engine : public LegacyEngine {
  static constexpr int MemberCapacity=XSYNTH_MEMBER_CAPACITY;
  static_assert(MemberCapacity>=32&&MemberCapacity<=64&&MemberCapacity%16==0,"bounded member pool");
  struct BatchMask {
    std::array<uint32_t,(MemberCapacity+31)/32> words{};
    bool test(int i)const{if constexpr(MemberCapacity==32)return (words[0]&(1u<<i))!=0;else return (words[i/32]&(1u<<(i%32)))!=0;}
    void set(int i,uint32_t bits){if constexpr(MemberCapacity==32)words[0]|=bits<<i;else words[i/32]|=bits<<(i%32);}
  };
  friend struct DspValidationProbe;
  friend struct PreUnisonProbe;
  friend struct UnisonProbe;
  friend struct FourProbe;
  friend struct WidthProbe;
  friend struct StealProbe;
  // Long envelopes must keep sub-float increments through sustain/loop edges.
  struct Envelope { double value=0, start=0, progress=0; int stage=0; };
  struct Voice {
    float phase[2]{},highFilter[4]{};
    float highG=0,highStep=0;
    float resonanceStrength=0,resonanceStep=0;
    float cachedCutoff=-1,cachedHighCoefficient=0,cachedDamping=-1,cachedCalibration=1;
    int cachedFilterType=-1,cachedFilterStages=0;
    SignalBlend<6> waves[2],mix;
    SignalBlend<3> engines[2];SignalBlend<2> slope;
    table::Oscillator tables[2];
    uint64_t tableGeneration[2]{};
    SignalBlend<4> topology;
    std::array<std::array<float,2>,4> topologyPhase{};
    std::array<bool,4> topologyWrapped{};
    SignalBlend<3> cross;
    SignalBlend<4> types;
    std::array<int,23> selection=[] {std::array<int,23> a{};a.fill(-1);return a;}();
    int envCurve[2]{1,1},egLoop=0;
    float egVelocity[2]{},egPitch[2]{},egFilter[2]{},lfoPitch=0,lfoFilter=0;
    float tracking=1,tune=1,glideRate=1,audibility=1;
    bool retiring=false,firstSample=true;
    Envelope eg[2]{};
    std::array<float,DestinationCount> mod{},stepTarget{},stepValue{},stepDelta{};
    std::array<unsigned char,DestinationCount> stepRemaining{};
    bool modSmoothing=false;
    uint64_t controlDestinations=0;
    float pitchRatio[2]{1,1}, pitchStep[2]{};
    float envelopeRate[2][3]{}, envelopeStep[2][3]{};
    float hz=0, targetHz=0, velocity=1, random=0;
    float damping=2, dampingStep=0;
    int note=0, noteId=-1, control=0;
    int group=-1,member=0,members=1;
    float detunePosition=0,spread=0;
    float cachedSpread=-1,cachedDetunePosition=-2,cachedDetuneRatio=1;
    uint32_t age=0;
    bool active=false, gate=false, glide=false, masterWrapped=false;
    bool keyDown=true,pedalLatched=false;
    uint32_t renderedSamples=0;
    int handoffRemaining=0,handoffTotal=0;
    uint32_t choicesGeneration=0;
    eg2::Runtime syncedEg2;
    DualFilterState dual;
    std::array<std::array<float,2>,4> previousPulseDuty{{{{-1,-1}},{{-1,-1}},{{-1,-1}},{{-1,-1}}}};
  };
  table::Selection sourceSelection[2];
  uint64_t sourceSelectionGeneration=0;
  int sourceKind[2]{};const table::Table* selectedTables[2]{};
  const table::Bank* rom=nullptr;const table::Document* document=nullptr;int documentOwner=-1;
  std::array<Voice,MemberCapacity> pool{};
  int poolEnd=0;bool scanBoundsEnabled=true;
  int renderEnd()const{return scanBoundsEnabled?poolEnd:MemberCapacity;}
  int renderGroups()const{return scanBoundsEnabled?(poolEnd+groupSize-1)/groupSize:MemberCapacity;}
  std::array<bool,128> held{};
  std::array<float,128> velocities{};
  std::array<float,24> p{};
  float sr=44100, invSr=1/44100.f, lfoPos=0, lfoValue=0, randomA=0, randomB=0;
  float baseRates[2][3]{}, glideCoefficient=1, tuneBend=1, gain=1, lastHz=0;
  uint32_t seed=0x7853796e, serial=0,firstLfoSeed=0x4c464f31;
  double tempo=0, beat=0,eg2Tempo=120;
  int eg2Policy=0,eg2Division=5;
  bool tempoValid=false, beatValid=false, playing=false, expanded=false, changed=true;
  bool expandedLive=false,hostAnchor=true,forceExpanded=false;
  int previousSync=0,previousSlope=0;
  double lfoPhaseAccumulator=0;
  int transitionSamples=88;
  int effectivePhase=-1,effectivePolarity=-1;
  SignalBlend<2> polarityMix;
  SignalBlend<2> phaseMix;
  double previousLfoPhase=0;
  int voiceMode=0, voiceLimit=8, key=0;
  uint64_t expansionBits=0;
  uint32_t coefficients=(1u<<24)-1;
  bool tuningDirty=true;
  int waveKind[2]{}, engineKind[2]{}, crossMode=0, mixerKind=0, filterKind=0, filterStages=1;
  int curveKind[2]{1,1}, loopKind=0, lfoKind=0, randomKind=0, phaseKind=0;
  bool reverse=false,matrixActive=false,sampleControl=false,globalXmodOnly=false,globalPositionOnly=false,sparseRenderControl=false,filterLfoActive=false;
  int lfoTick=0;
  float lfoIncrement=0,lfoIncrementStep=0,lfoFactor=1;
  struct Route {int source=0,destination=0;float amount=0;};
  std::array<Route,12> activeRoutes{};
  int routeCount=0;
  uint64_t routeDestinations=0;
  bool routesDirty=true, lfoRateRouted=false,lfoFeedbackMayExist=true;
  static constexpr uint64_t LfoFeedbackMask=(uint64_t(1)<<10)|(uint64_t(1)<<26)|(uint64_t(1)<<27);
  float preLimiterOutput=0;
  unsigned numericalFaults=0;
  float velocitySensitivity[2]{};
  float random() {
    seed^=seed<<13; seed^=seed>>17; seed^=seed<<5;
    return float(seed>>8)*(2.f/16777216.f)-1.f;
  }
  static float flush(float v) { return std::abs(v)<1.e-20f?0:v; }
  static float wrap(float v) { return v>0&&v<1?v:v-std::floor(v); }
  static float sine(float phase) { return LegacyEngine::sineSample(wrap(phase)); }
  static void seedPulsePath(Voice&v,int target,int dominant) {
    v.previousPulseDuty[target]=v.topology.target<0?std::array<float,2>{-1,-1}:v.previousPulseDuty[dominant];
    for(auto&r:v.tables){r.current.previousPulseDuty[target]=r.current.previousPulseDuty[dominant];r.target.previousPulseDuty[target]=r.target.previousPulseDuty[dominant];}
  }
  static float raw(float phase,int wave,float shape) {
    switch(wave) {
    case 0:return sine(phase);
    case 1:return phase<.25f?4*phase:phase<.75f?2-4*phase:4*phase-4;
    case 2:return 2*phase-1;
    case 3:return 1-2*phase;
    case 4:return phase<.5f?1.f:-1.f;
    default:return phase<oscillator::pulseDuty(shape)?1.f:-1.f;
    }
  }
  static float analyticRendered(float phase,float delta,int wave,float shape,int renderer){
    if(renderer==2){phase=std::floor(oscillator::wrap(phase)*32)/32;if(wave==5)return phase<std::clamp(oscillator::pulseDuty(shape),1.f/32,31.f/32)?1.f:-1.f;}
    float sample=raw(phase,wave,shape);return renderer==1?oscillator::clean(phase,delta,wave,shape,sample):sample;
  }
  float oscillator(Voice &v,int o,float hz,float phaseMod,float shape,bool &wrapped,int path=0,bool silent=false,const Voice*noteState=nullptr) {
    int source=sourceKind[o];auto&reader=v.tables[o];
    if(source||reader.ready){
      if(!source||!reader.ready||v.tableGeneration[o]!=sourceSelectionGeneration){auto selected=sourceSelection[o];
      if(!source){selected.wave=v.waves[o].target<0?waveKind[o]:v.waves[o].target;selected.renderer=engineKind[o];}
      if(!reader.ready){auto initial=selected;if(!v.firstSample){initial.source=0;initial.data=nullptr;initial.owner=-1;initial.wave=waveKind[o];initial.renderer=engineKind[o];}reader.reset(initial,v.topologyPhase,o,sr);}
      reader.request(selected);v.tableGeneration[o]=sourceSelectionGeneration;}
      if(source||reader.active||reader.current.selection.source){
        float position=std::clamp(float(state[oscParam(o,3)])+(noteState?noteState->mod[42+o]:v.mod[42+o]),0.f,1.f);
        float sample=reader.read(path,hz,phaseMod,position,shape,sr,wrapped,[](float p,float delta,int wave,float shape,int renderer){return analyticRendered(p,delta,wave,shape,renderer);},silent);
        v.phase[o]=reader.phase(path);v.previousPulseDuty[path][o]=-1;return sample;
      }reader.ready=false;
    }
    const float delta=hz*invSr;
    const float unwrapped=v.phase[o]+delta;
    wrapped=unwrapped>=1 || unwrapped<0;
    v.phase[o]=wrap(unwrapped);
    float phase=wrap(v.phase[o]+phaseMod);
    auto &waves=v.waves[o];auto &engines=v.engines[o];
    if(waves.target<0)waves.reset(waveKind[o]);
    if(engines.target<0)engines.reset(engineKind[o]);
    float previousDuty=v.previousPulseDuty[path][o],duty=oscillator::pulseDuty(shape);
    v.previousPulseDuty[path][o]=(waves.target==5||waves.weights[5]!=0)?duty:-1;
    auto correction=[&](float sample,int wave){return wave==5?oscillator::movingPulse(sample,phase,delta,duty,previousDuty):sample;};
    if(!waves.remaining&&!engines.remaining) {
      float sample=raw(phase,waves.target,shape);
      return engines.target==2?analyticRendered(phase,delta,waves.target,shape,2):engines.target?correction(oscillator::clean(phase,delta,waves.target,shape,sample),waves.target):sample;
    }
    float sample=0;
    for(int k=0;k<6;++k)if(waves.weights[k]!=0){
      float x=raw(phase,k,shape),y=correction(oscillator::clean(phase,delta,k,shape,x),k);
      sample+=waves.weights[k]*(engines.weights[0]*x+engines.weights[1]*y);
      if(engines.weights[2]!=0)sample+=waves.weights[k]*engines.weights[2]*analyticRendered(phase,delta,k,shape,2);
    }
    return sample;
  }

  void classify() {
    expansionBits=0;
    for(int i=36;i<PreWavetableCount;++i) if(i!=Detune && state[i]!=extraDefault(i))expansionBits=1;
    for(int o=0;o<2;++o)if(state[oscParam(o,0)]!=0||state[RasterFirst+o]>=.5)expansionBits=1;
    if(state[VcfRoute]!=0||(state[Eg2Sync]!=0&&state[Loop2]!=0)||state[DistMode]!=0||state[VoicesExtension]!=0)expansionBits=1;
    expanded=expansionBits!=0;
  }
  int highest() const { for(int n=96;n>=21;--n)if(held[n])return n; return 0; }
  void trigger(Voice &v,int n,float velocity,bool legato,int id) {
    poolEnd=std::max(poolEnd,int(&v-pool.data())+1);
    bool connected=v.active && v.gate;if(!v.active&&distortionControl.live())clearDistortion(v);v.retiring=false;v.audibility=1;
    float oldHz=v.hz>0?v.hz:lastHz;
    if(!connected || !legato) {v.syncedEg2={}; v.eg[0].stage=v.eg[1].stage=0; v.eg[0].progress=v.eg[1].progress=0; v.eg[0].start=v.eg[0].value; v.eg[1].start=v.eg[1].value; }
    if(!v.active){v.dual.route=filterRoute;v.dual.remaining=0;}
    v.note=n; v.noteId=id; v.keyDown=true;v.pedalLatched=false;v.targetHz=440*std::exp2((n-69)/12.f);
    lastHz=v.targetHz;
    int mode=discrete(GlideMode,state[GlideMode]);
    bool any=key!=0;
    v.glide=mode==1 || (mode==2 && any) || (mode==3 && !any);
    v.hz=v.glide && oldHz>0?oldHz:v.targetHz;
    v.glideRate=glideCoefficient;v.velocity=velocity; v.random=random(); v.gate=v.active=true; v.age=++serial; v.control=0;
    for(int o=0;o<2;++o) {
      v.pitchRatio[o]=tuneBend*p[o?2:0];v.pitchStep[o]=0;
      for(int j=0;j<3;++j) {v.envelopeRate[o][j]=baseRates[o][j];v.envelopeStep[o][j]=0;}
    }
    for(int l=1;l<4;++l)if(discrete(lfoParam(l,5),state[lfoParam(l,5)])==1){extraLfos[l-1].lfoPos=0;extraLfos[l-1].lfoPhaseAccumulator=0;}
    if(effectivePhase==1 || (effectivePhase<0&&discrete(LfoPhase,state[LfoPhase])==1)){lfoPos=0;lfoPhaseAccumulator=0;}
  }
  static void release(Voice &v) {
    v.gate=false;
    for(auto &e:v.eg) { e.stage=2; e.progress=0; e.start=e.value; }
  }
#include "StereoWidth.h"
#include "UnisonGroups.h"
#include "UnisonGain.h"
#include "WaveSharingReferenceSharedNote.h"
#include "VoiceStealing.h"
#include "Pedals.h"
#include "DistortionEngine.h"
  // Invalidate constant choices on every prepare, independently of control
  // ticks. Do not reset that counter: it also clocks pitch/filter interpolation.
  uint32_t choicesGeneration=1;
  void prepare() {
    if(!++choicesGeneration){choicesGeneration=1;for(auto&v:pool)v.choicesGeneration=0;for(auto&v:topologyVoices)v.choicesGeneration=0;for(auto&r:retirePool)for(auto&v:r.voices)v.choicesGeneration=0;}

    lfoFeedbackMayExist=true;
    unshareNotes();if(noteSharingNeedsSettle)for(auto&group:groups)if(group.active)group.sharingDelay=transitionSamples+8;noteSharingNeedsSettle=false;
    for(int i=0;i<24;++i)if(coefficients&(1u<<i))p[i]=float(physical(i,state[i]));
    for(int e=0;e<2;++e) for(int j=0;j<3;++j) {
      int id=(e?15:9)+(j==2?3:j);
      if(!(coefficients&(1u<<id)))continue;
      float c=p[id];
      baseRates[e][j]=float(-std::expm1(std::log1p(-c)*44100/sr));
    }
    if(coefficients&(1u<<8))glideCoefficient=p[8]>=1?1:float(-std::expm1(std::log1p(-p[8])*44100/(256*sr)));
    int range=BendRanges[discrete(BendRange,state[BendRange])];
    if(tuningDirty)tuneBend=std::exp2(float(((state[30]*2-1)*range+(state[MasterTune]*2-1))/12));
    if(coefficients&(1u<<23))gain=std::pow(p[23],float(1/(2*std::log10(2.))));
    coefficients=0;tuningDirty=false;
    int mode=discrete(VoiceMode,state[VoiceMode]);
    const int previousLimit=voiceLimit;
    voiceLimit=std::min(MemberCapacity,voiceCount(state));
    // Pedal-only notes must not become held keys when leaving polyphony.
    if(mode!=2&&voiceMode==2){
      if(groupLive){for(auto&i:intents){if(!i.keyDown)i.held=false;i.pedalLatched=false;}refreshHeld();}
      else for(auto&v:pool)if(v.active&&!v.keyDown)release(v);
    }
    if(mode!=2&&voiceMode==2&&groupLive)for(int g=0;g<groupLimit;++g)if(groups[g].active&&!pool[g*groupSize].keyDown)releaseGroup(g);
    if(state[Unison]>0 && !groupLive)enableGroups();
    if(groupLive){
      int count=std::min(voiceLimit,UnisonCounts[discrete(Unison,state[Unison])]);
      if(count!=groupSize || mode!=voiceMode || previousLimit!=voiceLimit)if(!rebuildGroups(count,mode)){voiceLimit=previousLimit;mode=voiceMode;}
    }
    if(!groupLive && mode!=voiceMode && expandedLive) {
      // Keep the selected sounding voice's oscillator/envelope/filter history.
      // Other voices retire individually; never clear or mute the whole pool.
      if(mode!=2) {
        int wanted=highest(),chosen=-1;
        for(int i=0;i<MemberCapacity;++i)if(pool[i].active && pool[i].gate && pool[i].note==wanted){chosen=i;break;}
        if(chosen>0){std::swap(pool[0],pool[chosen]);if(distortionControl.live())std::swap((*distortionBank)[0],(*distortionBank)[chosen]);}
        if(chosen<0&&wanted){pool[0]={};trigger(pool[0],wanted,velocities[wanted],false,-1);pool[0].audibility=0;}
        for(int i=1;i<MemberCapacity;++i)if(pool[i].active)pool[i].retiring=true;
      } else for(int n=21;n<=96;++n)if(held[n]){
        bool found=false;for(auto &v:pool)if(v.active&&v.gate&&v.note==n){v.retiring=false;found=true;}
        if(!found)for(int i=0;i<voiceLimit;++i)if(!pool[i].active||pool[i].retiring){pool[i]={};trigger(pool[i],n,velocities[n],false,-1);pool[i].audibility=0;break;}
      }
    }
    voiceMode=mode;
    int route=discrete(VcfRoute,state[VcfRoute]);if(route!=filterRoute){dualEverUsed=true;dualControlFrames=2*transitionSamples;}filterRoute=route;dualEverUsed|=route!=0;selectNetwork();
    if(filterRoute||dualControlFrames){bCutoffBase=float(physical(21,state[VcfBCutoff]));bNominalBase=float(physical(22,state[VcfBResonance]));bTrackingBase=float(state[VcfBKeytrack]);bTypeKind=discrete(VcfBType,state[VcfBType]);bFilterStages=state[VcfBSlope]>=.5?2:1;}
    widthTargets();
    if(!groupLive && previousLimit!=voiceLimit)for(int i=voiceMode==2?voiceLimit:1;i<MemberCapacity;++i)if(pool[i].active)pool[i].retiring=true;
    std::array<table::Selection,2> previousSelections{sourceSelection[0],sourceSelection[1]};
    for(int o=0;o<2;++o){
      sourceKind[o]=discrete(oscParam(o,0),state[oscParam(o,0)]);selectedTables[o]=nullptr;
      if(sourceKind[o]==1&&rom){int id=discrete(oscParam(o,1),state[oscParam(o,1)]);for(const auto&t:rom->tables)if(t.id==id){selectedTables[o]=&t;break;}}
      if(sourceKind[o]==2&&document){auto&t=document->slots[discrete(oscParam(o,2),state[oscParam(o,2)])];if(t)selectedTables[o]=&t->playback;}
      waveKind[o]=discrete(24+o,state[24+o]);engineKind[o]=state[RasterFirst+o]>=.5?2:state[Engine1+o]>=.5;curveKind[o]=discrete(o?Curve2:Curve1,state[o?Curve2:Curve1]);
      auto&selection=sourceSelection[o];selection.source=sourceKind[o];selection.data=selectedTables[o];selection.owner=sourceKind[o]==2?documentOwner:-1;selection.wave=0;selection.renderer=0;selection.gain=sourceKind[o]==1?2.f:1.f; // One fixed ROM-only playback calibration; canonical data stay untouched.
      selection.sample=table::Sample(discrete(oscParam(o,4),state[oscParam(o,4)]));selection.frame=table::Frame(discrete(oscParam(o,5),state[oscParam(o,5)]));
    }
    // Unrelated note/geometry edits do not change immutable table selections.
    // Preserve pending reader transitions; invalidate only actual selection data.
    if(!sourceSelectionGeneration||sourceSelection[0]!=previousSelections[0]||sourceSelection[1]!=previousSelections[1])
      if(++sourceSelectionGeneration==0)++sourceSelectionGeneration;
    crossMode=discrete(XMode,state[XMode]);reverse=state[XDirection]>=.5;
    mixerKind=discrete(MixerMode,state[MixerMode]);filterKind=discrete(FilterType,state[FilterType]);filterStages=state[28]<.5?1:2;
    eg2Policy=discrete(Eg2Sync,state[Eg2Sync]);eg2Division=discrete(Eg2Rate,state[Eg2Rate]);
    loopKind=discrete(Loop2,state[Loop2]);lfoKind=discrete(26,state[26]);randomKind=discrete(LfoRandom,state[LfoRandom]);phaseKind=discrete(LfoPhase,state[LfoPhase]);
    if(routesDirty) {
      routeCount=0;routeDestinations=0;lfoRateRouted=false;
      for(int i=0;i<MatrixSlots;++i) {
        int id=matrixId(i),source=effectiveSource(state,i);
        float amount=float(state[id+2]*2-1);
        if(source && amount!=0) {
          int destination=renderDestination(state,routeDestination(state,i));
          activeRoutes[routeCount++]={source,destination,amount};
          routeDestinations|=uint64_t(1)<<destination;
          if(destination==10)lfoRateRouted=true;
        }
      }
      matrixActive=routeCount!=0;routesDirty=false;
      if(routeDestinations&LfoFeedbackMask)lfoFeedbackMayExist=true;
    }
    for(int l=1;l<4;++l)extraLfoRates[l-1]=float(physical(4,state[lfoParam(l,0)]));
    sampleControl=matrixActive||p[5]!=0||p[6]!=0;for(int l=1;l<4;++l)sampleControl|=state[lfoParam(l,1)]!=0||state[lfoParam(l,2)]!=0;
    filterLfoActive=p[6]!=0;for(int l=1;l<4;++l)filterLfoActive|=state[lfoParam(l,2)]!=0;
    velocitySensitivity[0]=float(state[Velocity1]);velocitySensitivity[1]=float(state[Velocity2]);
    globalXmodOnly=routeCount>0&&routeDestinations==(uint64_t(1)<<6);
    for(int i=0;i<routeCount;++i)globalXmodOnly&=activeRoutes[i].source==1||activeRoutes[i].source>=11;
    constexpr uint64_t positionDestinations=(uint64_t(1)<<42)|(uint64_t(1)<<43);
    globalPositionOnly=routeCount>0&&(routeDestinations&~positionDestinations)==0;
    for(int i=0;i<routeCount;++i)globalPositionOnly&=activeRoutes[i].source==1||activeRoutes[i].source>=11;
    sparseRenderControl=routeCount>0&&(routeDestinations&~uint64_t(15447925722102ull))==0;
    // Parameter edits still take effect on the very next control evaluation.
    if(globalXmodOnly||globalPositionOnly){
      for(auto&v:pool)if(v.active)v.control=0;
      if(topologyTailRemaining)for(auto&v:topologyVoices)if(v.active)v.control=0;
      for(auto&r:retirePool)if(r.count)for(int i=0;i<r.count;++i)r.voices[i].control=0;
    }
    noteSharingSafe=liveNoteSharingSafe=true;sharedDirectionControl=false;sharedDiscreteControls=false;
    for(int i=0;i<routeCount;++i){int d=activeRoutes[i].destination;
      // Direction changes share controls, but each member initializes its own
      // oscillator/sync phase bank from the pre-change dominant path.
      if(d==21)sharedDirectionControl=true;
      // Existing outgoing-tail lanes retain their original eligibility. Only
      // live groups use the newly verified direction-transition batching.
      if((d>=18&&d<=27)||d==33||d==37||d==38){
        noteSharingSafe=false; // Keep outgoing retirement/topology eligibility unchanged.
        bool commonDiscrete=d==20||d==22||d==27;
#ifdef XSYNTH_DISCRETE_DIAGNOSTICS
        commonDiscrete=commonDiscrete&&discreteModulationEnabled;
#endif
        sharedDiscreteControls|=commonDiscrete;
        if(d!=21&&!commonDiscrete)liveNoteSharingSafe=false;
      }
    }
    changed=false;
  }
  struct LfoRuntime {
 float lfoPos=0,randomA=0,randomB=0,lfoFactor=1;
 double lfoPhaseAccumulator=0,previousLfoPhase=0;
 int effectivePhase=-1,effectivePolarity=-1;bool hostAnchor=true;
 SignalBlend<2> polarityMix,phaseMix;
 uint32_t seed=0x4c464f32;
 };
 std::array<LfoRuntime,3> extraLfos{};
 std::array<float,4> lfoValues{};
 std::array<float,3> extraLfoRates{.1f,.1f,.1f};
 float lfo(int index=0) {
 auto &runtime=extraLfos[index?index-1:0];
 auto &randomSeed=index?runtime.seed:firstLfoSeed;
 auto &lfoPos=index?runtime.lfoPos:this->lfoPos;
 auto &randomA=index?runtime.randomA:this->randomA;auto &randomB=index?runtime.randomB:this->randomB;
 auto &lfoFactor=index?runtime.lfoFactor:this->lfoFactor;
 auto &lfoPhaseAccumulator=index?runtime.lfoPhaseAccumulator:this->lfoPhaseAccumulator;
 auto &previousLfoPhase=index?runtime.previousLfoPhase:this->previousLfoPhase;
 auto &effectivePhase=index?runtime.effectivePhase:this->effectivePhase;
 auto &effectivePolarity=index?runtime.effectivePolarity:this->effectivePolarity;
 auto &hostAnchor=index?runtime.hostAnchor:this->hostAnchor;
 auto &polarityMix=index?runtime.polarityMix:this->polarityMix;
 auto &phaseMix=index?runtime.phaseMix:this->phaseMix;
 int lfoKind=index?discrete(lfoParam(index,3),state[lfoParam(index,3)]):this->lfoKind;
 int randomKind=index?discrete(lfoParam(index,7),state[lfoParam(index,7)]):this->randomKind;
 float rateValue=index?extraLfoRates[index-1]:p[4];
 auto valueFor=[&](int field){return state[lfoParam(index,field)];};
    float previous=lfoPos;
    double duration=divisionBeats(valueFor(8));
    float rateMod=0,phaseMod=0,polarityMod=0;int count=0;
    if(!index){
      if(routeDestinations&LfoFeedbackMask){
        for(int i=0;i<renderEnd();++i)if(auto&v=pool[i];v.active&&!v.retiring){const auto&note=v.group>=0&&groups[v.group].noteShared?pool[v.group*groupSize]:v;rateMod+=note.mod[10];phaseMod+=note.mod[26];polarityMod+=note.mod[27];++count;}
      }else if(lfoFeedbackMayExist){
        // A removed route may still have a pending step tail. Skip the scan
        // only after all original averaging inputs and pending bits are zero.
        bool pendingFeedback=false;
        for(int i=0;i<renderEnd();++i)if(auto&v=pool[i];v.active&&!v.retiring){
          const auto&note=v.group>=0&&groups[v.group].noteShared?pool[v.group*groupSize]:v;
          rateMod+=note.mod[10];phaseMod+=note.mod[26];polarityMod+=note.mod[27];++count;
          uint32_t a,b,c;std::memcpy(&a,&note.mod[10],4);std::memcpy(&b,&note.mod[26],4);std::memcpy(&c,&note.mod[27],4);
          pendingFeedback|=(note.controlDestinations&LfoFeedbackMask)!=0||((a|b|c)&0x7fffffffu)!=0;
        }
        lfoFeedbackMayExist=pendingFeedback;
      }
    }
    if(index){rateMod=phaseMod=polarityMod=0;count=0;}
    if(count){rateMod/=count;phaseMod/=count;polarityMod/=count;}
    lfoFactor=rateMod!=0?std::exp2(std::clamp(rateMod,-4.f,4.f)*4):1.f;
    int phase=stableChoice(float(valueFor(5))+phaseMod,2,effectivePhase);
    int polarity=stableChoice(float(valueFor(6))+polarityMod,1,effectivePolarity);
    bool phaseEdit=effectivePhase>=0&&phase!=effectivePhase;
    if(phase!=effectivePhase){effectivePhase=phase;hostAnchor=true;}
    effectivePolarity=polarity;polarityMix.select(polarity,transitionSamples);polarityMix.advance();
    double rate=valueFor(4)<.5?rateValue:(tempoValid?tempo/(60*duration):0.);
    bool host=phase==2&&beatValid&&tempoValid;
    if(host && hostAnchor){
      if(phaseEdit){previousLfoPhase=lfoPhaseAccumulator;phaseMix.reset(0);phaseMix.select(1,transitionSamples);}
      double cycles=valueFor(4)>=.5?beat/duration:beat*60/tempo*rateValue;
      lfoPhaseAccumulator=cycles;hostAnchor=false;
    } else if(!host||playing)lfoPhaseAccumulator+=rate*lfoFactor/sr;
    // Accumulate frequency, never multiply absolute song position by changing
    // modulation. Double precision prevents slow-rate phase quantization.
    double cycles=lfoPhaseAccumulator;
    lfoPos=float(cycles-std::floor(cycles));
    if(randomKind && host){
      auto hash=[](uint32_t x){x^=x>>16;x*=0x7feb352du;x^=x>>15;x*=0x846ca68bu;x^=x>>16;return float(x>>8)*(2.f/16777216.f)-1.f;};
      uint32_t cycle=uint32_t(int64_t(std::floor(std::clamp(cycles,-1.e12,1.e12))));
      randomA=hash(cycle+uint32_t(index)*0x9e3779b9u);randomB=hash(cycle+1+uint32_t(index)*0x9e3779b9u);
    } else if(lfoPos<previous){randomA=randomB;randomSeed^=randomSeed<<13;randomSeed^=randomSeed>>17;randomSeed^=randomSeed<<5;randomB=float(randomSeed>>8)*(2.f/16777216.f)-1.f;}
    float value=randomKind==1?randomA:randomKind==2?randomA+(randomB-randomA)*lfoPos*lfoPos*(3-2*lfoPos):raw(lfoPos,lfoKind,.25f);
    if(phaseMix.remaining){
      previousLfoPhase+=rate*lfoFactor/sr;
      // Only phase-mode edits crossfade signals; ordinary continuous modulation
      // remains sample-clocked without added smoothing/timing lag.
      float previousValue=randomKind?value:raw(float(previousLfoPhase-std::floor(previousLfoPhase)),lfoKind,.25f);
      value=phaseMix.weights[0]*previousValue+phaseMix.weights[1]*value;phaseMix.advance();
    }
    return value*polarityMix.weights[0]+(value+1)*.5f*polarityMix.weights[1];
  }
  float envelopeLegacy(Voice &v,int index,int forcedLoop=-1) {
    auto &e=v.eg[index]; int offset=index?15:9;
    int curve=v.selection[index?19:15]<0?curveKind[index]:v.envCurve[index];
    int loop=forcedLoop>=0?forcedLoop:v.selection[20]<0?loopKind:v.egLoop;
    double coefficient=std::clamp(v.envelopeRate[index][e.stage],0.f,1.f);
    float target=e.stage==0?1:e.stage==1?p[offset+2]:0;
    if(curve==1) e.value=coefficient*target+(1-coefficient)*e.value;
    else {
      e.progress=std::min(1.,e.progress+coefficient/4.60517);
      double t=curve==2?e.progress*(2-e.progress):e.progress;
      e.value=e.start+(target-e.start)*t;
    }
    bool done=curve==1?std::abs(e.value-target)<(e.stage==0?.01f:1.e-5f):e.progress>=1;
    if(done) {
      if(e.stage==0) { e.stage=1; e.start=e.value; e.progress=0; }
      else if(index==1 && v.gate && loop && (e.stage==1 || e.stage==2)) {
        e.stage=e.stage==1 && loop==2?2:0;
        e.start=e.value; e.progress=0;
      }
    }
    if(std::abs(e.value)<1.e-20)e.value=0;
    float sensitivity=v.selection[index?19:15]<0?velocitySensitivity[index]:v.egVelocity[index];
    return float(e.value*(1-sensitivity+sensitivity*v.velocity));
  }
  float envelope(Voice&v,int index){
    XS_DISCRETE_COUNT(1,1);
    if(index==1){int loop=v.selection[20]<0?loopKind:v.egLoop;
      if(eg2Policy&&loop){int curve=v.selection[19]<0?curveKind[1]:v.envCurve[1];
        v.syncedEg2.next(v.eg[1],v.envelopeRate[1],curve,p[17],loop,eg2Policy,eg2Division,eg2Tempo,sr,v.gate,[&]{envelopeLegacy(v,1,0);});
        float sensitivity=v.selection[19]<0?velocitySensitivity[1]:v.egVelocity[1];return float(v.eg[1].value*(1-sensitivity+sensitivity*v.velocity));}
      if(v.syncedEg2.mode)v.syncedEg2.leave(v.eg[1]);
    }
    return envelopeLegacy(v,index);
  }
  void control(Voice&v,float lfoSample){controlBase(v,lfoSample);secondaryControl(v);}
  void controlBase(Voice &v,float lfoSample) {
    XS_DISCRETE_COUNT(0,1);
    // Global table-position routes require sample-rate position evaluation, but do not
    // change waveform/engine choices, curves or envelope coefficients. Keep
    // the full evaluation on the existing eight-sample ticks and after every
    // prepare(), including parameter edits and route changes. Never lower the
    // modulation cadence to obtain the saving.
    if(globalPositionOnly&&v.control&&!v.modSmoothing&&v.controlDestinations==routeDestinations){
      float position[2]{};
      for(int i=0;i<routeCount;++i){const auto&r=activeRoutes[i];position[r.destination-42]+=(r.source==1?lfoSample:lfoValues[r.source-10])*r.amount;}
      for(int o=0;o<2;++o)v.mod[42+o]=std::clamp(position[o],-4.f,4.f);
      float s0=v.selection[15]<0?velocitySensitivity[0]:v.egVelocity[0],s1=v.selection[19]<0?velocitySensitivity[1]:v.egVelocity[1];
      if(!filterLfoActive&&v.egFilter[0]==0&&v.egFilter[1]==0&&v.hz==v.targetHz&&v.cachedCutoff>=0)updateFilterSteps(v,v.cachedCutoff,0);
      else updateFilterControl(v,lfoSample,v.mod,float(v.eg[0].value*(1-s0+s0*v.velocity)),float(v.eg[1].value*(1-s1+s1*v.velocity)));
      return;
    }
    // A global LFO->XMOD-only route changes a render-time scalar, not the
    // oscillator choices, ADSR coefficients or filter destinations. Between
    // full control ticks preserve those cached values, while updating PM/FM
    // and the original filter-coefficient recursion at the original cadence.
    if(globalXmodOnly&&v.control&&!v.modSmoothing&&v.controlDestinations==(uint64_t(1)<<6)){
      float value=0;for(int i=0;i<routeCount;++i){const auto&r=activeRoutes[i];value+=(r.source==1?lfoSample:lfoValues[r.source-10])*r.amount;}
      v.mod[6]=std::clamp(value,-4.f,4.f);
      float s0=v.selection[15]<0?velocitySensitivity[0]:v.egVelocity[0],s1=v.selection[19]<0?velocitySensitivity[1]:v.egVelocity[1];
      if(!filterLfoActive&&v.egFilter[0]==0&&v.egFilter[1]==0&&v.hz==v.targetHz&&v.cachedCutoff>=0)updateFilterSteps(v,v.cachedCutoff,0);
      else updateFilterControl(v,lfoSample,v.mod,float(v.eg[0].value*(1-s0+s0*v.velocity)),float(v.eg[1].value*(1-s1+s1*v.velocity)));
      return;
    }
    std::array<float,DestinationCount> destinations{},continuous{};
    float sensitivity0=v.selection[15]<0?velocitySensitivity[0]:v.egVelocity[0];
    float sensitivity1=v.selection[19]<0?velocitySensitivity[1]:v.egVelocity[1];
    const float source[]={0,lfoSample,float(v.eg[0].value*(1-sensitivity0+sensitivity0*v.velocity)),
      float(v.eg[1].value*(1-sensitivity1+sensitivity1*v.velocity)),v.velocity,(v.note-60)/36.f,
      float(state[31]),float(state[Pressure]),float(state[30]*2-1),v.random,1,lfoValues[1],lfoValues[2],lfoValues[3]};
    for(int i=0;i<routeCount;++i) {
      const auto &route=activeRoutes[i];
      (route.source>=1&&route.source<=3||route.source>=11?continuous:destinations)[route.destination]+=source[route.source]*route.amount;
    }
    v.modSmoothing=false;
    // Removed routes retain their original eight-sample step tails. Each
    // destination is independent here; route summation above stays ordered.
    uint64_t pending=v.controlDestinations|routeDestinations;const uint64_t choiceInputs=pending;
    v.controlDestinations=routeDestinations;
    while(pending){
      int i=0;
#if defined(_MSC_VER)
      unsigned long bitIndex;_BitScanForward64(&bitIndex,pending);i=int(bitIndex);
#else
      i=__builtin_ctzll(pending);
#endif
      const uint64_t bit=uint64_t(1)<<i;pending&=~bit;
      if(destinations[i]!=v.stepTarget[i]){
        v.stepTarget[i]=destinations[i];v.stepRemaining[i]=8;v.stepDelta[i]=(destinations[i]-v.stepValue[i])*.125f;
      }
      if(v.stepRemaining[i]){v.stepValue[i]+=v.stepDelta[i];if(!--v.stepRemaining[i])v.stepValue[i]=v.stepTarget[i];}
      v.modSmoothing|=v.stepRemaining[i]!=0;
      destinations[i]=std::clamp(continuous[i]+v.stepValue[i],-4.f,4.f);
      v.mod[i]=destinations[i];
      if(v.stepRemaining[i]||v.stepValue[i]!=0||v.stepTarget[i]!=0)v.controlDestinations|=bit;
    }
    auto n=[&](int id,int destination){return std::clamp(float(state[id])+destinations[destination],0.f,1.f);};
    // Retain the original ordered routing/step tails
    // above, and sample-rate Direction/filter/EG-depth updates below. Cached
    // waveform/mode/curve and unmodulated rate calculations remain valid
    // between full ticks while the prepare generation is unchanged.
    if(sparseRenderControl&&sampleControl&&v.control&&!v.firstSample&&v.choicesGeneration==choicesGeneration&&(choiceInputs&~uint64_t(15447925722102ull))==0){
      v.selection[3]=stableChoice(n(XDirection,21),stepsFor(XDirection),v.selection[3]);
      int topology=v.selection[3]*2+(state[27]>=.5);
      if(topology!=v.topology.target){int dominant=0;for(int t=1;t<4;++t)if(v.topology.weights[t]>v.topology.weights[dominant])dominant=t;
        v.topologyPhase[topology]=v.topology.target<0?std::array<float,2>{v.phase[0],v.phase[1]}:v.topologyPhase[dominant];
      seedPulsePath(v,topology,dominant);
        v.topologyWrapped[topology]=v.topology.target<0?v.masterWrapped:v.topologyWrapped[dominant];}
      v.topology.select(topology,transitionSamples);
      for(int o=0;o<2;++o){v.egPitch[o]=n(o?19:13,o?34:30);v.egFilter[o]=destinations[o?35:31]==0?p[o?20:14]:50*n(o?20:14,o?35:31);}
      updateFilterControl(v,lfoSample,destinations,source[2],source[3]);return;
    }
    // Destination identities/step counts are static, not per-voice controls.
    static constexpr auto choices=[] {
      std::array<std::array<int,3>,12> result{};int count=0;
      for(int i=0;i<23;++i){int id=DestinationParams[18+i],steps=stepsFor(id);
        if(steps)result[count++]={i,id,steps};}
      return result;
    }();
    static constexpr auto choiceLookup=[] {
      std::array<std::array<int,3>,DestinationCount> result{};
      for(const auto&choice:choices)result[18+choice[0]]=choice;
      return result;
    }();
    static constexpr uint64_t choiceMask=[] {uint64_t result=0;for(const auto&choice:choices)result|=uint64_t(1)<<(18+choice[0]);return result;}();
    if(v.choicesGeneration!=choicesGeneration||v.control==0||v.firstSample){
      v.choicesGeneration=choicesGeneration;
      for(const auto&choice:choices){int i=choice[0];v.selection[i]=stableChoice(n(choice[1],18+i),choice[2],v.selection[i]);}
    }else{
      // Only constant, already-evaluated inputs are omitted. Active routes and
      // their final removed-route tail sample retain the original cadence.
      uint64_t changing=choiceInputs&choiceMask;
      while(changing){int destination=0;
#if defined(_MSC_VER)
        unsigned long bitIndex;_BitScanForward64(&bitIndex,changing);destination=int(bitIndex);
#else
        destination=__builtin_ctzll(changing);
#endif
        changing&=changing-1;
        const auto&choice=choiceLookup[destination];int i=choice[0];
        v.selection[i]=stableChoice(n(choice[1],destination),choice[2],v.selection[i]);
      }
    }
    for(int o=0;o<2;++o){
      v.waves[o].select(v.selection[o],transitionSamples);
      v.engines[o].select(engineKind[o],transitionSamples);
      int curve=v.selection[o?19:15];
      if(curve!=v.envCurve[o]){auto &e=v.eg[o];e.start=e.value;e.progress=0;v.envCurve[o]=curve;}
      v.egVelocity[o]=n(o?Velocity2:Velocity1,o?36:32);
      v.egPitch[o]=n(o?19:13,o?34:30);
      v.egFilter[o]=destinations[o?35:31]==0?p[o?20:14]:50*n(o?20:14,o?35:31);
    }
    v.cross.select(v.selection[2],transitionSamples);
    int topology=v.selection[3]*2+(state[27]>=.5);
    if(topology!=v.topology.target){
      int dominant=0;for(int t=1;t<4;++t)if(v.topology.weights[t]>v.topology.weights[dominant])dominant=t;
      v.topologyPhase[topology]=v.topology.target<0?std::array<float,2>{v.phase[0],v.phase[1]}:v.topologyPhase[dominant];
      seedPulsePath(v,topology,dominant);
      v.topologyWrapped[topology]=v.topology.target<0?v.masterWrapped:v.topologyWrapped[dominant];
    }
    v.topology.select(topology,transitionSamples);
    v.mix.select(v.selection[4],transitionSamples);
    v.types.select(v.selection[5],transitionSamples);
    v.slope.select(v.selection[6],transitionSamples);
    v.egLoop=v.selection[20];v.tracking=n(Keytrack,25);
    v.lfoPitch=n(5,28);v.lfoFilter=n(6,29);
    v.tune=destinations[40]==0?1.f:std::exp2((n(MasterTune,40)-float(state[MasterTune]))/6.f);
    float glide=destinations[39]==0?p[8]:float(physical(8,n(8,39)));
    v.glideRate=destinations[39]==0?glideCoefficient:glide>=1?1:float(-std::expm1(std::log1p(-glide)*44100/(256*sr)));
    for(int e=0;e<2;++e){
      float modulation=destinations[e?3:0];
      float ratio=tuneBend*p[e?2:0]*(modulation!=0?std::exp2(modulation*4):1.f)*v.tune;
      if(sampleControl){v.pitchRatio[e]=ratio;v.pitchStep[e]=0;}else v.pitchStep[e]=(ratio-v.pitchRatio[e])*.125f;
      for(int j=0;j<3;++j){float amount=destinations[(e?15:12)+j];
        float rate=std::clamp(baseRates[e][j]*(amount!=0?std::exp2(-4*amount):1.f),0.f,1.f);
        if(sampleControl){v.envelopeRate[e][j]=rate;v.envelopeStep[e][j]=0;}else v.envelopeStep[e][j]=(rate-v.envelopeRate[e][j])*.125f;
      }
    }
    updateFilterControl(v,lfoSample,destinations,source[2],source[3]);
  }
  void updateFilterControl(Voice&v,float lfoSample,const std::array<float,DestinationCount>&destinations,float envSource1,float envSource2){
    float hz=v.hz*tuneBend*v.tune;
    float tracked=v.tracking==1?hz:v.tracking==0?440:440*std::pow(std::max(.01f,hz/440),v.tracking);
    float filterLfo=lfoSample*v.lfoFilter*std::max(0.f,1+destinations[11]);for(int l=1;l<4;++l)filterLfo+=lfoValues[l]*float(state[lfoParam(l,2)]);
    float cutoff=tracked*(p[21]+v.egFilter[0]*envSource1+v.egFilter[1]*envSource2)*
      std::max(0.f,1+filterLfo)*(destinations[8]!=0?std::exp2(destinations[8]*4):1.f)*(1-float(state[31]));
    cutoff=std::clamp(cutoff,0.f,.49f*sr);
    updateFilterSteps(v,cutoff,destinations[9]);
  }
  void updateFilterSteps(Voice&v,float cutoff,float resonanceMod){
    float damping=std::clamp(p[22]-resonanceMod*1.5f,.005f,2.f);
    int type=v.types.target<0?filterKind:v.types.target;
    int stages=(v.slope.target<0?filterStages:v.slope.target+1);
    bool lawChanged=damping!=v.cachedDamping||type!=v.cachedFilterType||stages!=v.cachedFilterStages;
    if(lawChanged)v.cachedCalibration=highcut::productionRatio(damping,type,stages);
    if(cutoff!=v.cachedCutoff||lawChanged){
      v.cachedCutoff=cutoff;v.cachedDamping=damping;v.cachedFilterType=type;v.cachedFilterStages=stages;
      float g=highcut::coefficient(cutoff,sr);
      v.cachedHighCoefficient=type==0?g/v.cachedCalibration:type==1?g*v.cachedCalibration:g;
    }
    v.highStep=(v.cachedHighCoefficient-v.highG)*.125f;
    v.dampingStep=(highcut::resonanceDamping(damping)-v.damping)*.125f;
    v.resonanceStep=(highcut::resonanceStrength(damping)-v.resonanceStrength)*.125f;
  }

  float filter(Voice &v,float input,const Voice*shared=nullptr) {
    const Voice&coefficient=shared?*shared:v;
    if(v.types.target<0)v.types.reset(filterKind);
    if(v.slope.target<0)v.slope.reset(filterStages-1);
    int stages=v.slope.remaining?2:v.slope.target+1;
    float highInput=input;
    float highFirst=0;
    double alpha=coefficient.resonanceStrength>0?highcut::radialAlpha(coefficient.highG,coefficient.resonanceStrength):0;
    for(int i=0;i<stages;++i){float outputs[4]{};
      float damping=coefficient.damping;
      if(coefficient.resonanceStrength>0&&i==0&&stages==2){float firstWeight=v.slope.remaining?v.slope.weights[0]:0;damping=std::max(.005f,damping)*(1-firstWeight)+damping*firstWeight;}
      highcut::stage(highInput,coefficient.highG,damping,v.highFilter+2*i,outputs);
      if(alpha>0)highcut::radialState(v.highFilter+2*i,alpha);highInput=0;
      if(!v.types.remaining)highInput=outputs[v.types.target];else for(int j=0;j<4;++j)highInput+=outputs[j]*v.types.weights[j];if(i==0)highFirst=highInput;}
    float highPath=v.slope.remaining?highFirst*v.slope.weights[0]+highInput*v.slope.weights[1]:highInput;
    return highPath;
  }

  struct PreparedVoice {float input=0,env1=0;};
  float finishVoice(Voice&v,float sample,const Voice&note){
      if(!std::isfinite(sample)) { ++numericalFaults;if(v.group>=0)groups[v.group].fault=true;v={};return 0; }
      if(v.retiring){v.audibility=std::max(0.f,v.audibility-1.f/transitionSamples);}
      else if(v.audibility<1)v.audibility=std::min(1.f,v.audibility+1.f/transitionSamples);
    float weighted=sample*v.audibility;
    if(v.handoffRemaining){float t=float(v.handoffTotal-v.handoffRemaining)/std::max(1,v.handoffTotal-1);weighted*=handoffIn(t);--v.handoffRemaining;}
    if((v.retiring&&v.audibility==0)||(!v.gate&&note.eg[0].value<1.e-6f)){if(v.group>=0&&groups[v.group].noteShared)groups[v.group].noteFinished=true;else v={};}
    return weighted;
  }
  template<bool Sharing> float renderVoicePath(Voice &v,Voice*common){
    Voice*shared=Sharing?common:nullptr;
    Voice&note=shared?*shared:v;
    if(!v.active)return 0;
    XS_DISCRETE_COUNT(4,1);
    if(v.renderedSamples!=UINT32_MAX)++v.renderedSamples;
      if(!shared&&v.group>=0&&groups[v.group].noteShared&&sharedDirectionControl)groups[v.group].topologyDominant=topologyDominant(v);
      if(shared&&sharedDirectionControl){adoptSharedTopology(v,*shared);v.resonanceStrength=shared->resonanceStrength;v.resonanceStep=shared->resonanceStep;}
      if(!shared){v.hz=v.glide?v.hz+v.glideRate*(v.targetHz-v.hz):v.targetHz;
      if(sampleControl||v.modSmoothing||v.control==0)control(v,lfoValue);
      for(int o=0;o<2;++o){v.waves[o].advance();v.engines[o].advance();}
      v.cross.advance();v.topology.advance();v.mix.advance();v.types.advance();v.slope.advance();advanceSecondary(v);
      for(int e=0;e<2;++e) {
        v.pitchRatio[e]+=v.pitchStep[e];
        for(int j=0;j<3;++j)v.envelopeRate[e][j]+=v.envelopeStep[e][j];
      }
      v.control=(v.control+1)&7;}
      float env1=shared?groups[v.group].envelopeValue[0]:envelope(v,0), env2=shared?groups[v.group].envelopeValue[1]:envelope(v,1);
      if(!shared&&v.group>=0&&groups[v.group].noteShared){groups[v.group].envelopeValue[0]=env1;groups[v.group].envelopeValue[1]=env2;}
      float lfoDepth=std::max(0.f,1+note.mod[11]);
      float detuneRatio=1;
      if(v.members>1||v.detunePosition!=0){
        float target=50*std::clamp(float(state[Detune])+note.mod[41],0.f,1.f);
        v.spread+=(target-v.spread)/transitionSamples;
        if(v.spread!=v.cachedSpread||v.detunePosition!=v.cachedDetunePosition){v.cachedSpread=v.spread;v.cachedDetunePosition=v.detunePosition;v.cachedDetuneRatio=std::exp2(v.detunePosition*v.spread/2400.f);}
        detuneRatio=v.cachedDetuneRatio;
      }
      float directPitch=lfoValue*lfoDepth*note.lfoPitch;for(int l=1;l<4;++l)directPitch+=lfoValues[l]*float(state[lfoParam(l,1)]);
      float directRatio=std::max(0.f,1+directPitch);
      float hz1=note.hz*note.pitchRatio[0]*detuneRatio*directRatio;
      float hz2=note.hz*note.pitchRatio[1]*detuneRatio*std::max(0.f,1+env1*note.egPitch[0])*std::max(0.f,1+env2*note.egPitch[1])*
        directRatio;
      float shape1=std::clamp(p[1]+note.mod[1],0.f,1.f), shape2=std::clamp(p[3]+note.mod[4],0.f,1.f);
      float depth=std::clamp(float(state[XDepth])+note.mod[6],0.f,1.f);
      float fm=8*depth*note.cross.weights[1],pm=4*depth*note.cross.weights[2];
      float o1=0,o2=0;
      float balance=std::clamp(p[7]+note.mod[7],0.f,1.f);
      auto oscillators=[&](Voice &path,bool reversed,bool sync,float &a,float &b,int t){
        bool wa=false,wb=false;
        bool skipB=sourceKind[1]!=0&&balance==0&&fm==0&&pm==0&&!sync;
        if(!reversed){a=oscillator(path,0,hz1,0,shape1,wa,t,false,&note);if(sync&&wa){path.phase[1]=0;path.tables[1].sync(t);}
          b=oscillator(path,1,hz2*(1+fm*a),pm*a,shape2,wb,t,skipB,&note);
        }else{if(sync&&path.masterWrapped){path.phase[1]=0;path.tables[1].sync(t);}b=oscillator(path,1,hz2,0,shape2,wb,t,skipB,&note);
          a=oscillator(path,0,hz1*(1+fm*b),pm*b,shape1,wa,t,false,&note);}
        path.masterWrapped=wa;
      };
      auto path=[&](int t,float weight){
        std::copy_n(v.topologyPhase[t].data(),2,v.phase);v.masterWrapped=v.topologyWrapped[t];
        float a=0,b=0;oscillators(v,t/2!=0,t%2!=0,a,b,t);
        std::copy_n(v.phase,2,v.topologyPhase[t].data());v.topologyWrapped[t]=v.masterWrapped;
        o1+=weight*a;o2+=weight*b;
      };
      if(note.topology.remaining){for(int t=0;t<4;++t)if(note.topology.weights[t]!=0)path(t,note.topology.weights[t]);}
      else path(note.topology.target,1);
      std::copy_n(v.topologyPhase[note.topology.target].data(),2,v.phase);v.masterWrapped=v.topologyWrapped[note.topology.target];
      for(auto&t:v.tables)t.advance();v.firstSample=false;
      o1*=std::clamp(1+note.mod[2],0.f,2.f);o2*=std::clamp(1+note.mod[5],0.f,2.f);
      float b=std::clamp(p[7]+note.mod[7],0.f,1.f), input=0;
      auto mix=[&](int kind){switch(kind){
        case 0:return (1-b)*o1+b*o2;
        case 1:return .5f*(std::min(1.f,2*(1-b))*o1+std::min(1.f,2*b)*o2);
        case 2:return (1-b)*o1+b*o1*o2;
        case 3:return (1-b)*o1+b*o1*(o2+1)*.5f;
        case 4:return (1-b)*o1+b*((o1>=0)!=(o2>=0)?1.f:-1.f);
        default:return (1-b)*o1-b*o2;}};
      if(!note.mix.remaining)input=mix(note.mix.target);else for(int kind=0;kind<6;++kind)input+=note.mix.weights[kind]*mix(kind);
      if(!shared){v.damping=std::clamp(v.damping+v.dampingStep,-.04f,2.f);
      v.resonanceStrength=std::clamp(v.resonanceStrength+v.resonanceStep,0.f,1.f);
      v.highG=std::max(0.f,v.highG+v.highStep);}
      float sample=networkSample(v,input,shared)*env1;
      if(!std::isfinite(sample)) { ++numericalFaults;if(v.group>=0)groups[v.group].fault=true;v={};return 0; }
      if(v.retiring){v.audibility=std::max(0.f,v.audibility-1.f/transitionSamples);}
      else if(v.audibility<1)v.audibility=std::min(1.f,v.audibility+1.f/transitionSamples);
    float weighted=sample*v.audibility;
    if(v.handoffRemaining){float t=float(v.handoffTotal-v.handoffRemaining)/std::max(1,v.handoffTotal-1);weighted*=handoffIn(t);--v.handoffRemaining;}
    if((v.retiring&&v.audibility==0)||(!v.gate&&note.eg[0].value<1.e-6f)){if(v.group>=0&&groups[v.group].noteShared)groups[v.group].noteFinished=true;else v={};}
    return weighted;
  }
  float renderVoice(Voice&v,Voice*shared=nullptr){
    return shared?renderVoicePath<true>(v,shared):renderVoicePath<false>(v,nullptr);
  }
  // SIMD lanes represent distinct members. No horizontal reduction, reciprocal
  // approximation, FMA, skipped filter state or reordered member summation.
  template<int N,class V> static __m128 gatherFilter(V**v,int field){
    auto at=[&](int i){return v[i]->highFilter[field];};
    if constexpr(N==4)return _mm_set_ps(at(3),at(2),at(1),at(0));
    else return _mm_set_ps(0,at(2),at(1),at(0));
  }
  template<int N,class V> static void storeFilter(V**v,int field,__m128 x){
    alignas(16) float lanes[4];_mm_store_ps(lanes,x);
    for(int i=0;i<N;++i)v[i]->highFilter[field]=lanes[i];
  }
  static __m128 filterOutput4(int kind,__m128 low,__m128 high,__m128 band){switch(kind){case 0:return low;case 1:return high;case 2:return band;default:return _mm_add_ps(low,high);}}
  template<int N,class V> static void filterBatch(V**v,const PreparedVoice*p,const V&note,float*output){
    __m128 input;
    if constexpr(N==4)input=_mm_set_ps(p[3].input,p[2].input,p[1].input,p[0].input);
    else input=_mm_set_ps(0,p[2].input,p[1].input,p[0].input);
    __m128 highInput=input,g=_mm_set1_ps(note.highG),damping=_mm_set1_ps(note.damping);
    int stages=note.slope.target+1,kind=note.types.target;
    auto two=_mm_set1_ps(2.f);
    if(note.resonanceStrength<=0){
      // Preserve the accepted hot loop verbatim below the approved boundary.
      auto denominator=_mm_add_ps(_mm_set1_ps(1.f),_mm_mul_ps(g,_mm_add_ps(g,damping)));
      for(int i=0;i<stages;++i){auto oldBand=gatherFilter<N>(v,i*2),oldLow=gatherFilter<N>(v,i*2+1);
        auto band=_mm_div_ps(_mm_add_ps(oldBand,_mm_mul_ps(g,_mm_sub_ps(highInput,oldLow))),denominator);
        auto low=_mm_add_ps(oldLow,_mm_mul_ps(g,band));
        storeFilter<N>(v,i*2,_mm_sub_ps(_mm_mul_ps(two,band),oldBand));storeFilter<N>(v,i*2+1,_mm_sub_ps(_mm_mul_ps(two,low),oldLow));
        auto high=_mm_sub_ps(_mm_sub_ps(highInput,_mm_mul_ps(damping,band)),low);highInput=filterOutput4(kind,low,high,band);
      }
      alignas(16)float result[4];_mm_store_ps(result,highInput);for(int i=0;i<N;++i)output[i]=result[i];return;
    }
    double alpha=note.resonanceStrength>0?highcut::radialAlpha(note.highG,note.resonanceStrength):0;
    for(int i=0;i<stages;++i){
      auto stageDamping=(i==0&&stages==2)?_mm_set1_ps(std::max(.005f,note.damping)):damping;
      auto denominator=_mm_add_ps(_mm_set1_ps(1.f),_mm_mul_ps(g,_mm_add_ps(g,stageDamping)));
      auto oldBand=gatherFilter<N>(v,i*2),oldLow=gatherFilter<N>(v,i*2+1);
      auto band=_mm_div_ps(_mm_add_ps(oldBand,_mm_mul_ps(g,_mm_sub_ps(highInput,oldLow))),denominator);
      auto low=_mm_add_ps(oldLow,_mm_mul_ps(g,band));
      auto nextBand=_mm_sub_ps(_mm_mul_ps(two,band),oldBand),nextLow=_mm_sub_ps(_mm_mul_ps(two,low),oldLow);
      if(alpha>0){
        auto pair=[&](__m128 a,__m128 b){auto aa=_mm_cvtps_pd(a),bb=_mm_cvtps_pd(b);
          auto energy=_mm_add_pd(_mm_mul_pd(aa,aa),_mm_mul_pd(bb,bb));
          return _mm_cvtpd_ps(_mm_div_pd(_mm_set1_pd(1),_mm_sqrt_pd(_mm_add_pd(_mm_set1_pd(1),_mm_mul_pd(_mm_set1_pd(alpha),energy)))));};
        auto lo=pair(nextBand,nextLow),hi=pair(_mm_movehl_ps(nextBand,nextBand),_mm_movehl_ps(nextLow,nextLow));
        auto contraction=_mm_movelh_ps(lo,hi);nextBand=_mm_mul_ps(nextBand,contraction);nextLow=_mm_mul_ps(nextLow,contraction);
      }
      storeFilter<N>(v,i*2,nextBand);storeFilter<N>(v,i*2+1,nextLow);
      auto high=_mm_sub_ps(_mm_sub_ps(highInput,_mm_mul_ps(stageDamping,band)),low);
      highInput=filterOutput4(kind,low,high,band);
    }
    auto result=highInput;
    alignas(16) float lanes[4];_mm_store_ps(lanes,result);for(int i=0;i<N;++i)output[i]=lanes[i];
  }


#include "DualVcf.h"
#include "MemberMath.h"
  template<int N> __m128 XS_MEMBER_VECTORCALL analyticBatch(Voice**voices,int oscillator,int path,__m128 hz,__m128 pm,float shape,const Voice&note,bool*wrapped){
    auto delta=_mm_mul_ps(hz,_mm_set1_ps(invSr));
    alignas(16)float old[4]{},phase[4]{},position[4]{},increments[4]{},rawValues[4]{};
    for(int i=0;i<N;++i)old[i]=voices[i]->phase[oscillator];
    _mm_store_ps(phase,_mm_add_ps(_mm_load_ps(old),delta));
    phaseWrap::advance<N>(phase,wrapped);for(int i=0;i<N;++i)voices[i]->phase[oscillator]=phase[i];
    _mm_store_ps(position,_mm_add_ps(_mm_load_ps(phase),pm));
    phaseWrap::position<N>(position);
    int wave=note.waves[oscillator].target,renderer=note.engines[oscillator].target;
    auto phaseVector=_mm_load_ps(position),one=_mm_set1_ps(1.f);
    if(renderer==2){for(int i=0;i<N;++i)position[i]=std::floor(oscillator::wrap(position[i])*32)/32;phaseVector=_mm_load_ps(position);}
    __m128 raw;
    switch(wave){
      case 0:raw=memberSine<N>(phaseVector);break;
      case 1:{auto p4=_mm_mul_ps(_mm_set1_ps(4.f),phaseVector);auto first=_mm_cmplt_ps(phaseVector,_mm_set1_ps(.25f)),second=_mm_cmplt_ps(phaseVector,_mm_set1_ps(.75f));raw=table::MemberTableBatch::select(first,p4,table::MemberTableBatch::select(second,_mm_sub_ps(_mm_set1_ps(2.f),p4),_mm_sub_ps(p4,_mm_set1_ps(4.f))));break;}
      case 2:raw=_mm_sub_ps(_mm_mul_ps(_mm_set1_ps(2.f),phaseVector),one);break;
      case 3:raw=_mm_sub_ps(one,_mm_mul_ps(_mm_set1_ps(2.f),phaseVector));break;
      default:{float duty=wave==4?.5f:oscillator::pulseDuty(shape);if(renderer==2&&wave==5)duty=std::clamp(duty,1.f/32,31.f/32);raw=table::MemberTableBatch::select(_mm_cmplt_ps(phaseVector,_mm_set1_ps(duty)),one,_mm_set1_ps(-1.f));}
    }
    if(renderer==1)raw=memberClean<N>(phaseVector,delta,wave,shape,raw);
    if(wave==5){
      float duty=xs::oscillator::pulseDuty(shape);bool changing=false;
      for(int i=0;i<N;++i)changing|=voices[i]->previousPulseDuty[path][oscillator]!=duty;
      if(changing){
        if(renderer==1){
          alignas(16)float values[4]{},inc[4]{};_mm_store_ps(values,raw);_mm_store_ps(inc,delta);
          for(int i=0;i<N;++i)values[i]=xs::oscillator::movingPulse(values[i],position[i],inc[i],duty,voices[i]->previousPulseDuty[path][oscillator]);
          raw=_mm_load_ps(values);
        }
        // CLASSIC/RASTER also remember duty, matching scalar handoff into CLEAN.
        for(int i=0;i<N;++i)voices[i]->previousPulseDuty[path][oscillator]=duty;
      }
    }else for(int i=0;i<N;++i){float&previous=voices[i]->previousPulseDuty[path][oscillator];if(previous>=0)previous=-1;}
    return raw;
  }

  static __m128 discreteMixValue(__m128 a,__m128 b,float balance,int kind){
    auto one=_mm_set1_ps(1.f),zero=_mm_setzero_ps();
    switch(kind){
      case 0:return _mm_add_ps(_mm_mul_ps(_mm_set1_ps(1-balance),a),_mm_mul_ps(_mm_set1_ps(balance),b));
      case 1:return _mm_mul_ps(_mm_set1_ps(.5f),_mm_add_ps(_mm_mul_ps(_mm_set1_ps(std::min(1.f,2*(1-balance))),a),_mm_mul_ps(_mm_set1_ps(std::min(1.f,2*balance)),b)));
      case 2:return _mm_add_ps(_mm_mul_ps(_mm_set1_ps(1-balance),a),_mm_mul_ps(_mm_mul_ps(_mm_set1_ps(balance),a),b));
      case 3:return _mm_add_ps(_mm_mul_ps(_mm_set1_ps(1-balance),a),_mm_mul_ps(_mm_mul_ps(_mm_mul_ps(_mm_set1_ps(balance),a),_mm_add_ps(b,one)),_mm_set1_ps(.5f)));
      case 4:{auto sign=_mm_xor_ps(_mm_cmpge_ps(a,zero),_mm_cmpge_ps(b,zero));auto wave=table::MemberTableBatch::select(sign,one,_mm_set1_ps(-1.f));return _mm_add_ps(_mm_mul_ps(_mm_set1_ps(1-balance),a),_mm_mul_ps(_mm_set1_ps(balance),wave));}
      default:return _mm_sub_ps(_mm_mul_ps(_mm_set1_ps(1-balance),a),_mm_mul_ps(_mm_set1_ps(balance),b));
    }
  }

  bool fullBatchReady(int first,const Voice&note)const{
    if(!note.active||note.topology.target<0||note.types.target<0||note.slope.target<0||note.mix.target<0)return false;
    // Preserve the scalar leader's fault recovery if its histories are outside
    // a conservative, finite operating region. This never truncates a tail or
    // changes a sample: it selects the original scalar path in those cases.
    auto magnitude=[](__m128 x){return _mm_and_ps(x,_mm_castsi128_ps(_mm_set1_epi32(0x7fffffff)));};
    if(_mm_movemask_ps(_mm_cmplt_ps(magnitude(_mm_loadu_ps(note.highFilter)),_mm_set1_ps(1.e10f)))!=15)return false;
    auto coefficients=_mm_set_ps(0,note.highG,note.damping,0);
    if(_mm_movemask_ps(_mm_and_ps(_mm_cmpge_ps(coefficients,_mm_set_ps(0,0,-.04f,0)),_mm_cmple_ps(coefficients,_mm_set_ps(1,128,2,1))))!=15)return false;
    auto steps=_mm_set_ps(0,note.highStep,note.dampingStep,0);
    if(_mm_movemask_ps(_mm_cmplt_ps(magnitude(steps),_mm_set1_ps(1024.f)))!=15)return false;
    for(int o=0;o<2;++o)if(sourceKind[o]&&!sourceSelection[o].data)return false;
    for(int i=0;i<groupSize;++i){const auto&m=pool[first+i];if(!m.active||m.group!=note.group)return false;
      for(int o=0;o<2;++o){const auto&r=m.tables[o];if(sourceKind[o]){if(!r.ready||r.active||m.tableGeneration[o]!=sourceSelectionGeneration)return false;}else if(r.ready)return false;}
    }return true;
  }
  void advanceSharedNote(Voice&v){
    if(sharedDirectionControl&&v.group>=0)groups[v.group].topologyDominant=topologyDominant(v);
    v.hz=v.glide?v.hz+v.glideRate*(v.targetHz-v.hz):v.targetHz;
    if(sampleControl||v.modSmoothing||v.control==0)control(v,lfoValue);
    for(int o=0;o<2;++o){v.waves[o].advance();v.engines[o].advance();}
    v.cross.advance();v.topology.advance();v.mix.advance();v.types.advance();v.slope.advance();advanceSecondary(v);
    for(int e=0;e<2;++e){v.pitchRatio[e]+=v.pitchStep[e];for(int j=0;j<3;++j)v.envelopeRate[e][j]+=v.envelopeStep[e][j];}
    v.control=(v.control+1)&7;
    groups[v.group].envelopeValue[0]=envelope(v,0);groups[v.group].envelopeValue[1]=envelope(v,1);
    v.damping=std::clamp(v.damping+v.dampingStep,-.04f,2.f);
      v.resonanceStrength=std::clamp(v.resonanceStrength+v.resonanceStep,0.f,1.f);
    v.highG=std::max(0.f,v.highG+v.highStep);
  }
  template<int N,bool Ready=false> bool renderDirectionBatch(int first,Voice&note,float*output){
    // Shared direction changes retain per-member histories; other mode blends stay scalar.
    // A ready, inactive table head carrying the current generation has fully
    // accepted that generation's immutable selection. No field-by-field
    // selection comparison or follower mode comparison is needed per sample.
    if constexpr(!Ready){if(!note.active)return false;for(int o=0;o<2;++o)if(sourceKind[o]&&!sourceSelection[o].data)return false;}
    int path=note.topology.target;if constexpr(!Ready){if(path<0||note.types.target<0||note.slope.target<0||note.mix.target<0)return false;}
    Voice*v[N];table::Oscillator::Head*head[2][N];PreparedVoice prepared[N];
    for(int i=0;i<N;++i){v[i]=&pool[first+i];auto&m=*v[i];if constexpr(!Ready){if(!m.active||m.group!=note.group)return false;}
      for(int o=0;o<2;++o){auto&r=m.tables[o];auto&leader=note.tables[o];
        if(sourceKind[o]){if constexpr(!Ready){if(!r.ready||r.active||m.tableGeneration[o]!=sourceSelectionGeneration)return false;}head[o][i]=&r.current;}
        else if constexpr(!Ready){if(r.ready)return false;}
      }
    }
    for(int i=0;i<N;++i)if(v[i]!=&note){adoptSharedTopology(*v[i],note);v[i]->resonanceStrength=note.resonanceStrength;v[i]->resonanceStep=note.resonanceStep;}
    float env1=groups[note.group].envelopeValue[0],env2=groups[note.group].envelopeValue[1];
    float depth=std::clamp(float(state[XDepth])+note.mod[6],0.f,1.f),fm=8*depth*note.cross.weights[1],pm=4*depth*note.cross.weights[2];
    float lfoDepth=std::max(0.f,1+note.mod[11]);float directPitch=lfoValue*lfoDepth*note.lfoPitch;for(int l=1;l<4;++l)directPitch+=lfoValues[l]*float(state[lfoParam(l,1)]);
    float ratio=std::max(0.f,1+directPitch),shape[2]={std::clamp(p[1]+note.mod[1],0.f,1.f),std::clamp(p[3]+note.mod[4],0.f,1.f)};
    float balance=std::clamp(p[7]+note.mod[7],0.f,1.f),spread=50*std::clamp(float(state[Detune])+note.mod[41],0.f,1.f);
    alignas(16)float detune[4]{};
    for(int i=0;i<N;++i){auto&m=*v[i];if(m.renderedSamples!=UINT32_MAX)++m.renderedSamples;detune[i]=1;
      if(m.members>1||m.detunePosition!=0){m.spread+=(spread-m.spread)/transitionSamples;if(m.spread!=m.cachedSpread||m.detunePosition!=m.cachedDetunePosition){m.cachedSpread=m.spread;m.cachedDetunePosition=m.detunePosition;m.cachedDetuneRatio=std::exp2(m.detunePosition*m.spread/2400.f);}detune[i]=m.cachedDetuneRatio;}
    }
    auto lanes=_mm_load_ps(detune);
    auto hz1=_mm_mul_ps(_mm_mul_ps(_mm_set1_ps(note.hz*note.pitchRatio[0]),lanes),_mm_set1_ps(ratio));
    auto hz2=_mm_mul_ps(_mm_mul_ps(_mm_mul_ps(_mm_mul_ps(_mm_set1_ps(note.hz*note.pitchRatio[1]),lanes),_mm_set1_ps(std::max(0.f,1+env1*note.egPitch[0]))),_mm_set1_ps(std::max(0.f,1+env2*note.egPitch[1]))),_mm_set1_ps(ratio));
    float position[2]={std::clamp(float(state[oscParam(0,3)])+note.mod[42],0.f,1.f),std::clamp(float(state[oscParam(1,3)])+note.mod[43],0.f,1.f)};
    auto one=_mm_set1_ps(1.f),zero=_mm_setzero_ps();
    __m128 totalA=zero,totalB=zero;
    auto renderPath=[&](int path,float weight){
      for(int i=0;i<N;++i){std::copy_n(v[i]->topologyPhase[path].data(),2,v[i]->phase);v[i]->masterWrapped=v[i]->topologyWrapped[path];}
    bool wa[N]{},wb[N]{};__m128 a,b;bool sync=path%2!=0,reversed=path/2!=0,skipB=sourceKind[1]!=0&&balance==0&&fm==0&&pm==0&&!sync;
    auto read=[&](int o,__m128 frequency,__m128 phaseMod,bool*wrapped,bool silent=false){
      return sourceKind[o]?table::MemberTableBatch::read<N>(head[o],path,frequency,phaseMod,position[o],sr,wrapped,&note.tables[o].current,silent):analyticBatch<N>(v,o,path,frequency,phaseMod,shape[o],note,wrapped);
    };
    if(!reversed){a=read(0,hz1,zero,wa);
      if(sync)for(int i=0;i<N;++i)if(wa[i]){v[i]->phase[1]=0;v[i]->tables[1].sync(path);}
      b=read(1,_mm_mul_ps(hz2,_mm_add_ps(one,_mm_mul_ps(_mm_set1_ps(fm),a))),_mm_mul_ps(_mm_set1_ps(pm),a),wb,skipB);
    }else{if(sync)for(int i=0;i<N;++i)if(v[i]->topologyWrapped[path]){v[i]->phase[1]=0;v[i]->tables[1].sync(path);}
      b=read(1,hz2,zero,wb,skipB);
      a=read(0,_mm_mul_ps(hz1,_mm_add_ps(one,_mm_mul_ps(_mm_set1_ps(fm),b))),_mm_mul_ps(_mm_set1_ps(pm),b),wa);
    }

      totalA=_mm_add_ps(totalA,_mm_mul_ps(_mm_set1_ps(weight),a));
      totalB=_mm_add_ps(totalB,_mm_mul_ps(_mm_set1_ps(weight),b));
      for(int i=0;i<N;++i){auto&m=*v[i];for(int o=0;o<2;++o){if(sourceKind[o])m.phase[o]=float(head[o][i]->phase[path]);m.topologyPhase[path][o]=m.phase[o];}m.topologyWrapped[path]=wa[i];}
    };
    if(note.topology.remaining){for(int t=0;t<4;++t)if(note.topology.weights[t]!=0)renderPath(t,note.topology.weights[t]);}
    else renderPath(note.topology.target,1);
    __m128 a=totalA,b=totalB;
    for(int i=0;i<N;++i){std::copy_n(v[i]->topologyPhase[note.topology.target].data(),2,v[i]->phase);v[i]->masterWrapped=v[i]->topologyWrapped[note.topology.target];}
    a=_mm_mul_ps(a,_mm_set1_ps(std::clamp(1+note.mod[2],0.f,2.f)));b=_mm_mul_ps(b,_mm_set1_ps(std::clamp(1+note.mod[5],0.f,2.f)));
    __m128 input;
    if(note.mix.remaining){input=zero;for(int kind=0;kind<6;++kind)
      input=_mm_add_ps(input,_mm_mul_ps(_mm_set1_ps(note.mix.weights[kind]),discreteMixValue(a,b,balance,kind)));
    }else{
    switch(note.mix.target){
      case 0:input=_mm_add_ps(_mm_mul_ps(_mm_set1_ps(1-balance),a),_mm_mul_ps(_mm_set1_ps(balance),b));break;
      case 1:input=_mm_mul_ps(_mm_set1_ps(.5f),_mm_add_ps(_mm_mul_ps(_mm_set1_ps(std::min(1.f,2*(1-balance))),a),_mm_mul_ps(_mm_set1_ps(std::min(1.f,2*balance)),b)));break;
      case 2:input=_mm_add_ps(_mm_mul_ps(_mm_set1_ps(1-balance),a),_mm_mul_ps(_mm_mul_ps(_mm_set1_ps(balance),a),b));break;
      case 3:input=_mm_add_ps(_mm_mul_ps(_mm_set1_ps(1-balance),a),_mm_mul_ps(_mm_mul_ps(_mm_mul_ps(_mm_set1_ps(balance),a),_mm_add_ps(b,one)),_mm_set1_ps(.5f)));break;
      case 4:{auto sign=_mm_xor_ps(_mm_cmpge_ps(a,zero),_mm_cmpge_ps(b,zero));auto wave=table::MemberTableBatch::select(sign,one,_mm_set1_ps(-1.f));input=_mm_add_ps(_mm_mul_ps(_mm_set1_ps(1-balance),a),_mm_mul_ps(_mm_set1_ps(balance),wave));break;}
      default:input=_mm_sub_ps(_mm_mul_ps(_mm_set1_ps(1-balance),a),_mm_mul_ps(_mm_set1_ps(balance),b));
    }
    }
    alignas(16)float mixed[4];_mm_store_ps(mixed,input);
    for(int i=0;i<N;++i){v[i]->firstSample=false;prepared[i]={mixed[i],env1};}
    float filtered[N];networkBatch<N>(v,prepared,note,filtered);
    for(int i=0;i<N;++i)output[i]=finishVoice(*v[i],filtered[i]*env1,note);
    return true;
  }
  template<int N,bool Ready=false> bool renderHoldDirectionBatch(int first,Voice&note,float*output){
    // Shared direction changes retain per-member histories; other mode blends stay scalar.
    // A ready, inactive table head carrying the current generation has fully
    // accepted that generation's immutable selection. No field-by-field
    // selection comparison or follower mode comparison is needed per sample.
    if constexpr(!Ready){if(!note.active)return false;for(int o=0;o<2;++o)if(sourceKind[o]&&!sourceSelection[o].data)return false;}
    int path=note.topology.target;if constexpr(!Ready){if(path<0||note.types.target<0||note.slope.target<0||note.mix.target<0)return false;}
    Voice*v[N];table::Oscillator::Head*head[2][N];PreparedVoice prepared[N];
    for(int i=0;i<N;++i){v[i]=&pool[first+i];auto&m=*v[i];if constexpr(!Ready){if(!m.active||m.group!=note.group)return false;}
      for(int o=0;o<2;++o){auto&r=m.tables[o];if constexpr(!Ready){if(!r.ready||r.active||m.tableGeneration[o]!=sourceSelectionGeneration)return false;}head[o][i]=&r.current;}
    }
    for(int i=0;i<N;++i)if(v[i]!=&note){adoptSharedTopology(*v[i],note);v[i]->resonanceStrength=note.resonanceStrength;v[i]->resonanceStep=note.resonanceStep;}
    float env1=groups[note.group].envelopeValue[0],env2=groups[note.group].envelopeValue[1];
    float depth=std::clamp(float(state[XDepth])+note.mod[6],0.f,1.f),fm=8*depth*note.cross.weights[1],pm=4*depth*note.cross.weights[2];
    float lfoDepth=std::max(0.f,1+note.mod[11]);float directPitch=lfoValue*lfoDepth*note.lfoPitch;for(int l=1;l<4;++l)directPitch+=lfoValues[l]*float(state[lfoParam(l,1)]);
    float ratio=std::max(0.f,1+directPitch),shape[2]={std::clamp(p[1]+note.mod[1],0.f,1.f),std::clamp(p[3]+note.mod[4],0.f,1.f)};
    float balance=std::clamp(p[7]+note.mod[7],0.f,1.f),spread=50*std::clamp(float(state[Detune])+note.mod[41],0.f,1.f);
    alignas(16)float detune[4]{};
    for(int i=0;i<N;++i){auto&m=*v[i];if(m.renderedSamples!=UINT32_MAX)++m.renderedSamples;detune[i]=1;
      if(m.members>1||m.detunePosition!=0){m.spread+=(spread-m.spread)/transitionSamples;if(m.spread!=m.cachedSpread||m.detunePosition!=m.cachedDetunePosition){m.cachedSpread=m.spread;m.cachedDetunePosition=m.detunePosition;m.cachedDetuneRatio=std::exp2(m.detunePosition*m.spread/2400.f);}detune[i]=m.cachedDetuneRatio;}
    }
    auto lanes=_mm_load_ps(detune);
    auto hz1=_mm_mul_ps(_mm_mul_ps(_mm_set1_ps(note.hz*note.pitchRatio[0]),lanes),_mm_set1_ps(ratio));
    auto hz2=_mm_mul_ps(_mm_mul_ps(_mm_mul_ps(_mm_mul_ps(_mm_set1_ps(note.hz*note.pitchRatio[1]),lanes),_mm_set1_ps(std::max(0.f,1+env1*note.egPitch[0]))),_mm_set1_ps(std::max(0.f,1+env2*note.egPitch[1]))),_mm_set1_ps(ratio));
    float position[2]={std::clamp(float(state[oscParam(0,3)])+note.mod[42],0.f,1.f),std::clamp(float(state[oscParam(1,3)])+note.mod[43],0.f,1.f)};
    table::HoldBatch::Plan<N> plans[2]={table::HoldBatch::prepare<N>(head[0],hz1,position[0],sr),table::HoldBatch::prepare<N>(head[1],hz2,position[1],sr)};
    auto one=_mm_set1_ps(1.f),zero=_mm_setzero_ps();
    __m128 totalA=zero,totalB=zero;
    const bool blending=note.topology.remaining!=0;
    for(int iteration=0;iteration<(blending?4:1);++iteration){
      const int path=blending?iteration:note.topology.target;
      const float weight=blending?note.topology.weights[path]:1.f;
      if(blending&&weight==0)continue;
    bool wa[N]{},wb[N]{};__m128 a,b;bool sync=path%2!=0,reversed=path/2!=0,skipB=sourceKind[1]!=0&&balance==0&&fm==0&&pm==0&&!sync;

    if(!reversed){a=table::HoldBatch::read<N>(plans[0],path,wa);
      if(sync)for(int i=0;i<N;++i)if(wa[i]){v[i]->tables[1].sync(path);}
      b=table::HoldBatch::read<N>(plans[1],path,wb,skipB);
    }else{if(sync)for(int i=0;i<N;++i)if(v[i]->topologyWrapped[path]){v[i]->tables[1].sync(path);}
      b=table::HoldBatch::read<N>(plans[1],path,wb,skipB);
      a=table::HoldBatch::read<N>(plans[0],path,wa);
    }

      totalA=_mm_add_ps(totalA,_mm_mul_ps(_mm_set1_ps(weight),a));
      totalB=_mm_add_ps(totalB,_mm_mul_ps(_mm_set1_ps(weight),b));
      for(int i=0;i<N;++i){auto&m=*v[i];for(int o=0;o<2;++o)m.topologyPhase[path][o]=float(head[o][i]->phase[path]);m.topologyWrapped[path]=wa[i];}
    }
    __m128 a=totalA,b=totalB;
    for(int i=0;i<N;++i){std::copy_n(v[i]->topologyPhase[note.topology.target].data(),2,v[i]->phase);v[i]->masterWrapped=v[i]->topologyWrapped[note.topology.target];}
    a=_mm_mul_ps(a,_mm_set1_ps(std::clamp(1+note.mod[2],0.f,2.f)));b=_mm_mul_ps(b,_mm_set1_ps(std::clamp(1+note.mod[5],0.f,2.f)));
    __m128 input;
    if(note.mix.remaining){input=zero;for(int kind=0;kind<6;++kind)
      input=_mm_add_ps(input,_mm_mul_ps(_mm_set1_ps(note.mix.weights[kind]),discreteMixValue(a,b,balance,kind)));
    }else{
    switch(note.mix.target){
      case 0:input=_mm_add_ps(_mm_mul_ps(_mm_set1_ps(1-balance),a),_mm_mul_ps(_mm_set1_ps(balance),b));break;
      case 1:input=_mm_mul_ps(_mm_set1_ps(.5f),_mm_add_ps(_mm_mul_ps(_mm_set1_ps(std::min(1.f,2*(1-balance))),a),_mm_mul_ps(_mm_set1_ps(std::min(1.f,2*balance)),b)));break;
      case 2:input=_mm_add_ps(_mm_mul_ps(_mm_set1_ps(1-balance),a),_mm_mul_ps(_mm_mul_ps(_mm_set1_ps(balance),a),b));break;
      case 3:input=_mm_add_ps(_mm_mul_ps(_mm_set1_ps(1-balance),a),_mm_mul_ps(_mm_mul_ps(_mm_mul_ps(_mm_set1_ps(balance),a),_mm_add_ps(b,one)),_mm_set1_ps(.5f)));break;
      case 4:{auto sign=_mm_xor_ps(_mm_cmpge_ps(a,zero),_mm_cmpge_ps(b,zero));auto wave=table::MemberTableBatch::select(sign,one,_mm_set1_ps(-1.f));input=_mm_add_ps(_mm_mul_ps(_mm_set1_ps(1-balance),a),_mm_mul_ps(_mm_set1_ps(balance),wave));break;}
      default:input=_mm_sub_ps(_mm_mul_ps(_mm_set1_ps(1-balance),a),_mm_mul_ps(_mm_set1_ps(balance),b));
    }
    }
    alignas(16)float mixed[4];_mm_store_ps(mixed,input);
    for(int i=0;i<N;++i){v[i]->firstSample=false;prepared[i]={mixed[i],env1};}
    float filtered[N];networkBatch<N>(v,prepared,note,filtered);
    for(int i=0;i<N;++i)output[i]=finishVoice(*v[i],filtered[i]*env1,note);
    return true;
  }
  template<int N,bool Ready=false> bool renderStableBatch(int first,Voice&note,float*output){
    // noteShared excludes discrete modulation and unfinished mode blends.
    // A ready, inactive table head carrying the current generation has fully
    // accepted that generation's immutable selection. No field-by-field
    // selection comparison or follower mode comparison is needed per sample.
    if constexpr(!Ready){if(!note.active)return false;for(int o=0;o<2;++o)if(sourceKind[o]&&!sourceSelection[o].data)return false;}
    int path=note.topology.target;if constexpr(!Ready){if(path<0||note.types.target<0||note.slope.target<0||note.mix.target<0)return false;}
    Voice*v[N];table::Oscillator::Head*head[2][N];PreparedVoice prepared[N];
    for(int i=0;i<N;++i){v[i]=&pool[first+i];auto&m=*v[i];if constexpr(!Ready){if(!m.active||m.group!=note.group)return false;}
      for(int o=0;o<2;++o){auto&r=m.tables[o];auto&leader=note.tables[o];
        if(sourceKind[o]){if constexpr(!Ready){if(!r.ready||r.active||m.tableGeneration[o]!=sourceSelectionGeneration)return false;}head[o][i]=&r.current;}
        else if constexpr(!Ready){if(r.ready)return false;}
      }
    }
    for(int i=0;i<N;++i){std::copy_n(v[i]->topologyPhase[path].data(),2,v[i]->phase);v[i]->masterWrapped=v[i]->topologyWrapped[path];}
    float env1=groups[note.group].envelopeValue[0],env2=groups[note.group].envelopeValue[1];
    float depth=std::clamp(float(state[XDepth])+note.mod[6],0.f,1.f),fm=8*depth*note.cross.weights[1],pm=4*depth*note.cross.weights[2];
    float lfoDepth=std::max(0.f,1+note.mod[11]);float directPitch=lfoValue*lfoDepth*note.lfoPitch;for(int l=1;l<4;++l)directPitch+=lfoValues[l]*float(state[lfoParam(l,1)]);
    float ratio=std::max(0.f,1+directPitch),shape[2]={std::clamp(p[1]+note.mod[1],0.f,1.f),std::clamp(p[3]+note.mod[4],0.f,1.f)};
    float balance=std::clamp(p[7]+note.mod[7],0.f,1.f),spread=50*std::clamp(float(state[Detune])+note.mod[41],0.f,1.f);
    alignas(16)float detune[4]{};
    for(int i=0;i<N;++i){auto&m=*v[i];if(m.renderedSamples!=UINT32_MAX)++m.renderedSamples;detune[i]=1;
      if(m.members>1||m.detunePosition!=0){m.spread+=(spread-m.spread)/transitionSamples;if(m.spread!=m.cachedSpread||m.detunePosition!=m.cachedDetunePosition){m.cachedSpread=m.spread;m.cachedDetunePosition=m.detunePosition;m.cachedDetuneRatio=std::exp2(m.detunePosition*m.spread/2400.f);}detune[i]=m.cachedDetuneRatio;}
    }
    auto lanes=_mm_load_ps(detune);
    auto hz1=_mm_mul_ps(_mm_mul_ps(_mm_set1_ps(note.hz*note.pitchRatio[0]),lanes),_mm_set1_ps(ratio));
    auto hz2=_mm_mul_ps(_mm_mul_ps(_mm_mul_ps(_mm_mul_ps(_mm_set1_ps(note.hz*note.pitchRatio[1]),lanes),_mm_set1_ps(std::max(0.f,1+env1*note.egPitch[0]))),_mm_set1_ps(std::max(0.f,1+env2*note.egPitch[1]))),_mm_set1_ps(ratio));
    float position[2]={std::clamp(float(state[oscParam(0,3)])+note.mod[42],0.f,1.f),std::clamp(float(state[oscParam(1,3)])+note.mod[43],0.f,1.f)};
    bool wa[N]{},wb[N]{};__m128 a,b;bool sync=path%2!=0,reversed=path/2!=0,skipB=sourceKind[1]!=0&&balance==0&&fm==0&&pm==0&&!sync;
    auto one=_mm_set1_ps(1.f),zero=_mm_setzero_ps();
    auto read=[&](int o,__m128 frequency,__m128 phaseMod,bool*wrapped,bool silent=false){
      return sourceKind[o]?table::MemberTableBatch::read<N>(head[o],path,frequency,phaseMod,position[o],sr,wrapped,&note.tables[o].current,silent):analyticBatch<N>(v,o,path,frequency,phaseMod,shape[o],note,wrapped);
    };
    if(!reversed){a=read(0,hz1,zero,wa);
      if(sync)for(int i=0;i<N;++i)if(wa[i]){v[i]->phase[1]=0;v[i]->tables[1].sync(path);}
      b=read(1,_mm_mul_ps(hz2,_mm_add_ps(one,_mm_mul_ps(_mm_set1_ps(fm),a))),_mm_mul_ps(_mm_set1_ps(pm),a),wb,skipB);
    }else{if(sync)for(int i=0;i<N;++i)if(v[i]->topologyWrapped[path]){v[i]->phase[1]=0;v[i]->tables[1].sync(path);}
      b=read(1,hz2,zero,wb,skipB);
      a=read(0,_mm_mul_ps(hz1,_mm_add_ps(one,_mm_mul_ps(_mm_set1_ps(fm),b))),_mm_mul_ps(_mm_set1_ps(pm),b),wa);
    }
    // Original path accumulation starts at +0 even for one settled path.
    a=_mm_add_ps(zero,a);b=_mm_add_ps(zero,b);
    a=_mm_mul_ps(a,_mm_set1_ps(std::clamp(1+note.mod[2],0.f,2.f)));b=_mm_mul_ps(b,_mm_set1_ps(std::clamp(1+note.mod[5],0.f,2.f)));
    __m128 input;
    if(note.mix.remaining){input=zero;for(int kind=0;kind<6;++kind)
      input=_mm_add_ps(input,_mm_mul_ps(_mm_set1_ps(note.mix.weights[kind]),discreteMixValue(a,b,balance,kind)));
    }else{
    switch(note.mix.target){
      case 0:input=_mm_add_ps(_mm_mul_ps(_mm_set1_ps(1-balance),a),_mm_mul_ps(_mm_set1_ps(balance),b));break;
      case 1:input=_mm_mul_ps(_mm_set1_ps(.5f),_mm_add_ps(_mm_mul_ps(_mm_set1_ps(std::min(1.f,2*(1-balance))),a),_mm_mul_ps(_mm_set1_ps(std::min(1.f,2*balance)),b)));break;
      case 2:input=_mm_add_ps(_mm_mul_ps(_mm_set1_ps(1-balance),a),_mm_mul_ps(_mm_mul_ps(_mm_set1_ps(balance),a),b));break;
      case 3:input=_mm_add_ps(_mm_mul_ps(_mm_set1_ps(1-balance),a),_mm_mul_ps(_mm_mul_ps(_mm_mul_ps(_mm_set1_ps(balance),a),_mm_add_ps(b,one)),_mm_set1_ps(.5f)));break;
      case 4:{auto sign=_mm_xor_ps(_mm_cmpge_ps(a,zero),_mm_cmpge_ps(b,zero));auto wave=table::MemberTableBatch::select(sign,one,_mm_set1_ps(-1.f));input=_mm_add_ps(_mm_mul_ps(_mm_set1_ps(1-balance),a),_mm_mul_ps(_mm_set1_ps(balance),wave));break;}
      default:input=_mm_sub_ps(_mm_mul_ps(_mm_set1_ps(1-balance),a),_mm_mul_ps(_mm_set1_ps(balance),b));
    }
    }
    alignas(16)float mixed[4];_mm_store_ps(mixed,input);
    for(int i=0;i<N;++i){auto&m=*v[i];for(int o=0;o<2;++o){if(sourceKind[o])m.phase[o]=float(head[o][i]->phase[path]);m.topologyPhase[path][o]=m.phase[o];}m.masterWrapped=m.topologyWrapped[path]=wa[i];m.firstSample=false;prepared[i]={mixed[i],env1};}
    float filtered[N];networkBatch<N>(v,prepared,note,filtered);
    for(int i=0;i<N;++i)output[i]=finishVoice(*v[i],filtered[i]*env1,note);
    return true;
  }
  // Keep the previously accepted stable-mode kernel intact. The new
  // transition kernel is selected once per member batch, never per member.
  template<int N,bool Ready=false> bool renderBatch(int first,Voice&note,float*output){
    XS_DISCRETE_COUNT(3,N);
    if(!sharedDirectionControl)return renderStableBatch<N,Ready>(first,note,output);
    bool hold=sourceKind[0]&&sourceKind[1]&&note.tables[0].current.selection.sample==table::Sample::Hold&&note.tables[1].current.selection.sample==table::Sample::Hold;
    bool neutral=std::clamp(float(state[XDepth])+note.mod[6],0.f,1.f)==0||(note.cross.weights[1]==0&&note.cross.weights[2]==0);
    return hold&&neutral?renderHoldDirectionBatch<N,Ready>(first,note,output):renderDirectionBatch<N,Ready>(first,note,output);
  }
#include "RetirementLanes.h"
#include "TopologyLanes.h"



public:
  Engine(){resetDistortion();}
  void setAssets(const table::Bank* bank,const table::Document* users=nullptr,int owner=-1){unshareNotes();noteSharingNeedsSettle=true;rom=bank;document=users;documentOwner=owner;changed=true;}
  unsigned assetOwners()const{unsigned mask=documentOwner<0?0:1u<<documentOwner;for(int i=0;i<renderEnd();++i)if(const auto&v=pool[i];v.active)for(const auto&t:v.tables)mask|=t.owners();return mask|retireOwners()|topologyOwners();}
  void reset(double rate) {
    LegacyEngine::reset(rate); sr=float(rate); invSr=1/sr;
    resetDistortion();
    filterRoute=0;dualEverUsed=false;dualControlFrames=0;eg2Tempo=120;
    resetWidth();topologyFromLeft=topologyFromRight=0;topologyTailRemaining=topologyTailTotal=0;topologyPending=false;topologyEpoch=0;topologyIncomingWeight=1;topologyExtraRenders=0;topologyRetained.fill(-1);topologyRetainedOld.fill(-1);
    for(auto&r:retirePool)r.count=0;batchSaturated=false;steals=retireOverflows=0;
    sustainDown=sostenutoDown=false;state[Sustain]=state[Sostenuto]=0;
    extraLfos={};firstLfoSeed=0x4c464f31;lfoValues={};for(int l=0;l<3;++l)extraLfos[l].seed+=l*0x9e3779b9u;
    pool={};poolEnd=0;retirementLaneFrames=0;groups={};intents={};groupLive=state[Unison]>0;groupSize=groupLive?0:1;groupLimit=8;topologyRemaining=0;topologyFrom=lastPreOutput=0;held={}; velocities={}; key=0; serial=0; seed=0x7853796e;numericalFaults=0;
    lfoPos=0;lfoPhaseAccumulator=previousLfoPhase=0;expandedLive=false;forceExpanded=false;previousSync=state[27]>=.5;previousSlope=state[28]>=.5;hostAnchor=true;effectivePhase=effectivePolarity=-1;polarityMix={};phaseMix={};transitionSamples=std::max(1,int(sr*.002)); lastHz=0; randomA=0; randomB=0;lfoTick=0;lfoIncrement=lfoIncrementStep=0; changed=true;coefficients=(1u<<24)-1;tuningDirty=true;routesDirty=true;noteSharingNeedsSettle=true;classify();
  }
  void set(int id,double value) {
    if(id<0 || id>=Count || id==AllNotesOff || id==AllSoundsOff || !valid(value))return;
    value=stepped(id,value);
    if(id==DriveExtension){if(state[id]!=value){state[id]=value;requestDistortion();classify();}return;}
    if(id==VoicesExtension){if(state[id]!=value){state[id]=value;changed=true;classify();}return;}
    if(id==Voices&&state[VoicesExtension]!=0){state[VoicesExtension]=0;changed=true;classify();}
    if(id>=PreDistortionCount&&id<PreVoicesCount){if(state[id]!=value){state[id]=value;requestDistortion();classify();}return;}
    if(id==Sustain||id==Sostenuto){unshareNotes();setPedal(id,value);return;}
    if(id==Width){state[id]=value;widthTargets();return;}
    if(id==oscParam(0,3)||id==oscParam(1,3)){state[id]=value;return;}
    auto releaseB=[&](int slot){if(state[BFilterRouteFirst+slot]!=0){state[BFilterRouteFirst+slot]=0;changed=true;routesDirty=true;}};
    if(id>=PositionRouteFirst&&id<RasterFirst)releaseB(id-PositionRouteFirst);
    if(matrixDestination(id)||expandedDestination(id)|| (id>=DetuneRouteFirst&&id<PreFourLfoCount)){int slot=matrixDestination(id)?matrixSlot(id):expandedDestination(id)?id-ExpandedDestinationFirst:id-DetuneRouteFirst;state[PositionRouteFirst+slot]=0;releaseB(slot);}
    if(legacyDestination(id)||expandedDestination(id)){
      int extension=DetuneRouteFirst+(legacyDestination(id)?(id-MatrixFirst)/3:id-ExpandedDestinationFirst);
      if(state[extension]!=0){state[extension]=0;changed=true;routesDirty=true;classify();}
    }
    if(legacyDestination(id)) {
      int extension=ExpandedDestinationFirst+(id-MatrixFirst)/3;
      if(state[extension]!=0){state[extension]=0;changed=true;routesDirty=true;classify();}
    }
    if(matrixSource(id)&&state[SourceExtensionFirst+matrixSlot(id)]!=0){state[SourceExtensionFirst+matrixSlot(id)]=0;routesDirty=true;changed=true;classify();}
    if(value==state[id])return;
    // Spread is read per sample; it does not invalidate unrelated control caches.
    if(id==Detune){LegacyEngine::set(id,value);return;}
    if((id==27||id==28)&&!forceExpanded&&!expandedLive&&continuity().active){
      previousSync=state[27]>=.5;previousSlope=state[28]>=.5;forceExpanded=true;
    }
    if(stepsFor(id)>0&&id!=Unison)noteSharingNeedsSettle=true;
    LegacyEngine::set(id,value); changed=true;
    if(id<24)coefficients|=1u<<id;
    if(id==30 || id==MasterTune || id==BendRange)tuningDirty=true;
    if((id>=MatrixFirst&&id<PreUnisonCount)||id>=DetuneRouteFirst)routesDirty=true;
    if(matrixSource(id)&&state[SourceExtensionFirst+matrixSlot(id)]!=0){state[SourceExtensionFirst+matrixSlot(id)]=0;routesDirty=true;}
    if(id>=36)classify();
  }
  void transport(double bpm,bool bpmValid,double songBeat,bool songValid,bool running=true) {
    bool changedTempo=tempo!=bpm || tempoValid!=(bpmValid && std::isfinite(bpm) && bpm>0);
    if(bpmValid&&std::isfinite(bpm)&&bpm>0)eg2Tempo=bpm;
    tempo=bpm; tempoValid=bpmValid && std::isfinite(bpm) && bpm>0;
    bool anchor=(running&&!playing)||(songValid&&std::isfinite(songBeat)&&(!beatValid||std::abs(songBeat-beat)>std::max(1.e-7,(bpmValid?bpm:120)/(60*sr)*2)));
    if(anchor){hostAnchor=true;for(auto &l:extraLfos)l.hostAnchor=true;}
    beat=songBeat; beatValid=songValid && std::isfinite(songBeat);
    playing=running;
    if(changedTempo)lfoTick=0;
    if(state[LfoClock]>=.5 && !tempoValid)lfoIncrement=lfoIncrementStep=0;
  }
  void note(int n,bool down,float velocity=1,int id=-1) {
    if(n<21 || n>96)return;
    unshareNotes();LegacyEngine::note(n,down);
    if(changed)prepare();
    if(!down&&topologyTailRemaining)for(auto&v:topologyVoices)if(v.active&&v.note==n&&(id<0||v.noteId==id)){v.keyDown=false;if(!sustainDown&&!v.pedalLatched)release(v);}
    if(groupLive){groupNote(n,down,velocity,id);return;}
    bool wasHeld=key!=0; held[n]=down; velocities[n]=std::clamp(velocity,0.f,1.f);
    if(voiceMode==2) {
      if(down) {
        Voice *chosen=nullptr;
        for(int i=0;i<voiceLimit;++i) if(!pool[i].active) { chosen=&pool[i];break; }
        bool handoff=false;
        if(!chosen) {chosen=&pool[polyVictim()];handoff=retireNote(int(chosen-pool.data()),1);}
        *chosen={}; trigger(*chosen,n,velocity,false,id);if(handoff)incomingHandoff(*chosen);
      } else for(auto &v:pool) if(v.active && v.note==n && (id<0 || v.noteId==id)){v.keyDown=false;if(!sustainDown&&!v.pedalLatched)release(v);}
    } else {
      int highestNote=highest();
      if(highestNote && (highestNote!=key || (down && n==highestNote)))
        trigger(pool[0],highestNote,velocities[highestNote],voiceMode==1 && wasHeld,id);
      else if(!highestNote)release(pool[0]);
    }
    key=highest();
  }
  void allOff() {
    unshareNotes();
    if(topologyTailRemaining)for(auto&v:topologyVoices)if(v.active){v.keyDown=v.pedalLatched=false;release(v);}
    LegacyEngine::allOff();intents={};for(auto&g:groups)g.gate=false;held={}; key=0;sustainDown=sostenutoDown=false;state[Sustain]=state[Sostenuto]=0;for(auto &v:pool)if(v.active){v.keyDown=v.pedalLatched=false;release(v);}
  }
  int activeVoices() const { int n=0;for(auto &v:pool)n+=v.active&&!v.retiring;return n; }
  float modulationPhase() const { return lfoPos; }
  bool usingLegacy() const { return !expandedLive&&!expanded&&!forceExpanded; }
  float outputLeft()const{return outputLeftValue;}
  float outputRight()const{return outputRightValue;}
  // Keep the original scalar loop free of SIMD eligibility/bit-mask work.
  // Its prelude and final ordered summing intentionally match next(). Changing
  // shared control or transition behavior requires updating both and parity tests.
#if defined(_MSC_VER)
  __declspec(noinline)
#endif
  float nextScalar() {
    batchSaturated=false;
    if(!expandedLive&&!expanded&&!forceExpanded){if(changed)prepare();lfoValue=lfoValues[0]=lfo();for(int l=1;l<4;++l)lfoValues[l]=lfo(l);if(beatValid&&tempoValid&&playing)beat+=tempo/(60*sr);LegacyEngine::next();preLimiterOutput=preOutputLeft=preOutputRight=lastOutputBeforeClamp;return finishOutput();}
    if(!expandedLive){
      auto c=continuity();expandedLive=true;
      if(c.active&&pool[0].active){if(pool[0].group>=0)groups[pool[0].group].noteCoherent=false;auto &v=pool[0];v.firstSample=false;std::copy_n(c.oscillator,2,v.phase);std::copy_n(c.highFilter,4,v.highFilter);v.highG=c.coefficient;v.damping=c.damping;v.resonanceStrength=c.strength;
        for(int e=0;e<2;++e){v.eg[e].value=v.eg[e].start=c.env[e];v.eg[e].stage=c.stage[e];v.eg[e].progress=0;}
        lfoPos=c.lfo;lfoPhaseAccumulator=c.lfo;
        v.cross.reset(0);v.mix.reset(0);v.types.reset(0);v.slope.reset(forceExpanded?previousSlope:state[28]>=.5);
        int topology=forceExpanded?previousSync:state[27]>=.5;v.topology.reset(topology);v.topologyPhase[topology]={v.phase[0],v.phase[1]};v.previousPulseDuty[topology]={c.pulseDuty[0],c.pulseDuty[1]};v.topologyWrapped[topology]=v.masterWrapped;
        if(voiceMode==2)for(int n=21;n<=96;++n)if(held[n]&&n!=v.note){
          for(int i=1;i<voiceLimit;++i)if(!pool[i].active){trigger(pool[i],n,velocities[n],false,-1);pool[i].audibility=0;break;}
        }
      }
    }
    if(changed)prepare();
    if(distortionControl.live())distortionControl.advance();
    lfoValue=lfoValues[0]=lfo();for(int l=1;l<4;++l)lfoValues[l]=lfo(l); if(beatValid && tempoValid && playing)beat+=tempo/(60*sr);
    selectSharedNotes();
    advanceWidth();
    const bool stereo=groupLive&&(state[Width]!=0||panRemaining!=0||panLeft[0]!=1||retiredStereo()||topologyStereo());
    float out=0,outLeft=0,outRight=0;std::array<float,MemberCapacity> groupOutput,groupLeft,groupRight,rendered;
#ifdef XSYNTH_POISON_SCRATCH
    auto poison=std::numeric_limits<float>::quiet_NaN();groupOutput.fill(poison);groupLeft.fill(poison);groupRight.fill(poison);rendered.fill(poison);
#endif
    const int end=renderEnd(),groupEnd=renderGroups();
    // Prepare every retained member before any packed read of the group.
    // Scalar rendering previously updated each member immediately before use.
    if(topologyTailRemaining)for(int i=0;i<end;++i)if(pool[i].active&&topologyRetainedOld[i]>=0)
      pool[i].detunePosition=topologyPositionFrom[i]+(topologyPositionTo[i]-topologyPositionFrom[i])*topologyProgress();
    // Clear only accumulator fields that this frame consumes. Four-float
    // stores stay inside the fixed pool (capacity is a multiple of16).
    if(groupLive){auto zero=_mm_setzero_ps();for(int g=0;g<groupEnd;g+=4){_mm_storeu_ps(groupOutput.data()+g,zero);if(stereo){_mm_storeu_ps(groupLeft.data()+g,zero);_mm_storeu_ps(groupRight.data()+g,zero);}}}
    // Every live member writes rendered[] before use; only a topology handoff
    // can read retained members outside the current live range.
    if(topologyTailRemaining)rendered.fill(0.f);
    for(int i=0;i<end;++i) {
      auto &v=pool[i];if(!v.active)continue;
      int group=v.group,member=v.member,members=v.members;Voice*shared=group>=0&&member>0&&groups[group].noteShared?&pool[group*groupSize]:nullptr;float weighted=renderVoice(v,shared);rendered[i]=weighted;
      if(group>=0){weighted/=members;groupOutput[group]+=weighted;if(stereo){groupLeft[group]+=weighted*panLeft[member];groupRight[group]+=weighted*panRight[member];}}
      else {out+=weighted;outLeft+=weighted;outRight+=weighted;}
    }
    if(groupLive)for(int g=0;g<groupEnd;++g)if(groups[g].active){
      auto &group=groups[g];float value=normalizeGroup(g,groupOutput[g]);
      float left=stereo?normalizeGroup(g,groupLeft[g]):value,right=stereo?normalizeGroup(g,groupRight[g]):value;
      if(group.fault){clearGroup(g);continue;}
      if(group.fade){float t=1-float(group.fade)/transitionSamples;value=group.previous*(1-t)+value*t;left=group.previousLeft*(1-t)+left*t;right=group.previousRight*(1-t)+right*t;--group.fade;}
      group.last=value;group.lastLeft=left;group.lastRight=right;out+=value;outLeft+=left;outRight+=right;
      if(group.noteFinished){clearGroup(g);continue;}
      bool live=false;for(int m=0;m<groupSize;++m)live|=pool[g*groupSize+m].active;
      if(!live)group={};
    }
    while(poolEnd&&!pool[poolEnd-1].active)--poolEnd;
    renderTopologyTransition(out,outLeft,outRight,rendered);
    if(smoothSteals)renderRetirements(out,outLeft,outRight);
    if(distortionControl.live())distortionControl.finish();
    preLimiterOutput=out*gain;
    preOutputLeft=stereo?outLeft*gain:preLimiterOutput;preOutputRight=stereo?outRight*gain:preLimiterOutput;
    if(topologyRemaining){float t=1-float(topologyRemaining)/transitionSamples;preLimiterOutput=topologyFrom*(1-t)+preLimiterOutput*t;preOutputLeft=topologyFromLeft*(1-t)+preOutputLeft*t;preOutputRight=topologyFromRight*(1-t)+preOutputRight*t;if(!--topologyRemaining&&topologyPending)changed=true;}
    return finishOutput();
  }
  float next() {
    if(!groupLive||groupSize<4)return nextScalar();
    batchSaturated=false;
    if(!expandedLive&&!expanded&&!forceExpanded){if(changed)prepare();lfoValue=lfoValues[0]=lfo();for(int l=1;l<4;++l)lfoValues[l]=lfo(l);if(beatValid&&tempoValid&&playing)beat+=tempo/(60*sr);LegacyEngine::next();preLimiterOutput=preOutputLeft=preOutputRight=lastOutputBeforeClamp;return finishOutput();}
    if(!expandedLive){
      auto c=continuity();expandedLive=true;
      if(c.active&&pool[0].active){if(pool[0].group>=0)groups[pool[0].group].noteCoherent=false;auto &v=pool[0];v.firstSample=false;std::copy_n(c.oscillator,2,v.phase);std::copy_n(c.highFilter,4,v.highFilter);v.highG=c.coefficient;v.damping=c.damping;v.resonanceStrength=c.strength;
        for(int e=0;e<2;++e){v.eg[e].value=v.eg[e].start=c.env[e];v.eg[e].stage=c.stage[e];v.eg[e].progress=0;}
        lfoPos=c.lfo;lfoPhaseAccumulator=c.lfo;
        v.cross.reset(0);v.mix.reset(0);v.types.reset(0);v.slope.reset(forceExpanded?previousSlope:state[28]>=.5);
        int topology=forceExpanded?previousSync:state[27]>=.5;v.topology.reset(topology);v.topologyPhase[topology]={v.phase[0],v.phase[1]};v.previousPulseDuty[topology]={c.pulseDuty[0],c.pulseDuty[1]};v.topologyWrapped[topology]=v.masterWrapped;
        if(voiceMode==2)for(int n=21;n<=96;++n)if(held[n]&&n!=v.note){
          for(int i=1;i<voiceLimit;++i)if(!pool[i].active){trigger(pool[i],n,velocities[n],false,-1);pool[i].audibility=0;break;}
        }
      }
    }
    if(changed)prepare();
    if(distortionControl.live())distortionControl.advance();
    lfoValue=lfoValues[0]=lfo();for(int l=1;l<4;++l)lfoValues[l]=lfo(l); if(beatValid && tempoValid && playing)beat+=tempo/(60*sr);
    selectSharedNotes();
    advanceWidth();
    const bool stereo=groupLive&&(state[Width]!=0||panRemaining!=0||panLeft[0]!=1||retiredStereo()||topologyStereo());
    float out=0,outLeft=0,outRight=0;std::array<float,MemberCapacity> groupOutput,groupLeft,groupRight,rendered;
#ifdef XSYNTH_POISON_SCRATCH
    auto poison=std::numeric_limits<float>::quiet_NaN();groupOutput.fill(poison);groupLeft.fill(poison);groupRight.fill(poison);rendered.fill(poison);
#endif
    const int end=renderEnd(),groupEnd=renderGroups();
    // Prepare every retained member before any packed read of the group.
    // Scalar rendering previously updated each member immediately before use.
    if(topologyTailRemaining)for(int i=0;i<end;++i)if(pool[i].active&&topologyRetainedOld[i]>=0)
      pool[i].detunePosition=topologyPositionFrom[i]+(topologyPositionTo[i]-topologyPositionFrom[i])*topologyProgress();
    // Clear only accumulator fields that this frame consumes. Four-float
    // stores stay inside the fixed pool (capacity is a multiple of16).
    if(groupLive){auto zero=_mm_setzero_ps();for(int g=0;g<groupEnd;g+=4){_mm_storeu_ps(groupOutput.data()+g,zero);if(stereo){_mm_storeu_ps(groupLeft.data()+g,zero);_mm_storeu_ps(groupRight.data()+g,zero);}}}
    // Every live member writes rendered[] before use; only a topology handoff
    // can read retained members outside the current live range.
    if(topologyTailRemaining)rendered.fill(0.f);
    BatchMask batchReady;
    for(int i=0;i<end;++i) {
      auto &v=pool[i];if(!v.active)continue;
      int group=v.group,member=v.member,members=v.members;Voice*shared=group>=0&&member>0&&groups[group].noteShared?&pool[group*groupSize]:nullptr;
      if(group>=0&&member==0&&groups[group].noteShared&&groupSize>=4&&fullBatchReady(i,v)){
        advanceSharedNote(v);
        renderBatch<4,true>(i,v,rendered.data()+i);
        if(groupSize==8)renderBatch<4,true>(i+4,v,rendered.data()+i+4);
        // Consume a completed stack in its original scalar summation order.
        // Preserve the leader's pre-render metadata and follower fault skipping.
        for(int lane=0;lane<groupSize;++lane){
          const auto&m=pool[i+lane];if(lane&&!m.active)continue;
          const int count=lane?m.members:members,pan=lane?m.member:member;
          float weighted=rendered[i+lane];weighted/=count;
          groupOutput[group]+=weighted;
          if(stereo){groupLeft[group]+=weighted*panLeft[pan];groupRight[group]+=weighted*panRight[pan];}
        }
        i+=groupSize-1;continue;
      }
      if(shared&&!batchReady.test(i)&&(member==1||member==5)){
        int available=groupSize-member;
        if(available>=4&&renderBatch<4>(i,*shared,rendered.data()+i))batchReady.set(i,15u);
        else if(available==3&&renderBatch<3>(i,*shared,rendered.data()+i))batchReady.set(i,7u);
      }
      float weighted=batchReady.test(i)?rendered[i]:renderVoice(v,shared);rendered[i]=weighted;
      if(group>=0){weighted/=members;groupOutput[group]+=weighted;if(stereo){groupLeft[group]+=weighted*panLeft[member];groupRight[group]+=weighted*panRight[member];}}
      else {out+=weighted;outLeft+=weighted;outRight+=weighted;}
    }
    if(groupLive)for(int g=0;g<groupEnd;++g)if(groups[g].active){
      auto &group=groups[g];float value=normalizeGroup(g,groupOutput[g]);
      float left=stereo?normalizeGroup(g,groupLeft[g]):value,right=stereo?normalizeGroup(g,groupRight[g]):value;
      if(group.fault){clearGroup(g);continue;}
      if(group.fade){float t=1-float(group.fade)/transitionSamples;value=group.previous*(1-t)+value*t;left=group.previousLeft*(1-t)+left*t;right=group.previousRight*(1-t)+right*t;--group.fade;}
      group.last=value;group.lastLeft=left;group.lastRight=right;out+=value;outLeft+=left;outRight+=right;
      if(group.noteFinished){clearGroup(g);continue;}
      bool live=false;for(int m=0;m<groupSize;++m)live|=pool[g*groupSize+m].active;
      if(!live)group={};
    }
    while(poolEnd&&!pool[poolEnd-1].active)--poolEnd;
    renderTopologyTransition(out,outLeft,outRight,rendered);
    if(smoothSteals)renderRetirements(out,outLeft,outRight);
    if(distortionControl.live())distortionControl.finish();
    preLimiterOutput=out*gain;
    preOutputLeft=stereo?outLeft*gain:preLimiterOutput;preOutputRight=stereo?outRight*gain:preLimiterOutput;
    if(topologyRemaining){float t=1-float(topologyRemaining)/transitionSamples;preLimiterOutput=topologyFrom*(1-t)+preLimiterOutput*t;preOutputLeft=topologyFromLeft*(1-t)+preOutputLeft*t;preOutputRight=topologyFromRight*(1-t)+preOutputRight*t;if(!--topologyRemaining&&topologyPending)changed=true;}
    return finishOutput();
  }
private:
  float finishOutput() {
    // Floating-point host output preserves finite peaks and natural beating.
    // Silence a numerical fault only; this is not a level-dependent limiter.
    if(!std::isfinite(preLimiterOutput)||!std::isfinite(preOutputLeft)||!std::isfinite(preOutputRight)){
      ++numericalFaults;preLimiterOutput=preOutputLeft=preOutputRight=0;
    }
    lastPreOutput=preLimiterOutput;
    const bool powered=state[29]>=.5;
    outputLeftValue=powered?preOutputLeft:0;outputRightValue=powered?preOutputRight:0;
    return powered?preLimiterOutput:0;
  }
};
} // namespace xs

