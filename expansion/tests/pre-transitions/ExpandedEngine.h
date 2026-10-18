// GPL-2.0-or-later. Fixed-pool digital synthesis expansion, no realtime allocation.
#pragma once
#include <array>

namespace xs {
class Engine : public LegacyEngine {
  friend struct DspValidationProbe;
  // Long envelopes must keep sub-float increments through sustain/loop edges.
  struct Envelope { double value=0, start=0, progress=0; int stage=0; };
  struct Voice {
    float phase[2]{}, filter[4]{};
    Envelope eg[2]{};
    std::array<float,DestinationCount> mod{}, increment{};
    float pitchRatio[2]{1,1}, pitchStep[2]{};
    float envelopeRate[2][3]{}, envelopeStep[2][3]{};
    float hz=0, targetHz=0, velocity=1, random=0;
    float g=0, gStep=0, damping=2, dampingStep=0;
    int note=0, noteId=-1, control=0;
    uint32_t age=0;
    bool active=false, gate=false, glide=false, masterWrapped=false;
  };
  std::array<Voice,32> pool{};
  std::array<bool,128> held{};
  std::array<float,128> velocities{};
  std::array<float,24> p{};
  float sr=44100, invSr=1/44100.f, lfoPos=0, lfoValue=0, randomA=0, randomB=0;
  float baseRates[2][3]{}, glideCoefficient=1, tuneBend=1, gain=1, lastHz=0;
  uint32_t seed=0x7853796e, serial=0;
  double tempo=0, beat=0;
  bool tempoValid=false, beatValid=false, playing=false, expanded=false, changed=true;
  int voiceMode=0, voiceLimit=8, key=0;
  uint64_t expansionBits=0;
  uint32_t coefficients=(1u<<24)-1;
  bool tuningDirty=true;
  int waveKind[2]{}, engineKind[2]{}, crossMode=0, mixerKind=0, filterKind=0, filterStages=1;
  int curveKind[2]{1,1}, loopKind=0, lfoKind=0, randomKind=0, phaseKind=0;
  bool reverse=false,matrixActive=false;
  int lfoTick=0;
  float lfoIncrement=0,lfoIncrementStep=0,lfoFactor=1;
  struct Route {int source=0,destination=0;float amount=0;};
  std::array<Route,6> activeRoutes{};
  int routeCount=0;
  bool routesDirty=true, lfoRateRouted=false;
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
    int wave=waveKind[o];
    float sample=raw(phase,wave,shape);
    return engineKind[o]?oscillator::clean(phase,delta,wave,shape,sample):sample;
  }

