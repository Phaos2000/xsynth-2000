// GPL-2.0-or-later. Fixed-pool digital synthesis expansion, no realtime allocation.
#pragma once
#include <array>
#if defined(_MSC_VER)
#include <intrin.h>
#endif

namespace xs {
class Engine : public LegacyEngine {
  friend struct DspValidationProbe;
  friend struct PreUnisonProbe;
  friend struct UnisonProbe;
  friend struct FourProbe;
  // Long envelopes must keep sub-float increments through sustain/loop edges.
  struct Envelope { double value=0, start=0, progress=0; int stage=0; };
  struct Voice {
    float phase[2]{}, filter[4]{},highFilter[4]{};
    float highG=0,highStep=0,highBlend=0,highBlendStep=0;
    SignalBlend<6> waves[2],mix;
    SignalBlend<2> engines[2],slope;
    SignalBlend<4> topology;
    std::array<std::array<float,2>,4> topologyPhase{};
    std::array<bool,4> topologyWrapped{};
    SignalBlend<3> cross;
    SignalBlend<4> types;
    std::array<int,23> selection=[] {std::array<int,23> a{};a.fill(-1);return a;}();
    int envCurve[2]{1,1},egLoop=0;
    float egVelocity[2]{},egPitch[2]{},egFilter[2]{},lfoPitch=0,lfoFilter=0;
    float tracking=1,tune=1,glideRate=1,audibility=1;
    bool retiring=false;
    Envelope eg[2]{};
    std::array<float,DestinationCount> mod{},stepTarget{},stepValue{},stepDelta{};
    std::array<unsigned char,DestinationCount> stepRemaining{};
    bool modSmoothing=false;
    uint64_t controlDestinations=0;
    float pitchRatio[2]{1,1}, pitchStep[2]{};
    float envelopeRate[2][3]{}, envelopeStep[2][3]{};
    float hz=0, targetHz=0, velocity=1, random=0;
    float g=0, gStep=0, damping=2, dampingStep=0;
    int note=0, noteId=-1, control=0;
    int group=-1,member=0,members=1;
    float detunePosition=0,spread=0;
    uint32_t age=0;
    bool active=false, gate=false, glide=false, masterWrapped=false;
  };
  std::array<Voice,32> pool{};
  std::array<bool,128> held{};
  std::array<float,128> velocities{};
  std::array<float,24> p{};
  float sr=44100, invSr=1/44100.f, lfoPos=0, lfoValue=0, randomA=0, randomB=0;
  float baseRates[2][3]{}, glideCoefficient=1, tuneBend=1, gain=1, lastHz=0;
  uint32_t seed=0x7853796e, serial=0,firstLfoSeed=0x4c464f31;
  double tempo=0, beat=0;
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
  bool reverse=false,matrixActive=false,sampleControl=false;
  int lfoTick=0;
  float lfoIncrement=0,lfoIncrementStep=0,lfoFactor=1;
  struct Route {int source=0,destination=0;float amount=0;};
  std::array<Route,12> activeRoutes{};
  int routeCount=0;
  uint64_t routeDestinations=0;
  bool routesDirty=true, lfoRateRouted=false;
  float normalPowerAlpha=.001f,normalAttackAlpha=.001f,normalReleaseAlpha=.001f,normalSlowAlpha=.00001f,normalPeakRelease=.999f,normalCrestReleaseAlpha=.001f;
  float preLimiterOutput=0;
  unsigned numericalFaults=0;
  float velocitySensitivity[2]{};
  float random() {
    seed^=seed<<13; seed^=seed>>17; seed^=seed<<5;
    return float(seed>>8)*(2.f/16777216.f)-1.f;
  }
  static float flush(float v) { return std::abs(v)<1.e-20f?0:v; }
  static float wrap(float v) { return v-std::floor(v); }
  static float sine(float phase) { return LegacyEngine::sineSample(wrap(phase)); }
  static float raw(float phase,int wave,float shape) {
    switch(wave) {
    case 0:return sine(phase);
    case 1:return phase<.25f?4*phase:phase<.75f?2-4*phase:4*phase-4;
    case 2:return 2*phase-1;
    case 3:return 1-2*phase;
    case 4:return phase<.5f?1.f:-1.f;
    default:return phase<shape?1.f:-1.f;
    }
  }
  float oscillator(Voice &v,int o,float hz,float phaseMod,float shape,bool &wrapped) {
    const float delta=hz*invSr;
    const float unwrapped=v.phase[o]+delta;
    wrapped=unwrapped>=1 || unwrapped<0;
    v.phase[o]=wrap(unwrapped);
    float phase=wrap(v.phase[o]+phaseMod);
    auto &waves=v.waves[o];auto &engines=v.engines[o];
    if(waves.target<0)waves.reset(waveKind[o]);
    if(engines.target<0)engines.reset(engineKind[o]);
    if(!waves.remaining&&!engines.remaining) {
      float sample=raw(phase,waves.target,shape);
      return engines.target?oscillator::clean(phase,delta,waves.target,shape,sample):sample;
    }
    float sample=0;
    for(int k=0;k<6;++k)if(waves.weights[k]!=0){
      float x=raw(phase,k,shape),y=oscillator::clean(phase,delta,k,shape,x);
      sample+=waves.weights[k]*(engines.weights[0]*x+engines.weights[1]*y);
    }
    return sample;
  }

