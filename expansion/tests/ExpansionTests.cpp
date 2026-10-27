// GPL-2.0-or-later. Behavioral expansion/compatibility and realtime allocation tests.
#include "Core.h"
#include "HistoricalState.h"
#include "StateExchange.h"
#include <atomic>
#include <cstdlib>
#include <new>
#include <thread>
#include <iostream>
#include <stdexcept>
#include <chrono>
#include <memory>
std::atomic<size_t> allocations{0};
void *operator new(size_t n) { ++allocations; if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc(); }
void operator delete(void *p) noexcept {std::free(p);}
void *operator new[](size_t n) { return ::operator new(n); }
void operator delete[](void *p) noexcept {::operator delete(p);}
void operator delete(void *p,size_t) noexcept {std::free(p);}
void operator delete[](void *p,size_t) noexcept {std::free(p);}
void check(bool value,const char *message) { if(!value)throw std::runtime_error(message); }
void option(xs::Engine &e,int id,int value) {if(xs::legacyDestination(id)){if(value==xs::EstablishedDestinationCount)e.set(xs::DetuneRouteFirst+(id-xs::MatrixFirst)/3,1.);else e.set(xs::ExpandedDestinationFirst+(id-xs::MatrixFirst)/3,(value+1)/double(xs::EstablishedDestinationCount));}else e.set(id,double(value)/xs::stepsFor(id));}
double render(xs::Engine &e,int n) {double energy=0;for(int i=0;i<n;++i){float v=e.next();check(std::isfinite(v),"Nonfinite expansion output");energy+=v*v;}return energy;}
double alias(bool clean) {
 xs::Engine e;e.reset(48000);option(e,xs::FilterType,1);e.set(21,.01);e.set(24,.4);e.set(7,0);e.set(0,xs::normalized(0,7500/(440*std::exp2(27./12))));e.set(xs::Engine1,clean?1:0);
 e.note(96,true);render(e,2048);double re=0,im=0;
 for(int i=0;i<8192;++i){double v=e.next(),phase=2*xs::Pi*18000*i/48000;re+=v*std::cos(phase);im+=v*std::sin(phase);}
 return std::sqrt(re*re+im*im)/8192;
}
int main() {
 try {
  static_assert(sizeof(xs::Engine)<768*1024,"128 primary,128 topology-transition and16 steal members must remain below768KiB");
  auto original=xs::defaults();auto legacy=original;legacy[0]=legacy[2]=0;auto bytes=historicalState(legacy);bytes.resize(8+xs::LegacyCount*8);bytes[4]=1;bytes[2]='V';bytes[3]='3';xs::State old{};
  check(xs::decode(bytes.data(),bytes.size(),old)&&old==original,"Version-1 state migration failed");
  auto mixed=original;
  for(int i=34;i<xs::PreWavetableCount;++i)mixed[i]=xs::stepped(i,.723);
  bytes=xs::encode(mixed);xs::State restored{};
  check(xs::decode(bytes.data(),bytes.size(),restored)&&restored==mixed,"Expansion state recall failed");
  bytes[4]=99;check(!xs::decode(bytes.data(),bytes.size(),restored),"Unknown state version accepted");
  for(int voices=0;voices<6;++voices) {
    xs::Engine e;e.reset(48000);option(e,xs::VoiceMode,2);option(e,xs::Voices,voices);
    for(int n=36;n<76;++n)e.note(n,true,.7f,n);
    check(e.activeVoices()==std::min(40,xs::VoiceCounts[voices]),"Fixed voice-pool cap failed");
    auto before=allocations.load();check(render(e,2048)>0,"Polyphonic silence");
    for(int n=36;n<76;++n)e.note(n,false,0,n);
    render(e,20000);check(e.activeVoices()==0,"Voice release did not free fixed slots");
    check(before==allocations.load(),"Realtime DSP allocated memory");
  }
  for(int mode=0;mode<3;++mode)for(int direction=0;direction<2;++direction)
   for(int mixer=0;mixer<6;++mixer)for(int filter=0;filter<4;++filter) {
    xs::Engine e;e.reset(48000);option(e,xs::Engine1,1);option(e,xs::Engine2,1);
    option(e,xs::XMode,mode);option(e,xs::XDirection,direction);option(e,xs::MixerMode,mixer);option(e,xs::FilterType,filter);
    e.set(xs::XDepth,.8);e.set(27,1);e.set(28,1);e.set(24,.4);e.set(25,.8);
    e.note(72,true,.8);render(e,2048);
   }
  auto clockInstance=std::make_unique<xs::Engine>();auto&clock=*clockInstance;clock.reset(48000);clock.set(xs::LfoClock,1);option(clock,xs::LfoPhase,2);
  clock.transport(120,true,1.25,true,false);clock.note(60,true);clock.next();check(std::abs(clock.modulationPhase()-.25)<1.e-5,"HOST phase not locked to song beat");
  clock.transport(0,false,0,false,false);auto phase=clock.modulationPhase();render(clock,100);check(clock.modulationPhase()==phase,"Sync invented an internal tempo");
  option(clock,xs::LfoPhase,1);clock.transport(120,true,0,true);render(clock,800);clock.note(72,true);clock.next();check(clock.modulationPhase()<.001,"KEY did not reset LFO phase");
  for(int curve=0;curve<3;++curve)for(int loop=0;loop<3;++loop) {
    xs::Engine e;e.reset(44100);option(e,xs::Curve1,curve);option(e,xs::Curve2,curve);option(e,xs::Loop2,loop);e.set(xs::Velocity1,1);e.set(xs::Velocity2,1);e.note(60,true,.2);check(render(e,10000)>0,"Curve/loop silence");e.allOff();render(e,20000);check(e.next()==0,"Loop did not release");
  }
  for(int destination=0;destination<xs::DestinationCount;++destination) {
    xs::Engine e;e.reset(48000);option(e,xs::Engine1,1);
    option(e,xs::MatrixFirst,10);option(e,xs::MatrixFirst+1,destination);e.set(xs::MatrixFirst+2,.8);
    e.note(60,true,.6);check(render(e,4096)>0,"Matrix destination silence");
  }
  {
    auto softInstance=std::make_unique<xs::Engine>(),loudInstance=std::make_unique<xs::Engine>();auto&soft=*softInstance;auto&loud=*loudInstance;soft.reset(48000);loud.reset(48000);
    soft.set(xs::Velocity1,1);loud.set(xs::Velocity1,1);soft.note(60,true,.25);loud.note(60,true,1);
    double low=render(soft,4096),high=render(loud,4096);
    check(low<high*.15 && low>0,"Velocity did not scale EG1 amplitude");
    auto routesInstance=std::make_unique<xs::Engine>(),unroutedInstance=std::make_unique<xs::Engine>();auto&routes=*routesInstance;auto&unrouted=*unroutedInstance;routes.reset(48000);unrouted.reset(48000);
    routes.set(xs::Engine1,1);unrouted.set(xs::Engine1,1);
    option(routes,xs::MatrixFirst,10);option(routes,xs::MatrixFirst+1,0);routes.set(xs::MatrixFirst+2,.75);
    routes.note(60,true);unrouted.note(60,true);double difference=0;
    for(int i=0;i<4096;++i)difference+=std::abs(routes.next()-unrouted.next());
    check(difference>10,"Constant-to-pitch matrix route did not affect audio");
    auto legatoInstance=std::make_unique<xs::Engine>(),retriggerInstance=std::make_unique<xs::Engine>();auto&legato=*legatoInstance;auto&retrigger=*retriggerInstance;legato.reset(48000);retrigger.reset(48000);
    option(legato,xs::VoiceMode,1);retrigger.set(xs::Engine1,1);
    legato.set(9,.5);retrigger.set(9,.5);legato.set(11,.3);retrigger.set(11,.3);legato.note(60,true);retrigger.note(60,true);render(legato,10000);render(retrigger,10000);
    legato.note(72,true);retrigger.note(72,true);
    check(render(legato,1000)!=render(retrigger,1000),"Mono legato/retrigger not distinguished");
  }
  auto classic=alias(false),clean=alias(true);
  check(clean<classic*.5,"CLEAN saw failed to reduce folded 4th harmonic");
  std::cout<<"Saw folded harmonic amplitude CLASSIC="<<classic<<" CLEAN="<<clean<<'\n';
  xs::StateExchange exchange;std::atomic<bool> done{false};std::atomic<bool> coherent{true};
  std::thread audio([&]{xs::StateMessage s;while(!done.load()){if(exchange.audioRestore(s)){for(int i=0;i<xs::Count;++i)if(s.value[i]!=s.value[0])coherent=false;exchange.audioPublish(s.value,s.generation);}}});
  for(int k=1;k<=10000;++k){xs::State s;s.fill(k/10000.);exchange.restore(s);auto snapshot=exchange.snapshot();for(auto v:snapshot)check(v==snapshot[0],"UI snapshot torn");}
  done=true;audio.join();check(coherent,"Audio state handoff torn");
  auto loadInstance=std::make_unique<xs::Engine>();auto&e=*loadInstance; e.reset(48000);option(e,xs::VoiceMode,2);option(e,xs::Voices,5);option(e,xs::Engine1,1);option(e,xs::Engine2,1);
  for(int n=36;n<68;++n)e.note(n,true);
  auto start=std::chrono::steady_clock::now();render(e,48000);
  auto ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
  std::cout<<"Engine bytes="<<sizeof(xs::Engine)<<"; 32 CLEAN voices CPU ms/audio second="<<ms<<'\n';
  std::cout<<"PASS: migration, fixed pool, no DSP allocation, XMOD/mixer/filter combinations, tempo, envelopes, matrix, aliasing, coherent state exchange\n";
  return 0;
 }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}
}