  void classify() {
    expansionBits=0;
    for(int i=36;i<Count;++i) if(state[i]!=extraDefault(i))expansionBits|=uint64_t(1)<<(i-34);
    expanded=expansionBits!=0;
  }
  int highest() const { for(int n=96;n>=21;--n)if(held[n])return n; return 0; }
  void trigger(Voice &v,int n,float velocity,bool legato,int id) {
    bool connected=v.active && v.gate;
    float oldHz=v.hz>0?v.hz:lastHz;
    if(!connected || !legato) { v.eg[0].stage=v.eg[1].stage=0; v.eg[0].progress=v.eg[1].progress=0; v.eg[0].start=v.eg[0].value; v.eg[1].start=v.eg[1].value; }
    v.note=n; v.noteId=id; v.targetHz=440*std::exp2((n-69)/12.f);
    lastHz=v.targetHz;
    int mode=discrete(GlideMode,state[GlideMode]);
    bool any=key!=0;
    v.glide=mode==1 || (mode==2 && any) || (mode==3 && !any);
    v.hz=v.glide && oldHz>0?oldHz:v.targetHz;
    v.velocity=velocity; v.random=random(); v.gate=v.active=true; v.age=++serial; v.control=0;
    for(int o=0;o<2;++o) {
      v.pitchRatio[o]=tuneBend*p[o?2:0];v.pitchStep[o]=0;
      for(int j=0;j<3;++j) {v.envelopeRate[o][j]=baseRates[o][j];v.envelopeStep[o][j]=0;}
    }
    if(discrete(LfoPhase,state[LfoPhase])==1) lfoPos=0;
  }
  static void release(Voice &v) {
    v.gate=false;
    for(auto &e:v.eg) { e.stage=2; e.progress=0; e.start=e.value; }
  }
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
    if(mode!=voiceMode) {
      for(auto &v:pool) v={};
      voiceMode=mode;
      if(mode!=2 && highest()) trigger(pool[0],highest(),velocities[highest()],false,-1);
      else if(mode==2) for(int n=21;n<=96;++n) if(held[n]) { auto &v=pool[serial%voiceLimit];trigger(v,n,velocities[n],false,-1); }
    }
    if(previousLimit!=voiceLimit)for(int i=voiceMode==2?voiceLimit:1;i<32;++i)pool[i]={};
    for(int o=0;o<2;++o){waveKind[o]=discrete(24+o,state[24+o]);engineKind[o]=state[Engine1+o]>=.5;curveKind[o]=discrete(o?Curve2:Curve1,state[o?Curve2:Curve1]);}
    crossMode=discrete(XMode,state[XMode]);reverse=state[XDirection]>=.5;
    mixerKind=discrete(MixerMode,state[MixerMode]);filterKind=discrete(FilterType,state[FilterType]);filterStages=state[28]<.5?1:2;
    loopKind=discrete(Loop2,state[Loop2]);lfoKind=discrete(26,state[26]);randomKind=discrete(LfoRandom,state[LfoRandom]);phaseKind=discrete(LfoPhase,state[LfoPhase]);
    if(routesDirty) {
      routeCount=0;lfoRateRouted=false;
      for(int i=0;i<6;++i) {
        int id=MatrixFirst+i*3,source=discrete(id,state[id]);
        float amount=float(state[id+2]*2-1);
        if(source && amount!=0) {
          int destination=discrete(id+1,state[id+1]);
          activeRoutes[routeCount++]={source,destination,amount};
          if(destination==10)lfoRateRouted=true;
        }
      }
      matrixActive=routeCount!=0;routesDirty=false;
    }
    velocitySensitivity[0]=float(state[Velocity1]);velocitySensitivity[1]=float(state[Velocity2]);
    changed=false;
  }
  float lfo() {
    float previous=lfoPos;
    double duration=divisionBeats(state[Division]);
    if(!lfoTick) {
      float rate=state[LfoClock]<.5?p[4]:(tempoValid?float(tempo/(60*duration)):0.f);
      float rateMod=0; int count=0;
      if(lfoRateRouted)for(auto &v:pool)if(v.active){rateMod+=v.mod[10];++count;}
      lfoFactor=rateMod!=0?std::exp2(std::clamp(rateMod/count,-4.f,4.f)*4):1.f;
      lfoIncrementStep=(rate*lfoFactor*invSr-lfoIncrement)*.125f;
    }
    lfoTick=(lfoTick+1)&7;lfoIncrement+=lfoIncrementStep;
    if(phaseKind==2 && beatValid && tempoValid) {
      double cycles=(state[LfoClock]>=.5?beat/duration:beat*60/tempo*p[4])*lfoFactor;
      lfoPos=float(cycles-std::floor(cycles));
      if(randomKind) {
        auto hash=[](uint32_t x) {x^=x>>16;x*=0x7feb352du;x^=x>>15;x*=0x846ca68bu;x^=x>>16;return float(x>>8)*(2.f/16777216.f)-1.f;};
        uint32_t cycle=uint32_t(int64_t(std::floor(std::clamp(cycles,-1.e12,1.e12))));
        randomA=hash(cycle);randomB=hash(cycle+1);
      }
    } else lfoPos=wrap(lfoPos+lfoIncrement);
    if(lfoPos<previous && !(phaseKind==2 && beatValid && tempoValid)) { randomA=randomB; randomB=random(); }
    float value=randomKind==1?randomA:randomKind==2?randomA+(randomB-randomA)*lfoPos*lfoPos*(3-2*lfoPos):raw(lfoPos,lfoKind,.25f);
    return state[LfoPolarity]<.5?value:(value+1)*.5f;
  }
  float envelope(Voice &v,int index) {
    auto &e=v.eg[index]; int offset=index?15:9;
    int curve=curveKind[index];
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
      else if(index==1 && v.gate && loopKind && (e.stage==1 || e.stage==2)) {
        e.stage=e.stage==1 && loopKind==2?2:0;
        e.start=e.value; e.progress=0;
      }
    }
    if(std::abs(e.value)<1.e-20)e.value=0;
    return float(e.value*(1-velocitySensitivity[index]+velocitySensitivity[index]*v.velocity));
  }
  void control(Voice &v,float lfoSample) {
    std::array<float,DestinationCount> destinations{};
    const float source[]={0,lfoSample,float(v.eg[0].value*(1-velocitySensitivity[0]+velocitySensitivity[0]*v.velocity)),
      float(v.eg[1].value*(1-velocitySensitivity[1]+velocitySensitivity[1]*v.velocity)),v.velocity,(v.note-60)/36.f,
      float(state[31]),float(state[Pressure]),float(state[30]*2-1),v.random,1};
    for(int i=0;i<routeCount;++i) {
      const auto &route=activeRoutes[i];
      destinations[route.destination]+=source[route.source]*route.amount;
    }
    for(int i=0;i<DestinationCount;++i) {
      destinations[i]=std::clamp(destinations[i],-4.f,4.f);
      v.increment[i]=(destinations[i]-v.mod[i])*.125f;
    }
    for(int e=0;e<2;++e) {
      float modulation=destinations[e?3:0];
      float ratio=tuneBend*p[e?2:0]*(modulation!=0?std::exp2(modulation*4):1.f);
      v.pitchStep[e]=(ratio-v.pitchRatio[e])*.125f;
      for(int j=0;j<3;++j) {
        float amount=destinations[(e?15:12)+j];
        float rate=std::clamp(baseRates[e][j]*(amount!=0?std::exp2(-4*amount):1.f),0.f,1.f);
        v.envelopeStep[e][j]=(rate-v.envelopeRate[e][j])*.125f;
      }
    }
    float hz=v.hz*tuneBend;
    float tracked=state[Keytrack]==1?hz:state[Keytrack]==0?440:440*std::pow(std::max(.01f,hz/440),float(state[Keytrack]));
    float cutoff=tracked*(p[21]+p[14]*source[2]+p[20]*source[3])*
      std::max(0.f,1+lfoSample*p[6]*std::max(0.f,1+destinations[11]))*(destinations[8]!=0?std::exp2(destinations[8]*4):1.f)*(1-float(state[31]));
    cutoff=std::clamp(cutoff,0.f,.2f*sr);
    v.gStep=(std::min(.825f,float(2*Pi)*cutoff*invSr)-v.g)*.125f;
    v.dampingStep=(std::clamp(p[22]-destinations[9]*1.5f,.005f,2.f)-v.damping)*.125f;
  }
  float filter(Voice &v,float input) {
    int type=filterKind, stages=filterStages;
    for(int i=0;i<stages;++i) {
      float &band=v.filter[i*2], &low=v.filter[i*2+1];
      low=flush(low+v.g*band);
      float high=input-low-v.damping*band;
      band=flush(band+v.g*high);
      input=type==0?low:type==1?high:type==2?band:low+high;
    }
    return input;
  }
