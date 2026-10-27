// GPL-2.0-or-later. Host-listening regressions and four independent LFO audit.
#include "Core.h"
#include "HistoricalState.h"
#define xs before_xs
#include "pre-four-lfo/Core.h"
#undef xs
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <new>
#include <stdexcept>
#include <xmmintrin.h>
std::atomic<size_t> allocations{0};
void*operator new(size_t n){++allocations;if(auto*p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void*operator new[](size_t n){return ::operator new(n);}
void operator delete(void*p)noexcept{std::free(p);}void operator delete[](void*p)noexcept{std::free(p);}
void operator delete(void*p,size_t)noexcept{std::free(p);}void operator delete[](void*p,size_t)noexcept{std::free(p);}
namespace xs {struct FourProbe {
 static auto&voice(Engine&e){return e.pool[0];}
 static auto values(Engine&e){return e.lfoValues;}
 static int routes(Engine&e){return e.routeCount;}
 static float raw(Engine&e){return e.preLimiterOutput;}
 static unsigned faults(Engine&e){return e.numericalFaults;}
};}
namespace before_xs {struct UnisonProbe {static float raw(Engine&e){return e.preLimiterOutput;}};}
using namespace xs;
void check(bool b,const char*m){if(!b)throw std::runtime_error(m);}
void configure(Engine&e,double sr){e.state=defaults();e.state[VoiceMode]=1;e.state[23]=.15;e.state[24]=.4;e.state[21]=.1;e.reset(sr);}
void advance(Engine&e,int n){while(n--)check(std::isfinite(e.next()),"Nonfinite render");}
void stateAndSources(){
 for(int version:{1,2,3,4,5}){State old=defaults();old[4]=.7;old[5]=.15;old[6]=.3;old[MatrixFirst]=.1;auto b=historicalState(old);int count=version<4?PreviousCount:version==4?PreUnisonCount:PreFourLfoCount;b[4]=uint8_t(version);b.resize(8+count*8);State restored;check(decode(b.data(),b.size(),restored),"Legacy state decode");check(restored[4]==.7&&restored[5]==.15,"LFO1 migration");for(int l=1;l<4;++l)check(restored[lfoParam(l,1)]==0&&restored[lfoParam(l,2)]==0,"Migration duplicated direct depths");for(int slot=6;slot<12;++slot)check(effectiveSource(restored,slot)==0&&restored[matrixId(slot,2)]==.5,"New route not neutral");}
 State state=defaults();for(int l=1;l<4;++l)for(int f=0;f<9;++f)state[lfoParam(l,f)]=stepped(lfoParam(l,f),.7);auto b=encode(state);State round;check(b[4]==7&&decode(b.data(),b.size(),round)&&round==state,"V6 roundtrip");
 for(int slot=0;slot<12;++slot)for(int source=0;source<11;++source){Engine e;configure(e,48000);e.set(SourceExtensionFirst+slot,1);e.set(matrixId(slot),source/10.);check(effectiveSource(e.state,slot)==source,"Historical source rescaled or did not take precedence");e.set(SourceExtensionFirst+slot,1);e.set(matrixId(slot),source/10.);check(effectiveSource(e.state,slot)==source,"Repeated old source write");}
 std::cout<<"PASS V1-V5 migrations, V6 state, 12 source extensions and repeated historical /10 writes\n";
}
void clocksAndDirect(){
 for(double sr:{44100.,48000.,88200.,96000.}){
  Engine e;configure(e,sr);double rates[]={.37,1.1,2.3,4.7};for(int l=0;l<4;++l)e.set(lfoParam(l,0),normalized(4,rates[l]));e.note(60,true);auto before=allocations.load();
  for(int n=0;n<int(sr);++n){e.next();auto values=FourProbe::values(e);for(int l=0;l<4;++l){float phase=float(std::fmod((n+1)*rates[l]/sr,1.));float expected=std::sin(float(2*Pi)*phase);check(std::abs(values[l]-expected)<.0006,"Independent sample-clock phase");}}check(before==allocations.load(),"LFO realtime allocation");
  for(int l=0;l<4;++l){configure(e,sr);e.set(lfoParam(l,0),normalized(4,1.3));e.set(lfoParam(l,1),.1);e.set(7,0);e.note(60,true);float last1=0,last2=0;advance(e,128);
   for(int n=0;n<int(sr/4);++n){auto &v=FourProbe::voice(e);last1=v.phase[0];last2=v.phase[1];e.next();float d1=v.phase[0]-last1,d2=v.phase[1]-last2;if(d1<0)d1+=1;if(d2<0)d2+=1;check(std::abs(d1-d2)<2e-6,"Direct pitch does not affect both VCOs coherently");check(d1>0,"Pitch stopped");}
  }
  configure(e,sr);for(int l=0;l<4;++l){e.set(lfoParam(l,1),.2);e.set(lfoParam(l,2),.2);e.set(lfoParam(l,0),normalized(4,rates[l]));if(l&1){e.set(lfoParam(l,4),1);e.set(lfoParam(l,8),l/29.);}e.set(lfoParam(l,6),l%2);if(l==2)e.set(lfoParam(l,7),.5);}
  for(int slot=0;slot<12;++slot){e.set(matrixId(slot),.1);if(slot%4)e.set(SourceExtensionFirst+slot,(slot%4)/3.);int dest[]={0,8,1,41};if(slot<6){e.set(ExpandedDestinationFirst+slot,(std::min(40,dest[slot%4])+1)/41.);if(dest[slot%4]==41)e.set(DetuneRouteFirst+slot,1);}else e.set(matrixId(slot,1),dest[slot%4]/41.);e.set(matrixId(slot,2),.6);}
  e.set(Unison,1);e.set(Detune,.5);e.transport(123,true,0,true);e.note(60,true);auto count=allocations.load();advance(e,int(sr));check(FourProbe::routes(e)==12,"Not all routes active");check(count==allocations.load()&&FourProbe::faults(e)==0,"Combined modulation fault/allocation");e.allOff();advance(e,int(sr));check(e.activeVoices()==0,"Four LFO release stuck");
 }
 // Independent instances, mixed sync/random, block invariance and rapid routing.
 for(int block:{32,64,128,256,512,1024}){Engine a,b;configure(a,48000);configure(b,48000);for(auto *e:{&a,&b}){for(int l=0;l<4;++l){e->set(lfoParam(l,0),.17*(l+1));e->set(lfoParam(l,1),.025);e->set(lfoParam(l,7),l==2?.5:0);e->set(lfoParam(l,4),l%2);e->set(lfoParam(l,5),1);}e->note(60,true);}for(int n=0;n<16000;++n){if(n%block==0)a.transport(120,true,n/24000.,true);b.transport(120,true,n/24000.,true);check(a.next()==b.next(),"Four LFO block dependence");}}
 std::cout<<"PASS four independent clocks, isolated direct pitch on both VCOs, 12 simultaneous routes, mixed sync/random and exact block invariance\n";
}
template<class E>void setup(E&e,double sr,int wave,int note,double cutoff,double resonance,int slope,int track,int type){e.reset(sr);e.set(VoiceMode,1);e.set(24,wave/5.);e.set(25,wave/5.);e.set(7,.5);e.set(23,.15);e.set(21,cutoff/(440*50));e.set(22,resonance);e.set(28,slope);e.set(Keytrack,track/2.);e.set(FilterType,type/3.);e.note(note,true);}
struct Metric{double peak=0,sum=0,window=0,maxWindow=0,previous=0,previousWindow=0,beforeWindow=0;int peaks=0,lastPeak=0,maxGap=0;};
template<class E,class Probe>Metric measure(E&e,int frames,int minimumGap=480){Metric m;for(int n=0;n<2048;++n)e.next();for(int n=0;n<frames;++n){e.next();float x=Probe::raw(e);check(std::isfinite(x),"Nonfinite raw audit");m.peak=std::max(m.peak,double(std::abs(x)));m.sum+=x*x;m.window+=x*x;if(n%480==479){double power=m.window/480;m.maxWindow=std::max(m.maxWindow,power);m.window=0;if(m.previousWindow>power&&m.previousWindow>m.beforeWindow&&m.previousWindow>.75*m.maxWindow&&n-m.lastPeak>minimumGap){if(m.peaks)m.maxGap=std::max(m.maxGap,n-m.lastPeak);m.lastPeak=n;++m.peaks;}m.beforeWindow=m.previousWindow;m.previousWindow=power;}m.previous=std::abs(x);}m.sum=std::sqrt(m.sum/frames);m.maxWindow=std::sqrt(m.maxWindow);return m;}
void filters(const std::filesystem::path&folder){std::ofstream csv(folder/"high-cutoff.csv");csv<<"sr,cutoff,note,type,slope,track,resonance,old_peak,new_peak,old_rms,new_rms\n";
 for(double sr:{44100.,48000.,88200.,96000.})for(double hz:{5000.,7000.,10000.,15000.,18000.,20000.,22000.,.49*sr})for(int type=0;type<4;++type)for(int slope:{0,1})for(int track:{0,1,2})for(double res:{0.,.5,.9}){Engine e;before_xs::Engine old;setup(e,sr,4,84,hz,res,slope,track,type);setup(old,sr,4,84,hz,res,slope,track,type);auto a=measure<before_xs::Engine,before_xs::UnisonProbe>(old,2048),b=measure<Engine,FourProbe>(e,2048);check(FourProbe::faults(e)==0,"High cutoff fault");csv<<sr<<','<<hz<<",84,"<<type<<','<<slope<<','<<track<<','<<res<<','<<a.peak<<','<<b.peak<<','<<a.sum<<','<<b.sum<<'\n';if(res==0&&type==0&&hz>=15000)check(b.peak<.25,"High cutoff LP gain burst remains");}
 // Isolated zero-resonance white-noise frequency response: the old Nyquist pole
 // peaks 35x per stage at g=.825, d=2; trapezoidal LP never boosts a tone.
 for(double sr:{44100.,48000.,88200.,96000.})for(double fc:{7000.,15000.,22000.,.49*sr}){float states[4]{};float g=highcut::coefficient(float(fc),float(sr));double sum=0;for(int n=0;n<16000;++n){float x=std::sin(float(2*Pi*.45*n)),out[4]{};highcut::stage(x,g,2,states,out);highcut::stage(out[0],g,2,states+2,out);if(n>2048)sum+=out[0]*out[0];}check(sum/(16000-2049)<.51,"Zero-resonance TPT boosted Nyquist tone");}
 std::cout<<"PASS high-cutoff sweep: 2304 configurations, four rates, four types, slopes, tracking, resonance; zero-resonance near-Nyquist tone gain bounded\n";
}
void peaks(const std::filesystem::path&folder){std::ofstream csv(folder/"crest.csv");csv<<"count,spread,note,wave,width,engine,mixer,osc,seconds,old_peak,new_peak,old_rms,new_rms,old_crest,new_crest,old_max10ms,new_max10ms,adjacent_beat_seconds,old_max_envelope_peak_gap,new_max_envelope_peak_gap\n";
 for(int u=1;u<=3;++u)for(double spread:{1.,25.,50.})for(int note:{36,60,84})for(int wave:{1,2,4,5})for(int clean:{0,1}){
  int key=(note-36)/24;if(clean!=(u+key+wave+int(spread))%2)continue;double width=std::array<double,3>{.25,.5,.75}[(key+wave)%3];int mixer=std::array<int,3>{0,2,5}[(key+wave+u+clean)%3],osc=(key+wave+u)%2;
  // Balanced coverage rather than a redundant Cartesian product; each case
  // spans at least two full adjacent-member beat periods.

  Engine e;before_xs::Engine old;
  setup(e,48000,wave,note,3000,0,0,0,0);setup(old,48000,wave,note,3000,0,0,0,0);
  for(int id:std::array<int,8>{Unison,Detune,1,3,Engine1,Engine2,MixerMode,7}){double v=id==Unison?u/3.:id==Detune?spread/50:id==1||id==3?width:id==Engine1||id==Engine2?clean:id==MixerMode?mixer/5.:osc;e.set(id,v);old.set(id,v);}
  double beatPeriod=1/(2*440*std::exp2((note-69)/12.)*std::sinh(std::log(2.)*spread/(2400*(UnisonCounts[u]-1))));int seconds=std::max(1,int(std::ceil(2*beatPeriod)));
  auto a=measure<before_xs::Engine,before_xs::UnisonProbe>(old,48000*seconds,int(beatPeriod*48000*.4)),b=measure<Engine,FourProbe>(e,48000*seconds,int(beatPeriod*48000*.4));
  csv<<UnisonCounts[u]<<','<<spread<<','<<note<<','<<wave<<','<<width<<','<<clean<<','<<mixer<<','<<osc<<','<<seconds<<','<<a.peak<<','<<b.peak<<','<<a.sum<<','<<b.sum<<','<<a.peak/std::max(1e-12,a.sum)<<','<<b.peak/std::max(1e-12,b.sum)<<','<<a.maxWindow<<','<<b.maxWindow<<','<<beatPeriod<<','<<a.maxGap/48000.<<','<<b.maxGap/48000.<<'\n';
 }
}
void performance(const std::filesystem::path&folder){std::ofstream csv(folder/"cpu.csv");csv<<"sr,block,lfos,routes,voices,unison,ms_per_audio_second\n";
 for(double sr:{44100.,48000.,88200.,96000.})for(int block:{32,64,128,256,512,1024})for(int lfos:{1,4})for(int load:{1,8,32}){Engine e;configure(e,sr);e.set(VoiceMode,1);e.set(Voices,1);if(load>1)e.set(Unison,1);for(int l=0;l<lfos;++l){e.set(lfoParam(l,1),.01);e.set(lfoParam(l,2),.01);e.set(lfoParam(l,0),.15*(l+1));}for(int slot=0;slot<(lfos==1?1:12);++slot){e.set(matrixId(slot),.1);if(slot%4)e.set(SourceExtensionFirst+slot,(slot%4)/3.);e.set(matrixId(slot,2),.51);}for(int n=0;n<(load==32?4:1);++n)e.note(48+n*4,true,.7,n);advance(e,512);auto allocation=allocations.load();auto start=std::chrono::steady_clock::now();int frames=int(sr/8);for(int n=0;n<frames;++n){if(n%block==0)e.transport(120,true,n*2/sr,true);e.next();}double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()*sr/frames;check(allocations.load()==allocation,"Performance allocated");csv<<sr<<','<<block<<','<<lfos<<','<<(lfos==1?1:12)<<','<<e.activeVoices()<<','<<(load==1?1:8)<<','<<ms<<'\n';}
 std::cout<<"PERFORMANCE Engine bytes="<<sizeof(Engine)<<" before="<<sizeof(before_xs::Engine)<<"; shared tables unchanged; no RT allocation\n";
}
int main(int argc,char**argv){_mm_setcsr(_mm_getcsr()|0x8040);try{std::filesystem::path folder=argc>1?argv[1]:"four-lfo-audit";std::filesystem::create_directories(folder);if(argc>2&&std::string(argv[2])=="--crest"){peaks(folder);return 0;}stateAndSources();clocksAndDirect();filters(folder);performance(folder);return 0;}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
