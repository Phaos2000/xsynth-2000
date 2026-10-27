// GPL-2.0-or-later. Offline paired benchmark; no diagnostic work in plugin DSP.
#define NOMINMAX
#include <windows.h>
#include <intrin.h>
#include <array>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <atomic>
#include <cstdlib>
#include <new>
std::atomic<size_t> allocations{0};
void* operator new(size_t n){++allocations;if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void* operator new[](size_t n){return ::operator new(n);}
void operator delete(void*p)noexcept{std::free(p);}void operator delete[](void*p)noexcept{std::free(p);}
void operator delete(void*p,size_t)noexcept{std::free(p);}void operator delete[](void*p,size_t)noexcept{std::free(p);}
namespace profiler {
struct Entry{const char*name=nullptr;unsigned long long ticks=0,calls=0;};
inline std::array<Entry,32> entries{};inline int count=0;
inline unsigned long long clock(){unsigned aux;return __rdtscp(&aux);}
struct Scope{
 inline static Scope*top=nullptr;Scope*parent=nullptr;int id=-1;unsigned long long start=0,children=0;
 Scope(const char*name){begin(name);}
 void begin(const char*name){id=0;while(id<count&&std::strcmp(entries[id].name,name))++id;if(id==count)entries[count++].name=name;parent=top;top=this;children=0;start=clock();}
 void stop(){if(id<0)return;auto elapsed=clock()-start;entries[id].ticks+=elapsed-children;++entries[id].calls;if(parent)parent->children+=elapsed;top=parent;id=-1;}
 void section(const char*name){stop();begin(name);}~Scope(){stop();}
};
}
#include "Core.h"
#define xs perf_before
#include "perf-before/Core.h"
#undef xs
#define xs production_before
#include "pre-wavetable/Core.h"
#undef xs
#define xs perf_profile
#include "perf-profile/Core.h"
#undef xs
namespace xs {struct DspValidationProbe {
 static std::array<double,32*48> nonFilterState(const Engine&e){std::array<double,32*48>a{};for(int i=0;i<32;++i){const auto&v=e.pool[i];if(!v.active)continue;int j=i*48;a[j++]=v.active;a[j++]=v.phase[0];a[j++]=v.phase[1];a[j++]=v.hz;a[j++]=v.eg[0].value;a[j++]=v.eg[1].value;for(int d=0;d<42;++d)a[j++]=v.mod[d];}return a;}
};}
namespace production_before {struct DspValidationProbe {
 static std::array<double,32*48> nonFilterState(const Engine&e){std::array<double,32*48>a{};for(int i=0;i<32;++i){const auto&v=e.pool[i];if(!v.active)continue;int j=i*48;a[j++]=v.active;a[j++]=v.phase[0];a[j++]=v.phase[1];a[j++]=v.hz;a[j++]=v.eg[0].value;a[j++]=v.eg[1].value;for(int d=0;d<42;++d)a[j++]=v.mod[d];}return a;}
};}
volatile double sink=0;
void require(bool ok,const char*message){if(!ok)throw std::runtime_error(message);}
struct Work {int rate=48000,block=128,voices=32,kind=2,clean=1,mix=0,xmod=0,type=0,slope=1;bool high=false;};
const char*workNames[]={"A-minimal","B-one-route","C-four-twelve","D-unison","E-unison-four-twelve"};
template<class E>void setup(E&e,Work w){
 e.reset(w.rate);e.set(xs::VoiceMode,1);e.set(xs::Voices,1);e.set(23,.15);e.set(24,.4);e.set(25,1);e.set(7,.5);
 e.set(xs::Engine1,w.clean);e.set(xs::Engine2,w.clean);e.set(xs::XMode,w.xmod/2.);e.set(xs::XDepth,w.xmod?.15:0);e.set(xs::MixerMode,w.mix/5.);e.set(xs::FilterType,w.type/3.);e.set(28,w.slope);e.set(xs::Keytrack,0);e.set(21,(w.high?20000.:3000.)/22000);e.set(22,0);
 int unison=w.kind>=3?std::min(8,w.voices):1;if(unison>1){int u=unison==2?1:unison==4?2:3;e.set(xs::Unison,u/3.);e.set(xs::Detune,.5);}
 int lf=w.kind==1?1:(w.kind==2||w.kind==4?4:0);
 for(int l=0;l<4;++l){e.set(xs::lfoParam(l,0),.17*(l+1));if(l<lf){e.set(xs::lfoParam(l,1),.01);e.set(xs::lfoParam(l,2),.02);}}
 const int destinations[]={0,8,1,41,3,4,7,9,30,34,31,35};
 for(int slot=0;slot<(lf==1?1:lf==4?12:0);++slot){e.set(xs::matrixId(slot),.1);if(slot%4)e.set(xs::SourceExtensionFirst+slot,(slot%4)/3.);int d=destinations[slot];if(slot<6){if(d==41)e.set(xs::DetuneRouteFirst+slot,1);else e.set(xs::ExpandedDestinationFirst+slot,(d+1)/41.);}else e.set(xs::matrixId(slot,1),d/41.);e.set(xs::matrixId(slot,2),.51);}
 for(int n=0;n<w.voices/unison;++n)e.note(36+n,true,.7f,n);
 for(int i=0;i<w.rate/20;++i)e.next();require(e.activeVoices()==w.voices,"Wrong physical workload");
}
template<class E>double render(E&e,Work w,int frames,int offset=0){double sum=0;for(int pos=0;pos<frames;){int end=std::min(frames,pos+w.block);e.transport(120,true,(pos+offset)*2./w.rate,true);for(;pos<end;++pos)sum+=e.next();}return sum;}
struct Time{double wall,cycles;};
template<class E>Time timed(Work w){E e;setup(e,w);auto before=allocations.load();int frames=w.rate/8;double scale=double(w.rate)/frames;unsigned long long c0=0,c1=0;QueryThreadCycleTime(GetCurrentThread(),&c0);auto t=std::chrono::steady_clock::now();double sum=render(e,w,frames);double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-t).count()*scale;QueryThreadCycleTime(GetCurrentThread(),&c1);require(allocations.load()==before,"Realtime allocation");sink=sum;return {ms,double(c1-c0)*scale};}
void bench(const std::filesystem::path&folder){
 std::ofstream csv(folder/"paired.csv"),raw(folder/"paired-raw.csv");csv<<"sr,block,voices,work,before_ms,after_ms,before_realtime_factor,after_realtime_factor,improvement_percent,before_thread_cycles,after_thread_cycles\n";raw<<"sr,block,voices,work,repeat,before_ms,after_ms,before_thread_cycles,after_thread_cycles\n";
 for(int sr:{44100,48000,88200,96000})for(int block:{32,64,128,256,512,1024})for(int voices:{1,8,16,32})for(int kind=0;kind<5;++kind){Work w;w.rate=sr;w.block=block;w.voices=voices;w.kind=kind;std::array<double,5>a{},b{},ca{},cb{};for(int k=0;k<5;++k){Time x,y;if(k&1){y=timed<xs::Engine>(w);x=timed<perf_before::Engine>(w);}else{x=timed<perf_before::Engine>(w);y=timed<xs::Engine>(w);}a[k]=x.wall;b[k]=y.wall;ca[k]=x.cycles;cb[k]=y.cycles;raw<<sr<<','<<block<<','<<voices<<','<<workNames[kind]<<','<<k<<','<<x.wall<<','<<y.wall<<','<<x.cycles<<','<<y.cycles<<'\n';}for(auto*p:{&a,&b,&ca,&cb})std::sort(p->begin(),p->end());csv<<sr<<','<<block<<','<<voices<<','<<workNames[kind]<<','<<a[2]<<','<<b[2]<<','<<1000/a[2]<<','<<1000/b[2]<<','<<(1-b[2]/a[2])*100<<','<<ca[2]<<','<<cb[2]<<'\n';csv.flush();}
}
void profile(const std::filesystem::path&folder){
 std::ofstream csv(folder/"profile.csv");csv<<"case,zone,exclusive_ticks,calls,share_percent\n";
 for(int clean:{0,1})for(int kind:{0,2,3,4})for(bool high:{false,true}){Work w;w.clean=clean;w.kind=kind;w.high=high;w.xmod=kind==2?2:0;w.mix=kind==3?2:0;perf_profile::Engine e;setup(e,w);profiler::entries={};profiler::count=0;sink=render(e,w,12000);unsigned long long total=0;for(int i=0;i<profiler::count;++i)total+=profiler::entries[i].ticks;for(int i=0;i<profiler::count;++i){auto&x=profiler::entries[i];csv<<(clean?"CLEAN":"CLASSIC")<<'-'<<workNames[kind]<<'-'<<(high?"high":"low")<<','<<x.name<<','<<x.ticks<<','<<x.calls<<','<<100.*x.ticks/total<<'\n';}}
 // Event and state bookkeeping outside the sample renderer, measured separately.
 perf_profile::Engine e;Work w;setup(e,w);profiler::entries={};profiler::count=0;for(int n=0;n<1000;++n){profiler::Scope scope("note_allocation_ownership");e.note(36+n%48,true,.7f,n);e.note(36+n%48,false,0,n);}for(int i=0;i<profiler::count;++i){auto&x=profiler::entries[i];csv<<"events,"<<x.name<<','<<x.ticks<<','<<x.calls<<",0\n";}
}
void compare(const std::filesystem::path&folder){
 std::ofstream csv(folder/"sound-comparison.csv");csv<<"sr,block,voices,work,clean,mixer,xmod,type,slope,high,max_error,rms_error\n";double worst=0,intentionalUnisonWorst=0;int cases=0,staticPolicyCases=0;
 for(int sr:{44100,48000,88200,96000})for(int kind=0;kind<5;++kind)for(int clean:{0,1})for(int mix=0;mix<6;++mix)for(int xmod:{0,1,2}){
 Work w;w.rate=sr;w.kind=kind;w.clean=clean;w.mix=mix;w.xmod=xmod;w.voices=kind>=3?std::array<int,3>{2,4,8}[(mix+xmod)%3]:kind==0?1:8;w.type=(mix+xmod)%4;w.slope=(mix+xmod)%2;w.high=(mix+xmod)%2;w.block=std::array<int,6>{32,64,128,256,512,1024}[(mix+xmod)%6];xs::Engine a;production_before::Engine b;setup(a,w);setup(b,w);double maximum=0,power=0;auto count=allocations.load();for(int n=0;n<8192;++n){if(n%w.block==0){a.transport(120,true,n*2./sr,true);b.transport(120,true,n*2./sr,true);}if(n==2000){a.set(xs::XMode,(2-xmod)/2.);b.set(xs::XMode,(2-xmod)/2.);}if(n==3001){a.set(xs::MixerMode,((mix+1)%6)/5.);b.set(xs::MixerMode,((mix+1)%6)/5.);}float x=a.next(),y=b.next();if(kind<3&&n%8==0)require(xs::DspValidationProbe::nonFilterState(a)==production_before::DspValidationProbe::nonFilterState(b),"Nonfilter trajectory changed");require(std::isfinite(x)&&std::isfinite(y),"Nonfinite comparison");double error=std::abs(double(std::clamp(x,-1.f,1.f))-y);maximum=std::max(maximum,error);power+=error*error;}require(count==allocations.load(),"Comparison realtime allocation");if(kind<3)require(xs::DspValidationProbe::nonFilterState(a)==production_before::DspValidationProbe::nonFilterState(b),"Nonfilter oscillator/envelope/route state changed");if(kind>=3){intentionalUnisonWorst=std::max(intentionalUnisonWorst,maximum);++staticPolicyCases;}else worst=std::max(worst,maximum);csv<<sr<<','<<w.block<<','<<w.voices<<','<<workNames[kind]<<','<<clean<<','<<mix<<','<<xmod<<','<<w.type<<','<<w.slope<<','<<w.high<<','<<maximum<<','<<std::sqrt(power/8192)<<'\n';++cases;}
 std::cout<<"SOUND historical-clamp comparison cases="<<cases<<" maximum_error="<<worst<<" Engine_before="<<sizeof(production_before::Engine)<<" Engine_after="<<sizeof(xs::Engine)<<" no realtime allocation; intentional static-Unison cases="<<staticPolicyCases<<" maximum difference="<<intentionalUnisonWorst<<"\n";require(std::isfinite(worst),"Historical filter comparison nonfinite");std::cout<<"APPROVED calibrated-TPT output differences; nonfilter oscillator/envelope/route state remains exact\n";
 // Exercise continuous and stepped route removal, zero amounts and retargeting
 // against the pre-pass engine, including old source-extension precedence.
 double tailWorst=0;int tailCases=0;
 for(int rate:{44100,48000,88200,96000})for(int source=0;source<14;++source)for(int dest=0;dest<42;++dest){
  xs::Engine a;production_before::Engine b;Work w;w.rate=rate;w.voices=1;w.kind=0;setup(a,w);setup(b,w);int slot=(source+dest)%12;
  auto edit=[&](int id,double value){a.set(id,value);b.set(id,value);};
  edit(31,.4);edit(xs::Pressure,.6);edit(30,.7);
  edit(xs::matrixId(slot),std::min(10,source)/10.);if(source>=11)edit(xs::SourceExtensionFirst+slot,(source-10)/3.);
  auto destination=[&](int d){if(slot<6){edit(xs::DetuneRouteFirst+slot,d==41?1:0);edit(xs::ExpandedDestinationFirst+slot,(std::min(40,d)+1)/41.);}else edit(xs::matrixId(slot,1),d/41.);};
  destination(dest);edit(xs::matrixId(slot,2),.6);auto alloc=allocations.load();
  for(int n=0;n<256;++n){if(n==97)edit(xs::matrixId(slot),0);if(n==120){destination((dest+7)%42);edit(xs::matrixId(slot),1);edit(xs::matrixId(slot,2),.4);}if(n==145)edit(xs::matrixId(slot,2),.5);if(n==180){edit(xs::SourceExtensionFirst+slot,1);edit(xs::matrixId(slot),0);}float x=a.next(),y=b.next();require(std::isfinite(x)&&std::isfinite(y),"Route tail nonfinite");tailWorst=std::max(tailWorst,std::abs(double(x)-y));}
  require(alloc==allocations.load(),"Route-tail realtime allocation");require(xs::DspValidationProbe::nonFilterState(a)==production_before::DspValidationProbe::nonFilterState(b),"Route removal/retargeting nonfilter state changed");++tailCases;
 }
 std::cout<<"ROUTE TRANSITIONS cases="<<tailCases<<" maximum_error="<<tailWorst<<'\n';require(std::isfinite(tailWorst),"Historical route output comparison nonfinite");
 double isolatedWorst=0;int isolatedCases=0;
 for(int rate:{44100,48000,88200,96000})for(int clean:{0,1})for(int oscillator=0;oscillator<2;++oscillator)for(int wave=0;wave<6;++wave){
  Work w;w.rate=rate;w.voices=1;w.kind=2;w.clean=clean;xs::Engine a;production_before::Engine b;setup(a,w);setup(b,w);
  for(int id:{24,25}){a.set(id,wave/5.);b.set(id,wave/5.);}a.set(7,oscillator);b.set(7,oscillator);
  for(int n=0;n<8192;++n)isolatedWorst=std::max(isolatedWorst,std::abs(double(a.next())-b.next()));require(xs::DspValidationProbe::nonFilterState(a)==production_before::DspValidationProbe::nonFilterState(b),"Isolated oscillator trajectory changed");++isolatedCases;
 }
 std::cout<<"ISOLATED OSCILLATORS cases="<<isolatedCases<<" maximum_error="<<isolatedWorst<<'\n';require(std::isfinite(isolatedWorst),"Historical oscillator-through-filter comparison nonfinite");
}
int main(int argc,char**argv){_mm_setcsr(_mm_getcsr()|0x8040);try{require(argc==3,"mode output-folder");std::filesystem::path folder=argv[2];std::filesystem::create_directories(folder);DWORD_PTR processMask=0,systemMask=0;if(GetProcessAffinityMask(GetCurrentProcess(),&processMask,&systemMask)&&processMask)SetThreadAffinityMask(GetCurrentThread(),processMask&(~processMask+1));std::string mode=argv[1];if(mode=="profile")profile(folder);else if(mode=="bench")bench(folder);else compare(folder);return 0;}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