public:
  Engine()=default;
  void reset(double rate) {
    LegacyEngine::reset(rate); sr=float(rate); invSr=1/sr;
    pool={}; held={}; velocities={}; key=0; serial=0; seed=0x7853796e;
    lfoPos=0; lastHz=0; randomA=0; randomB=0;lfoTick=0;lfoIncrement=lfoIncrementStep=0; changed=true;coefficients=(1u<<24)-1;tuningDirty=true;routesDirty=true;classify();
  }
  void set(int id,double value) {
    if(id<0 || id>=Count || id==AllNotesOff || id==AllSoundsOff || !valid(value))return;
    value=stepped(id,value); if(value==state[id])return;
    LegacyEngine::set(id,value); changed=true;
    if(id<24)coefficients|=1u<<id;
    if(id==30 || id==MasterTune || id==BendRange)tuningDirty=true;
    if(id>=MatrixFirst)routesDirty=true;
    if(id>=36) { uint64_t bit=uint64_t(1)<<(id-34);if(value==extraDefault(id))expansionBits&=~bit;else expansionBits|=bit;expanded=expansionBits!=0; }
  }
  void transport(double bpm,bool bpmValid,double songBeat,bool songValid,bool running=true) {
    bool changedTempo=tempo!=bpm || tempoValid!=(bpmValid && std::isfinite(bpm) && bpm>0);
    tempo=bpm; tempoValid=bpmValid && std::isfinite(bpm) && bpm>0;
    beat=songBeat; beatValid=songValid && std::isfinite(songBeat);
    playing=running;
    if(changedTempo)lfoTick=0;
    if(state[LfoClock]>=.5 && !tempoValid)lfoIncrement=lfoIncrementStep=0;
  }
  void note(int n,bool down,float velocity=1,int id=-1) {
    if(n<21 || n>96)return;
    LegacyEngine::note(n,down);
    if(changed)prepare();
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
    LegacyEngine::allOff(); held={}; key=0; for(auto &v:pool)if(v.active)release(v);
  }
  int activeVoices() const { int n=0;for(auto &v:pool)n+=v.active;return n; }
  float modulationPhase() const { return lfoPos; }
  bool usingLegacy() const { return !expanded; }
  float next() {
    if(!expanded) return LegacyEngine::next();
    if(changed)prepare();
    lfoValue=lfo(); if(beatValid && tempoValid && playing)beat+=tempo/(60*sr);
    float out=0;
    for(int i=0;i<(voiceMode==2?voiceLimit:1);++i) {
      auto &v=pool[i]; if(!v.active)continue;
      v.hz=v.glide?v.hz+glideCoefficient*(v.targetHz-v.hz):v.targetHz;
      if(v.control==0)control(v,lfoValue);
      for(int j=0;j<DestinationCount;++j)v.mod[j]+=v.increment[j];
      for(int e=0;e<2;++e) {
        v.pitchRatio[e]+=v.pitchStep[e];
        for(int j=0;j<3;++j)v.envelopeRate[e][j]+=v.envelopeStep[e][j];
      }
      v.control=(v.control+1)&7;
      float env1=envelope(v,0), env2=envelope(v,1);
      float lfoDepth=std::max(0.f,1+v.mod[11]);
      float hz1=v.hz*v.pitchRatio[0];
      float hz2=v.hz*v.pitchRatio[1]*std::max(0.f,1+env1*p[13])*std::max(0.f,1+env2*p[19])*
        std::max(0.f,1+lfoValue*lfoDepth*p[5]);
      float shape1=std::clamp(p[1]+v.mod[1],0.f,1.f), shape2=std::clamp(p[3]+v.mod[4],0.f,1.f);
      float depth=std::clamp(float(state[XDepth])+v.mod[6],0.f,1.f);
      int mode=crossMode;
      float o1=0,o2=0; bool wrap1=false,wrap2=false;
      if(!reverse) {
        o1=oscillator(v,0,hz1,0,shape1,wrap1);
        if(state[27]>=.5 && wrap1)v.phase[1]=0;
        o2=oscillator(v,1,hz2*(mode==1?1+8*depth*o1:1),mode==2?4*depth*o1:0,shape2,wrap2);
      } else {
        if(state[27]>=.5 && v.masterWrapped)v.phase[1]=0;
        o2=oscillator(v,1,hz2,0,shape2,wrap2);
        o1=oscillator(v,0,hz1*(mode==1?1+8*depth*o2:1),mode==2?4*depth*o2:0,shape1,wrap1);
      }
      v.masterWrapped=wrap1;
      o1*=std::clamp(1+v.mod[2],0.f,2.f);o2*=std::clamp(1+v.mod[5],0.f,2.f);
      float b=std::clamp(p[7]+v.mod[7],0.f,1.f), input=0;
      switch(mixerKind) {
      case 0:input=(1-b)*o1+b*o2;break;
      case 1:input=.5f*(std::min(1.f,2*(1-b))*o1+std::min(1.f,2*b)*o2);break;
      case 2:input=(1-b)*o1+b*o1*o2;break;
      case 3:input=(1-b)*o1+b*o1*(o2+1)*.5f;break;
      case 4:input=(1-b)*o1+b*((o1>=0)!=(o2>=0)?1.f:-1.f);break;
      case 5:input=(1-b)*o1-b*o2;break;
      }
      v.g=std::clamp(v.g+v.gStep,0.f,.825f);v.damping=std::clamp(v.damping+v.dampingStep,.005f,2.f);
      float sample=filter(v,input)*env1;
      if(!std::isfinite(sample)) { v={};continue; }
      out+=sample;
      if(!v.gate && v.eg[0].value<1.e-6f)v={};
    }
    return state[29]>=.5?std::clamp(out*gain,-1.f,1.f):0.f;
  }
};
} // namespace xs
