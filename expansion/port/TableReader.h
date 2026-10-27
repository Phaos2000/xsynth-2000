// GPL-2.0-or-later. Production PCM8/USER reader; immutable prepared storage.
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>
#include <fstream>
#include <stdexcept>
#include "PositionTuning.h"
#if defined(_MSC_VER)
#define P8_INLINE __forceinline
#else
#define P8_INLINE inline __attribute__((always_inline))
#endif
namespace xs { namespace table {
struct Level {int length=0,harmonic=0;std::vector<float> samples;};
struct Table {
  int id=0,frames=0,length=0,rootBin=1,familyId=0;
  std::array<char,44> name{};
  std::vector<float> pcm;
  std::vector<Level> levels;
};
// Bank owns all memory for the complete experiment. Reader references are const;
// lifetime extends beyond all voices/transitions. Never unload on the audio path.
struct Bank {
  std::vector<Table> tables;
  std::array<uint8_t,32> sourceHash{};
  Bank()=default;
  explicit Bank(const char* path){std::ifstream f(path,std::ios::binary);if(!f)throw std::runtime_error("prepared bank missing");load(f);}
  explicit Bank(std::istream& f){load(f);}
  void load(std::istream& f){
    auto u32=[&](){uint8_t b[4];f.read(reinterpret_cast<char*>(b),4);if(!f)throw std::runtime_error("truncated prepared bank");return uint32_t(b[0])|(uint32_t(b[1])<<8)|(uint32_t(b[2])<<16)|(uint32_t(b[3])<<24);};
    if(u32()!=0x44523850)throw std::runtime_error("not P8RD");
    auto count=u32();if(count>1024)throw std::runtime_error("bad table count");
    f.read(reinterpret_cast<char*>(sourceHash.data()),32);tables.resize(count);
    for(auto&t:tables){t.id=u32();t.frames=u32();t.length=u32();t.rootBin=u32();t.familyId=u32();auto levels=u32();
      if(t.frames<1||t.frames>4096||t.length<1||t.length>65536||t.rootBin<1||levels>32)throw std::runtime_error("invalid descriptor");
      f.read(t.name.data(),t.name.size());t.name.back()=0;t.pcm.resize(size_t(t.frames)*t.length);
      f.read(reinterpret_cast<char*>(t.pcm.data()),t.pcm.size()*sizeof(float));t.levels.resize(levels);
      for(auto&l:t.levels){l.length=u32();l.harmonic=u32();if(l.length<1||l.length>std::max(256,t.length)||l.harmonic>l.length/2)throw std::runtime_error("invalid BAND descriptor");
        l.samples.resize(size_t(t.frames)*l.length);f.read(reinterpret_cast<char*>(l.samples.data()),l.samples.size()*sizeof(float));}
      if(!f||t.levels.empty()||t.levels.back().harmonic!=0)throw std::runtime_error("incomplete prepared bank");
    }
    if(f.peek()!=EOF)throw std::runtime_error("trailing prepared bytes");
  }
};
enum class Sample {Hold,Linear,Band};
enum class Frame {Step,Morph};
inline double wrap(double p){return p-std::floor(p);}
inline double cycleRate(double intended,const Table&t){return intended/t.rootBin;}
inline float linear(const float*data,int n,double p){
  double x=wrap(p)*n;int i=std::min(n-1,int(x));float f=float(x-i);
  return data[i]+f*(data[(i+1)%n]-data[i]);
}
struct ReadStats {
  uint64_t heads=0,frameSelections=0,frameReads=0,bandSelections=0,addresses=0,loads=0;
};
struct FrameChoice {int a=0,b=0;float blend=0;};
struct BandChoice {const Level*high=nullptr;const Level*low=nullptr;float blend=0;};
P8_INLINE FrameChoice chooseFrames(const Table&t,float position,Frame frame,ReadStats*s=nullptr){
  if(s)++s->frameSelections;
  float u=std::clamp(position,0.f,1.f)*(t.frames-1);
  if(frame==Frame::Step){int a=std::min(t.frames-1,int(u+.5f));return {a,a,0};}
  int a=std::min(t.frames-1,int(u));return {a,std::min(a+1,t.frames-1),u-a};
}
P8_INLINE BandChoice chooseBand(const Table&t,double frequency,double sr,ReadStats*s=nullptr){
  if(s)++s->bandSelections;
  double available=frequency==0?1e300:.45*sr/std::abs(frequency);
  for(size_t i=0;i+1<t.levels.size();++i){const auto&a=t.levels[i];if(a.harmonic>available)continue;
    return {&a,&t.levels[i+1],float(std::clamp(available/a.harmonic-1.,0.,1.))};}
  return {nullptr,&t.levels.back(),0};
}
struct Address {int a,b;float fraction;};
P8_INLINE Address address(double wrapped,int length,ReadStats*s=nullptr){
  if(s)++s->addresses;
  double x=wrapped*length;int a=std::min(length-1,int(x));return {a,a+1==length?0:a+1,float(x-a)};
}
P8_INLINE float at(const float*data,Address a,ReadStats*s){
  if(s)s->loads+=2;
  float x=data[a.a];return x+a.fraction*(data[a.b]-x);
}
struct ReadPlan {Sample sample;BandChoice band;Address high{},low{};};
P8_INLINE ReadPlan plan(const Table&t,double wrapped,Sample sample,BandChoice band,ReadStats*s=nullptr){
  ReadPlan p{sample,band};
  if(sample!=Sample::Band){p.high=address(wrapped,t.length,s);return p;}
  if(band.low->length>1)p.low=address(wrapped,band.low->length,s);
  if(band.high&&band.blend!=0){p.high=band.high->length==band.low->length?p.low:address(wrapped,band.high->length,s);}
  return p;
}
P8_INLINE float plannedFrame(const Table&t,int frame,const ReadPlan&p,ReadStats*s=nullptr){
  if(s)++s->frameReads;
  if(p.sample==Sample::Hold){if(s)++s->loads;return t.pcm[size_t(frame)*t.length+p.high.a];}
  if(p.sample==Sample::Linear)return at(t.pcm.data()+size_t(frame)*t.length,p.high,s);
  auto level=[&](const Level*l,Address a){if(l->length==1){if(s)++s->loads;return l->samples[frame];}return at(l->samples.data()+size_t(frame)*l->length,a,s);};
  // Weight zero and DC elide exactly. Weight one retains legacy float blending:
  // low+(high-low) is not universally bit-identical to high in float arithmetic.
  if(!p.band.high||p.band.blend==0)return level(p.band.low,p.low);
  float high=level(p.band.high,p.high),low=level(p.band.low,p.low);
  return low+p.band.blend*(high-low);
}
P8_INLINE float planned(const Table&t,FrameChoice f,const ReadPlan&p,ReadStats*s=nullptr){
  float a=plannedFrame(t,f.a,p,s);
  if(f.blend==0||f.a==f.b)return a;
  return a+f.blend*(plannedFrame(t,f.b,p,s)-a);
}
inline float frameRead(const Table&t,int frame,double phase,Sample mode,double actualCycleHz,double sr){
  auto band=mode==Sample::Band?chooseBand(t,actualCycleHz,sr):BandChoice{};
  return plannedFrame(t,frame,plan(t,wrap(phase),mode,band));
}
inline float read(const Table&t,double phase,float position,Sample sample,Frame frame,double actualCycleHz,double sr){
  auto band=sample==Sample::Band?chooseBand(t,actualCycleHz,sr):BandChoice{};
  return planned(t,chooseFrames(t,position,frame),plan(t,wrap(phase),sample,band));
}
struct Config {
  const Table* table=nullptr;Sample sample=Sample::Hold;Frame frame=Frame::Step;
  bool operator==(const Config&b)const{return table==b.table&&sample==b.sample&&frame==b.frame;}
  bool operator!=(const Config&b)const{return !(*this==b);}
};
enum class PhaseTransfer {SameCycle,RootNearest};
inline double transfer(double phase,int oldRoot,int newRoot,PhaseTransfer policy){
  if(oldRoot==newRoot||policy==PhaseTransfer::SameCycle)return phase;
  double rootPhase=wrap(phase*oldRoot);
  double nearest=std::round(phase*newRoot-rootPhase);
  return wrap((rootPhase+nearest)/newRoot);
}
struct ClockHead {
  Config config{};double phase=0,frequency=0,lastFrequency=-1,lastSr=-1;BandChoice band{};
  double lastRootHz=-1,lastClockSr=-1,increment=0;
  void assign(Config c,double p){config=c;phase=p;lastFrequency=lastSr=lastRootHz=lastClockSr=-1;}
  P8_INLINE void clock(double rootHz,double sr,float position=0){
    rootHz=positionTuning::frequency(config.table->id,rootHz,position);
    if(rootHz!=lastRootHz||sr!=lastClockSr){frequency=cycleRate(rootHz,*config.table);increment=frequency/sr;lastRootHz=rootHz;lastClockSr=sr;}
    double advanced=phase+increment;
    // Most audio-rate increments remain inside this already-wrapped cycle.
    // Keep floor fallback for signed, multi-cycle and nonlocal advances.
    phase=advanced>=0&&advanced<1?advanced:wrap(advanced);
  }
  P8_INLINE float evaluate(FrameChoice frames,double sr,ReadStats*s=nullptr){
    if(s)++s->heads;
    if(config.sample==Sample::Band&&(frequency!=lastFrequency||sr!=lastSr)){
      band=chooseBand(*config.table,frequency,sr,s);lastFrequency=frequency;lastSr=sr;}
    return planned(*config.table,frames,plan(*config.table,phase,config.sample,band,s),s);
  }
};
// Canonical table-edit prototype: independently tuned old/new phase clocks.
// The earlier externally-clocked Transition remains a comparison primitive;
// callers changing rootBin must use this clock-owning implementation.
struct ClockTransition {
  ClockHead current{},target{};Config pending{};bool active=false,hasPending=false;
  int progress=0,length=1;PhaseTransfer mapping=PhaseTransfer::RootNearest;
  void reset(Config c,double sr,double phase=0,PhaseTransfer policy=PhaseTransfer::RootNearest){
    current.assign(c,wrap(phase));target=current;active=hasPending=false;progress=0;length=std::max(2,int(sr*.002));mapping=policy;}
  void start(Config c){target.assign(c,transfer(current.phase,current.config.table->rootBin,c.table->rootBin,mapping));active=true;progress=0;}
  void request(Config c){if(active){pending=c;hasPending=c!=target.config;return;}if(c!=current.config)start(c);}
  P8_INLINE float next(double rootHz,float position,double sr,ReadStats*s=nullptr){
    current.clock(rootHz,sr,position);if(active)target.clock(rootHz,sr,position);
    FrameChoice frames=chooseFrames(*current.config.table,position,current.config.frame,s);
    if(!active)return current.evaluate(frames,sr,s);
    float weight=float(progress)/(length-1),value=0;
    FrameChoice other=target.config.table->frames==current.config.table->frames&&target.config.frame==current.config.frame?
      frames:chooseFrames(*target.config.table,position,target.config.frame,s);
    if(weight==0)value=current.evaluate(frames,sr,s);
    else if(weight==1)value=target.evaluate(other,sr,s);
    else {float a=current.evaluate(frames,sr,s),b=target.evaluate(other,sr,s);value=a+weight*(b-a);}
    if(++progress>=length){current=target;active=false;if(hasPending){auto c=pending;hasPending=false;if(c!=current.config)start(c);}}
    return value;
  }
};
}} // namespace xs::table