  void classify() {
    expansionBits=0;
    for(int i=36;i<Count;++i) if(i!=Detune && state[i]!=extraDefault(i))expansionBits=1;
    expanded=expansionBits!=0;
  }
  int highest() const { for(int n=96;n>=21;--n)if(held[n])return n; return 0; }
  void trigger(Voice &v,int n,float velocity,bool legato,int id) {
    bool connected=v.active && v.gate;v.retiring=false;v.audibility=1;
    float oldHz=v.hz>0?v.hz:lastHz;
    if(!connected || !legato) { v.eg[0].stage=v.eg[1].stage=0; v.eg[0].progress=v.eg[1].progress=0; v.eg[0].start=v.eg[0].value; v.eg[1].start=v.eg[1].value; }
    v.note=n; v.noteId=id; v.targetHz=440*std::exp2((n-69)/12.f);
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
#include "UnisonGroups.h"
#include "UnisonGain.h"
  void prepare() {
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
    voiceLimit=VoiceCounts[discrete(Voices,state[Voices])];
    if(state[Unison]>0 && !groupLive)enableGroups();
    if(groupLive){
      int count=std::min(voiceLimit,UnisonCounts[discrete(Unison,state[Unison])]);
      if(count!=groupSize || mode!=voiceMode || previousLimit!=voiceLimit)rebuildGroups(count,mode);
    }
    if(!groupLive && mode!=voiceMode && expandedLive) {
      // Keep the selected sounding voice's oscillator/envelope/filter history.
      // Other voices retire individually; never clear or mute the whole pool.
      if(mode!=2) {
        int wanted=highest(),chosen=-1;
        for(int i=0;i<32;++i)if(pool[i].active && pool[i].gate && pool[i].note==wanted){chosen=i;break;}
        if(chosen>0)std::swap(pool[0],pool[chosen]);
        if(chosen<0&&wanted){pool[0]={};trigger(pool[0],wanted,velocities[wanted],false,-1);pool[0].audibility=0;}
        for(int i=1;i<32;++i)if(pool[i].active)pool[i].retiring=true;
      } else for(int n=21;n<=96;++n)if(held[n]){
        bool found=false;for(auto &v:pool)if(v.active&&v.gate&&v.note==n){v.retiring=false;found=true;}
        if(!found)for(int i=0;i<voiceLimit;++i)if(!pool[i].active||pool[i].retiring){pool[i]={};trigger(pool[i],n,velocities[n],false,-1);pool[i].audibility=0;break;}
      }
    }
    voiceMode=mode;
    if(!groupLive && previousLimit!=voiceLimit)for(int i=voiceMode==2?voiceLimit:1;i<32;++i)if(pool[i].active)pool[i].retiring=true;
    for(int o=0;o<2;++o){waveKind[o]=discrete(24+o,state[24+o]);engineKind[o]=state[Engine1+o]>=.5;curveKind[o]=discrete(o?Curve2:Curve1,state[o?Curve2:Curve1]);}
    crossMode=discrete(XMode,state[XMode]);reverse=state[XDirection]>=.5;
    mixerKind=discrete(MixerMode,state[MixerMode]);filterKind=discrete(FilterType,state[FilterType]);filterStages=state[28]<.5?1:2;
    loopKind=discrete(Loop2,state[Loop2]);lfoKind=discrete(26,state[26]);randomKind=discrete(LfoRandom,state[LfoRandom]);phaseKind=discrete(LfoPhase,state[LfoPhase]);
    if(routesDirty) {
      routeCount=0;routeDestinations=0;lfoRateRouted=false;
      for(int i=0;i<MatrixSlots;++i) {
        int id=matrixId(i),source=effectiveSource(state,i);
        float amount=float(state[id+2]*2-1);
        if(source && amount!=0) {
          int destination=routeDestination(state,i);
          activeRoutes[routeCount++]={source,destination,amount};
          routeDestinations|=uint64_t(1)<<destination;
          if(destination==10)lfoRateRouted=true;
        }
      }
      matrixActive=routeCount!=0;routesDirty=false;
    }
    for(int l=1;l<4;++l)extraLfoRates[l-1]=float(physical(4,state[lfoParam(l,0)]));
    sampleControl=matrixActive||p[5]!=0||p[6]!=0;for(int l=1;l<4;++l)sampleControl|=state[lfoParam(l,1)]!=0||state[lfoParam(l,2)]!=0;
    velocitySensitivity[0]=float(state[Velocity1]);velocitySensitivity[1]=float(state[Velocity2]);
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
    if(!index)for(auto &v:pool)if(v.active&&!v.retiring){rateMod+=v.mod[10];phaseMod+=v.mod[26];polarityMod+=v.mod[27];++count;}
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
  float envelope(Voice &v,int index) {
    auto &e=v.eg[index]; int offset=index?15:9;
    int curve=v.selection[index?19:15]<0?curveKind[index]:v.envCurve[index];
    int loop=v.selection[20]<0?loopKind:v.egLoop;
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
  void control(Voice &v,float lfoSample) {
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
    uint64_t pending=v.controlDestinations|routeDestinations;
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
    // Destination identities/step counts are static, not per-voice controls.
    static constexpr auto choices=[] {
      std::array<std::array<int,3>,12> result{};int count=0;
      for(int i=0;i<23;++i){int id=DestinationParams[18+i],steps=stepsFor(id);
        if(steps)result[count++]={i,id,steps};}
      return result;
    }();
    for(const auto &choice:choices){
      int i=choice[0];v.selection[i]=stableChoice(n(choice[1],18+i),choice[2],v.selection[i]);
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
    float hz=v.hz*tuneBend*v.tune;
    float tracked=v.tracking==1?hz:v.tracking==0?440:440*std::pow(std::max(.01f,hz/440),v.tracking);
    float filterLfo=lfoSample*v.lfoFilter*std::max(0.f,1+destinations[11]);for(int l=1;l<4;++l)filterLfo+=lfoValues[l]*float(state[lfoParam(l,2)]);
    float cutoff=tracked*(p[21]+v.egFilter[0]*source[2]+v.egFilter[1]*source[3])*
      std::max(0.f,1+filterLfo)*(destinations[8]!=0?std::exp2(destinations[8]*4):1.f)*(1-float(state[31]));
    cutoff=std::clamp(cutoff,0.f,.49f*sr);
    v.highStep=(highcut::coefficient(cutoff,sr)-v.highG)*.125f;
    v.highBlendStep=(highcut::weight(cutoff,sr)-v.highBlend)*.125f;
    v.gStep=(std::min(.825f,float(2*Pi)*cutoff*invSr)-v.g)*.125f;
    v.dampingStep=(std::clamp(p[22]-destinations[9]*1.5f,.005f,2.f)-v.damping)*.125f;
  }

  float filter(Voice &v,float input) {
    if(v.types.target<0)v.types.reset(filterKind);
    if(v.slope.target<0)v.slope.reset(filterStages-1);
    int stages=v.slope.remaining?2:v.slope.target+1;
    float highInput=input,first=0;
    for(int i=0;i<stages;++i){
      float &band=v.filter[i*2],&low=v.filter[i*2+1];
      low=flush(low+v.g*band);float high=input-low-v.damping*band;band=flush(band+v.g*high);
      float outputs[]={low,high,band,low+high};float next=0;
      if(!v.types.remaining)next=outputs[v.types.target];
      else for(int j=0;j<4;++j)next+=outputs[j]*v.types.weights[j];
      input=next;if(i==0)first=input;
    }
    float lowPath=v.slope.remaining?first*v.slope.weights[0]+input*v.slope.weights[1]:input;
    float highFirst=0;
    for(int i=0;i<stages;++i){float outputs[4]{};highcut::stage(highInput,v.highG,v.damping,v.highFilter+2*i,outputs);highInput=0;
      if(!v.types.remaining)highInput=outputs[v.types.target];else for(int j=0;j<4;++j)highInput+=outputs[j]*v.types.weights[j];if(i==0)highFirst=highInput;}
    float highPath=v.slope.remaining?highFirst*v.slope.weights[0]+highInput*v.slope.weights[1]:highInput;
    return lowPath*(1-v.highBlend)+highPath*v.highBlend;
  }

public:
  Engine()=default;
  void reset(double rate) {
    LegacyEngine::reset(rate); sr=float(rate); invSr=1/sr;
    normalPowerAlpha=float(-std::expm1(-1/(sr*.020)));normalAttackAlpha=float(-std::expm1(-1/(sr*.003)));normalReleaseAlpha=float(-std::expm1(-1/(sr*.035)));normalSlowAlpha=float(-std::expm1(-1/(sr*2.)));normalPeakRelease=float(std::exp(-1/(sr*.050)));normalCrestReleaseAlpha=float(-std::expm1(-1/(sr*.008)));
    extraLfos={};firstLfoSeed=0x4c464f31;lfoValues={};for(int l=0;l<3;++l)extraLfos[l].seed+=l*0x9e3779b9u;
    pool={};groups={};intents={};groupLive=state[Unison]>0;groupSize=groupLive?0:1;groupLimit=8;topologyRemaining=0;topologyFrom=lastPreOutput=0;held={}; velocities={}; key=0; serial=0; seed=0x7853796e;numericalFaults=0;
    lfoPos=0;lfoPhaseAccumulator=previousLfoPhase=0;expandedLive=false;forceExpanded=false;previousSync=state[27]>=.5;previousSlope=state[28]>=.5;hostAnchor=true;effectivePhase=effectivePolarity=-1;polarityMix={};phaseMix={};transitionSamples=std::max(1,int(sr*.002)); lastHz=0; randomA=0; randomB=0;lfoTick=0;lfoIncrement=lfoIncrementStep=0; changed=true;coefficients=(1u<<24)-1;tuningDirty=true;routesDirty=true;classify();
  }
  void set(int id,double value) {
    if(id<0 || id>=Count || id==AllNotesOff || id==AllSoundsOff || !valid(value))return;
    value=stepped(id,value);
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
    LegacyEngine::set(id,value); changed=true;
    if(id<24)coefficients|=1u<<id;
    if(id==30 || id==MasterTune || id==BendRange)tuningDirty=true;
    if((id>=MatrixFirst&&id<PreUnisonCount)||id>=DetuneRouteFirst)routesDirty=true;
    if(matrixSource(id)&&state[SourceExtensionFirst+matrixSlot(id)]!=0){state[SourceExtensionFirst+matrixSlot(id)]=0;routesDirty=true;}
    if(id>=36)classify();
  }
  void transport(double bpm,bool bpmValid,double songBeat,bool songValid,bool running=true) {
    bool changedTempo=tempo!=bpm || tempoValid!=(bpmValid && std::isfinite(bpm) && bpm>0);
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
    LegacyEngine::note(n,down);
    if(changed)prepare();
    if(groupLive){groupNote(n,down,velocity,id);return;}
    bool wasHeld=key!=0; held[n]=down; velocities[n]=std::clamp(velocity,0.f,1.f);
    if(voiceMode==2) {
      if(down) {
        Voice *chosen=nullptr;
        for(int i=0;i<voiceLimit;++i) if(!pool[i].active) { chosen=&pool[i];break; }
        if(!chosen) { chosen=&pool[0]; for(int i=1;i<voiceLimit;++i)
          if((!pool[i].gate && chosen->gate) || (pool[i].gate==chosen->gate && pool[i].age<chosen->age))chosen=&pool[i]; }
        *chosen={}; trigger(*chosen,n,velocity,false,id);
      } else for(auto &v:pool) if(v.active && v.note==n && (id<0 || v.noteId==id))release(v);
    } else {
      int highestNote=highest();
      if(highestNote && (highestNote!=key || (down && n==highestNote)))
        trigger(pool[0],highestNote,velocities[highestNote],voiceMode==1 && wasHeld,id);
      else if(!highestNote)release(pool[0]);
    }
    key=highest();
  }
  void allOff() {
    LegacyEngine::allOff();intents={};for(auto&g:groups)g.gate=false;held={}; key=0; for(auto &v:pool)if(v.active)release(v);
  }
  int activeVoices() const { int n=0;for(auto &v:pool)n+=v.active&&!v.retiring;return n; }
  float modulationPhase() const { return lfoPos; }
  bool usingLegacy() const { return !expandedLive&&!expanded&&!forceExpanded; }
  float next() {
    if(!expandedLive&&!expanded&&!forceExpanded){if(changed)prepare();lfoValue=lfoValues[0]=lfo();for(int l=1;l<4;++l)lfoValues[l]=lfo(l);if(beatValid&&tempoValid&&playing)beat+=tempo/(60*sr);float result=LegacyEngine::next();lastPreOutput=result;return result;}
    if(!expandedLive){
      auto c=continuity();expandedLive=true;
      if(c.active&&pool[0].active){auto &v=pool[0];std::copy_n(c.oscillator,2,v.phase);std::copy_n(c.filter,4,v.filter);std::copy_n(c.highFilter,4,v.highFilter);v.highG=highcut::coefficient(c.highHz,sr);v.highBlend=highcut::weight(c.highHz,sr);v.g=c.coefficient;v.damping=p[22];
        for(int e=0;e<2;++e){v.eg[e].value=v.eg[e].start=c.env[e];v.eg[e].stage=c.stage[e];v.eg[e].progress=0;}
        lfoPos=c.lfo;lfoPhaseAccumulator=c.lfo;
        v.cross.reset(0);v.mix.reset(0);v.types.reset(0);v.slope.reset(forceExpanded?previousSlope:state[28]>=.5);
        int topology=forceExpanded?previousSync:state[27]>=.5;v.topology.reset(topology);v.topologyPhase[topology]={v.phase[0],v.phase[1]};v.topologyWrapped[topology]=v.masterWrapped;
        if(voiceMode==2)for(int n=21;n<=96;++n)if(held[n]&&n!=v.note){
          for(int i=1;i<voiceLimit;++i)if(!pool[i].active){trigger(pool[i],n,velocities[n],false,-1);pool[i].audibility=0;break;}
        }
      }
    }
    if(changed)prepare();
    lfoValue=lfoValues[0]=lfo();for(int l=1;l<4;++l)lfoValues[l]=lfo(l); if(beatValid && tempoValid && playing)beat+=tempo/(60*sr);
    float out=0;std::array<float,32> groupOutput{};if(groupLive)for(auto &group:groups)if(group.active){group.square=0;group.memberPeak=0;}
    for(int i=0;i<32;++i) {
      auto &v=pool[i]; if(!v.active)continue;
      v.hz=v.glide?v.hz+v.glideRate*(v.targetHz-v.hz):v.targetHz;
      if(sampleControl||v.modSmoothing||v.control==0)control(v,lfoValue);
      for(int o=0;o<2;++o){v.waves[o].advance();v.engines[o].advance();}
      v.cross.advance();v.topology.advance();v.mix.advance();v.types.advance();v.slope.advance();
      for(int e=0;e<2;++e) {
        v.pitchRatio[e]+=v.pitchStep[e];
        for(int j=0;j<3;++j)v.envelopeRate[e][j]+=v.envelopeStep[e][j];
      }
      v.control=(v.control+1)&7;
      float env1=envelope(v,0), env2=envelope(v,1);
      float lfoDepth=std::max(0.f,1+v.mod[11]);
      float detuneRatio=1;
      if(v.members>1){
        float target=50*std::clamp(float(state[Detune])+v.mod[41],0.f,1.f);
        v.spread+=(target-v.spread)/transitionSamples;
        detuneRatio=std::exp2(v.detunePosition*v.spread/2400.f);
      }
      float directPitch=lfoValue*lfoDepth*v.lfoPitch;for(int l=1;l<4;++l)directPitch+=lfoValues[l]*float(state[lfoParam(l,1)]);
      float directRatio=std::max(0.f,1+directPitch);
      float hz1=v.hz*v.pitchRatio[0]*detuneRatio*directRatio;
      float hz2=v.hz*v.pitchRatio[1]*detuneRatio*std::max(0.f,1+env1*v.egPitch[0])*std::max(0.f,1+env2*v.egPitch[1])*
        directRatio;
      float shape1=std::clamp(p[1]+v.mod[1],0.f,1.f), shape2=std::clamp(p[3]+v.mod[4],0.f,1.f);
      float depth=std::clamp(float(state[XDepth])+v.mod[6],0.f,1.f);
      float fm=8*depth*v.cross.weights[1],pm=4*depth*v.cross.weights[2];
      float o1=0,o2=0;
      auto oscillators=[&](Voice &path,bool reversed,bool sync,float &a,float &b){
        bool wa=false,wb=false;
        if(!reversed){a=oscillator(path,0,hz1,0,shape1,wa);if(sync&&wa)path.phase[1]=0;
          b=oscillator(path,1,hz2*(1+fm*a),pm*a,shape2,wb);
        }else{if(sync&&path.masterWrapped)path.phase[1]=0;b=oscillator(path,1,hz2,0,shape2,wb);
          a=oscillator(path,0,hz1*(1+fm*b),pm*b,shape1,wa);}
        path.masterWrapped=wa;
      };
      auto path=[&](int t,float weight){
        std::copy_n(v.topologyPhase[t].data(),2,v.phase);v.masterWrapped=v.topologyWrapped[t];
        float a=0,b=0;oscillators(v,t/2!=0,t%2!=0,a,b);
        std::copy_n(v.phase,2,v.topologyPhase[t].data());v.topologyWrapped[t]=v.masterWrapped;
        o1+=weight*a;o2+=weight*b;
      };
      if(v.topology.remaining){for(int t=0;t<4;++t)if(v.topology.weights[t]!=0)path(t,v.topology.weights[t]);}
      else path(v.topology.target,1);
      std::copy_n(v.topologyPhase[v.topology.target].data(),2,v.phase);v.masterWrapped=v.topologyWrapped[v.topology.target];
      o1*=std::clamp(1+v.mod[2],0.f,2.f);o2*=std::clamp(1+v.mod[5],0.f,2.f);
      float b=std::clamp(p[7]+v.mod[7],0.f,1.f), input=0;
      auto mix=[&](int kind){switch(kind){
        case 0:return (1-b)*o1+b*o2;
        case 1:return .5f*(std::min(1.f,2*(1-b))*o1+std::min(1.f,2*b)*o2);
        case 2:return (1-b)*o1+b*o1*o2;
        case 3:return (1-b)*o1+b*o1*(o2+1)*.5f;
        case 4:return (1-b)*o1+b*((o1>=0)!=(o2>=0)?1.f:-1.f);
        default:return (1-b)*o1-b*o2;}};
      if(!v.mix.remaining)input=mix(v.mix.target);else for(int kind=0;kind<6;++kind)input+=v.mix.weights[kind]*mix(kind);
      v.g=std::clamp(v.g+v.gStep,0.f,.825f);v.damping=std::clamp(v.damping+v.dampingStep,.005f,2.f);
      v.highG=std::max(0.f,v.highG+v.highStep);v.highBlend=std::clamp(v.highBlend+v.highBlendStep,0.f,1.f);
      float sample=filter(v,input)*env1;
      if(!std::isfinite(sample)) { ++numericalFaults;if(v.group>=0)groups[v.group].fault=true;v={};continue; }
      if(v.retiring){v.audibility=std::max(0.f,v.audibility-1.f/transitionSamples);}
      else if(v.audibility<1)v.audibility=std::min(1.f,v.audibility+1.f/transitionSamples);
      if(v.group>=0){float weighted=sample*v.audibility;groupOutput[v.group]+=weighted/v.members;groups[v.group].square+=weighted*weighted/v.members;groups[v.group].memberPeak=std::max(groups[v.group].memberPeak,std::abs(weighted));groups[v.group].envelope=env1*v.audibility;}
      else out+=sample*v.audibility;
      if(v.retiring&&v.audibility==0){v={};continue;}
      if(!v.gate && v.eg[0].value<1.e-6f)v={};
    }
    if(groupLive)for(int g=0;g<32;++g)if(groups[g].active){
      auto &group=groups[g];float value=normalizeGroup(g,groupOutput[g]);
      if(group.fault){clearGroup(g);continue;}
      if(group.fade){float t=1-float(group.fade)/transitionSamples;value=group.previous*(1-t)+value*t;--group.fade;}
      group.last=value;out+=value;
      bool live=false;for(int m=0;m<groupSize;++m)live|=pool[g*groupSize+m].active;
      if(!live)group={};
    }
    preLimiterOutput=out*gain;
    if(topologyRemaining){float t=1-float(topologyRemaining)/transitionSamples;preLimiterOutput=topologyFrom*(1-t)+preLimiterOutput*t;--topologyRemaining;}
    lastPreOutput=preLimiterOutput;
    return state[29]>=.5?std::clamp(preLimiterOutput,-1.f,1.f):0.f;
  }
};
} // namespace xs
