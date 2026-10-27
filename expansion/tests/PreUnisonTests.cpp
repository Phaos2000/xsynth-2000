// GPL-2.0-or-later. Development-only transition/modulation/resonance audit.
#include "Core.h"
#include "HistoricalState.h"
#define xs frozen_xs
#include "pre-transitions/Core.h"
#undef xs
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <new>
#include <stdexcept>
std::atomic<size_t> allocations{0};
void *operator new(size_t n){++allocations;if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void *operator new[](size_t n){return ::operator new(n);}
void operator delete(void*p)noexcept{std::free(p);}void operator delete[](void*p)noexcept{std::free(p);}
void operator delete(void*p,size_t)noexcept{std::free(p);}void operator delete[](void*p,size_t)noexcept{std::free(p);}
namespace xs {
struct PreUnisonProbe {
 static auto &voice(Engine&e){return e.pool[0];}
 static float raw(Engine&e){return e.preLimiterOutput;}
 static unsigned faults(Engine&e){return e.numericalFaults;}
 static float lfo(Engine&e){return e.lfoValue;}
 static float filterPeak(Engine&e){float p=0;for(auto&v:e.pool)for(float x:v.highFilter){if(!std::isfinite(x))return INFINITY;p=std::max(p,std::abs(x));}return p;}
 static std::array<float,4> filterState(Engine&e){auto&v=e.pool[0];return {v.highFilter[0],v.highFilter[1],v.highFilter[2],v.highFilter[3]};}
};
}
namespace frozen_xs {struct DspValidationProbe {
 static std::array<float,4> filterState(Engine&e){auto&v=e.pool[0];return {v.filter[0],v.filter[1],v.filter[2],v.filter[3]};}
};}
using namespace xs;
void check(bool ok,const char*m){if(!ok)throw std::runtime_error(m);}
void option(Engine&e,int id,int k){if(legacyDestination(id)){if(k==EstablishedDestinationCount)e.set(DetuneRouteFirst+(id-MatrixFirst)/3,1.);else e.set(ExpandedDestinationFirst+(id-MatrixFirst)/3,(k+1)/double(EstablishedDestinationCount));}else e.set(id,double(k)/stepsFor(id));}
void setup(Engine&e,double rate=48000){e.reset(rate);option(e,VoiceMode,2);e.set(23,.2);e.set(24,0);e.set(25,.4);e.set(7,.5);e.set(21,.25);}
void advance(Engine&e,int n){for(int i=0;i<n;++i)check(std::isfinite(e.next()),"Nonfinite audio");}
void route(Engine&e,int destination,int source=10,double amount=.9){option(e,MatrixFirst,source);option(e,MatrixFirst+1,destination);e.set(MatrixFirst+2,amount);}
void migration(){
 for(int version:{1,2})for(int destination=0;destination<18;++destination){
  State old=defaults();for(int slot=0;slot<6;++slot){old[MatrixFirst+slot*3]=.1;old[MatrixFirst+slot*3+1]=destination/17.;old[MatrixFirst+slot*3+2]=.71;}
  auto b=historicalState(old);b.resize(8+PreviousCount*8);b[4]=uint8_t(version);State restored{};check(decode(b.data(),b.size(),restored),"Old native state rejected");
  for(int slot=0;slot<6;++slot){int id=MatrixFirst+slot*3;check(discrete(id+1,restored[id+1])==destination,"Old destination silently rerouted");check(restored[id]==old[id]&&restored[id+2]==old[id+2],"Route source/amount changed");}
 }
 for(int d=0;d<42;++d){State s=defaults();if(d==EstablishedDestinationCount)s[DetuneRouteFirst]=1;else s[ExpandedDestinationFirst]=(d+1)/double(EstablishedDestinationCount);auto b=encode(s);State t{};check(b[4]==7&&decode(b.data(),b.size(),t)&&s==t,"V3 destination roundtrip");}
 std::cout<<"PASS all 18 old destinations, six slots, XSED v1/v2 migration; all 41 v3 choices\n";
}
void transitions(){
 for(double sr:{44100.,48000.,96000.})for(int id:std::array<int,8>{XMode,MixerMode,24,VoiceMode,XDirection,27,28,Engine1}){
  Engine control,edited;setup(control,sr);setup(edited,sr);
  for(auto *e:{&control,&edited}){e->set(XDepth,.6);if(id==XMode)option(*e,XMode,1);e->note(60,true);if(id==VoiceMode){e->note(64,true);e->note(67,true);}advance(*e,4096);}
  edited.set(id,id==XMode?1.:id==MixerMode?1.:id==24?.8:.5);
  float a=control.next(),b=edited.next();check(std::abs(a-b)<.025,"Discrete edit injected hard output step");
  for(int n=0;n<12000;++n){if(n%17==0){edited.set(id,(n/17)%2?0:1);option(edited,XDirection,(n/17)%2);}float x=edited.next();check(std::isfinite(x),"Rapid transition nonfinite");}
  edited.allOff();advance(edited,int(sr));check(edited.activeVoices()==0&&edited.next()==0,"Rapid switching stuck voices");check(PreUnisonProbe::faults(edited)==0,"Transition numerical fault");
 }
 // First activation of expanded signal paths must preserve the old sounding history.
 Engine a,b;a.reset(48000);b.reset(48000);a.set(23,.2);b.set(23,.2);a.note(60,true);b.note(60,true);advance(a,4096);advance(b,4096);b.set(XMode,.5);check(std::abs(a.next()-b.next())<.01,"Legacy/expanded handoff click");
 std::cout<<"PASS local crossfades, first handoff and repeated 17-sample switching across three rates; no stuck notes/faults\n";
}
void lfo(){
 for(double sr:{44100.,48000.,96000.})for(double hz:{.1,9.5}){
  Engine e;setup(e,sr);e.set(4,normalized(4,hz));route(e,0,1,.55);e.note(60,true);auto before=allocations.load();
  for(int n=0;n<int(sr/4);++n){e.next();float l=PreUnisonProbe::lfo(e);float expected=std::exp2(l*.1f*4);check(std::abs(PreUnisonProbe::voice(e).pitchRatio[0]-expected)<2.e-6,"LFO pitch target held at control rate");}
  check(allocations.load()==before,"LFO path allocated");
 }
 for(int block:{1,7,32,128,512}){
  Engine e,reference;for(auto*p:{&e,&reference}){setup(*p);p->set(LfoClock,1);p->set(LfoPhase,1);route(*p,10,10,.6);p->note(60,true);}
  for(int n=0;n<12000;++n){double beat=100000+n*2./48000;if(n%block==0)e.transport(120,true,beat,true,true);reference.transport(120,true,beat,true,true);float a=e.next(),b=reference.next();check(a==b,"Host LFO depends on block size");if(n==6000){e.set(MatrixFirst+2,.8);reference.set(MatrixFirst+2,.8);}check(PreUnisonProbe::faults(e)==0,"LFO feedback fault");}
 }
 std::cout<<"PASS slow/fast continuous LFO pitch per sample, host-rate feedback at large song position, five buffer sizes\n";
}
void destinations(){
 for(int d=18;d<42;++d){Engine e;setup(e);route(e,d);e.note(60,true,.3);advance(e,512);auto &v=PreUnisonProbe::voice(e);
  check(std::abs(v.mod[d]-.8f)<1.e-6,"New destination not connected");
  int id=DestinationParams[d],steps=stepsFor(id);
  if(steps)check(v.selection[d-18]==int(std::round(std::clamp(float(e.state[id])+.8f,0.f,1.f)*steps)),"Discrete target does not select effective value");
  if(d==25)check(v.tracking==1,"Keytrack target");
  if(d==28)check(std::abs(v.lfoPitch-.8f)<1e-6&&v.lfoFilter==0,"Independent LFO pitch depth");
  if(d==29)check(std::abs(v.lfoFilter-.8f)<1e-6&&v.lfoPitch==0,"Independent LFO filter depth");
  if(d==30||d==34)check(std::abs(v.egPitch[d==34]-.8f)<1e-6,"Envelope pitch depth");
  if(d==31||d==35)check(std::abs(v.egFilter[d==35]-40)<1e-5,"Envelope filter depth");
  if(d==32||d==36)check(std::abs(v.egVelocity[d==36]-.8f)<1e-6,"Envelope velocity");
  if(d==39)check(v.glideRate<1,"Glide target");
  if(d==40)check(std::abs(v.tune-std::exp2(1.f/12))<1e-6,"Master Tune exceeds legal +/-100 cents");
  e.allOff();advance(e,48000);check(PreUnisonProbe::faults(e)==0,"New destination numerical fault");
 }
 for(int steps:{1,2,3,5}){int choice=1;for(int i=0;i<10000;++i){float n=(.5f+(i%2?.04f:-.04f))/steps;choice=stableChoice(n,steps,choice);check(choice==1,"Discrete boundary chatter");}}
 std::cout<<"PASS all 23 added targets, independent LFO depths, legal tune range and hysteresis\n";
}
void resonance(const std::filesystem::path&folder){
 std::ofstream csv(folder/"resonance.csv");csv<<"sample_rate,engine,type,slope,note,resonance,peak,rms,pre_limiter_peak,pre_limiter_rms,filter_peak,clipped_samples,faults,frozen_output_error,frozen_filter_error\n";
 double greatest=0;int clippedCases=0;
 for(double sr:{44100.,48000.,96000.})for(int engine:{0,1})for(int type=0;type<4;++type)for(int slope:{0,1})for(int note:{36,60,84})for(double res:{0.,.5,.9,.99,1.}){
  Engine e;setup(e,sr);e.set(Engine1,engine);e.set(Engine2,engine);e.set(24,.4);e.set(7,0);e.set(21,.25);e.set(22,res);option(e,FilterType,type);e.set(28,slope);e.note(note,true);advance(e,2048);
  frozen_xs::Engine frozen;std::copy_n(e.state.begin(),frozen.state.size(),frozen.state.begin());frozen.reset(sr);frozen.note(note,true);for(int n=0;n<2048;++n)frozen.next();
  double peak=0,energy=0,rawPeak=0,rawEnergy=0,filterPeak=0,frozenError=0,filterError=0;int clips=0;
  for(int n=0;n<4096;++n){float x=e.next(),reference=frozen.next(),raw=PreUnisonProbe::raw(e);frozenError=std::max(frozenError,double(std::abs(std::clamp(x,-1.f,1.f)-reference)));
    auto current=PreUnisonProbe::filterState(e),previous=frozen_xs::DspValidationProbe::filterState(frozen);for(int k=0;k<4;++k)filterError=std::max(filterError,double(std::abs(current[k]-previous[k])));
    peak=std::max(peak,double(std::abs(x)));energy+=x*x;rawPeak=std::max(rawPeak,double(std::abs(raw)));rawEnergy+=raw*raw;filterPeak=std::max(filterPeak,double(PreUnisonProbe::filterPeak(e)));clips+=std::abs(raw)>1;}
  // All filter responses intentionally differ after approved calibrated TPT;
  // retain historical errors and unchanged frozen source without blessing new fixtures.
  check(std::isfinite(frozenError)&&std::isfinite(filterError),"Historical comparison nonfinite");
  check(std::isfinite(filterPeak)&&std::isfinite(rawPeak)&&PreUnisonProbe::faults(e)==0,"Resonance instability/nonfinite");greatest=std::max(greatest,rawPeak);clippedCases+=clips>0;
  csv<<sr<<','<<engine<<','<<type<<','<<slope<<','<<note<<','<<res<<','<<peak<<','<<std::sqrt(energy/4096)<<','<<rawPeak<<','<<std::sqrt(rawEnergy/4096)<<','<<filterPeak<<','<<clips<<','<<PreUnisonProbe::faults(e)<<','<<frozenError<<','<<filterError<<'\n';
 }
 std::cout<<"PASS resonance audit 720 cases; pre-limiter max="<<greatest<<"; cases exceeding unity="<<clippedCases<<"; numerical faults=0\n";
}
int main(int argc,char**argv){try{std::filesystem::path folder=argc>1?argv[1]:".";std::filesystem::create_directories(folder);migration();transitions();lfo();destinations();resonance(folder);
 Engine e;setup(e);option(e,Voices,5);route(e,18,1,.7);for(int n=36;n<68;++n)e.note(n,true);auto count=allocations.load();advance(e,8192);
 for(int n=0;n<8192;++n){if(n%17==0)option(e,VoiceMode,(n/17)%3);check(std::isfinite(e.next()),"32-voice mode transition nonfinite");}
 e.allOff();advance(e,48000);check(e.activeVoices()==0&&e.next()==0,"32-voice mode switching left stuck voices");
 check(allocations.load()==count,"Realtime allocation with 32 voices/new targets/mode transitions");
 std::cout<<"PASS fixed pool, 32 voices, no realtime heap; Engine bytes="<<sizeof(Engine)<<'\n';return 0;}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
