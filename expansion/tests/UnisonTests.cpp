// GPL-2.0-or-later. Fixed-pool unison correctness and measured DSP audits.
#include "Core.h"
#include "ProductIdentity.h"
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
void* operator new(size_t n){++allocations;if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void* operator new[](size_t n){return ::operator new(n);}
void operator delete(void*p)noexcept{std::free(p);}void operator delete[](void*p)noexcept{std::free(p);}
void operator delete(void*p,size_t)noexcept{std::free(p);}void operator delete[](void*p,size_t)noexcept{std::free(p);}
namespace xs {struct UnisonProbe {
 static auto& voices(Engine&e){return e.pool;}
 static auto& groups(Engine&e){return e.groups;}
 static int size(Engine&e){return e.groupSize;}
 static unsigned faults(Engine&e){return e.numericalFaults;}
 static float raw(Engine&e){return e.preLimiterOutput;}
 static float filterPeak(Engine&e){float x=0;for(auto&v:e.pool)for(float f:v.filter)x=std::max(x,std::abs(f));return x;}
};}
using namespace xs;
void check(bool b,const char*m){if(!b)throw std::runtime_error(m);}
void option(Engine&e,int id,int k){e.set(id,k/double(stepsFor(id)));}
void setup(Engine&e,int limit,int unison,int mode=2,int clean=0,double sr=48000){
 e.state=defaults();e.state[VoiceMode]=mode/2.;e.state[Voices]=limit/5.;e.state[Unison]=unison/3.;e.state[Engine1]=e.state[Engine2]=clean;e.state[23]=.15;e.state[21]=.1;
 e.reset(sr);
}
void run(Engine&e,int n){for(int i=0;i<n;++i)check(std::isfinite(e.next()),"Nonfinite unison output");}
void invariants(Engine&e,int limit){
 auto &v=UnisonProbe::voices(e);auto &groups=UnisonProbe::groups(e);int size=UnisonProbe::size(e);
 check(e.activeVoices()<=limit,"Selected physical voice limit exceeded");
 for(int i=0;i<32;++i)if(v[i].active&&v[i].group>=0){int g=v[i].group;check(g>=0&&g<32&&groups[g].active,"Orphan physical member");
  check(i==g*size+v[i].member&&v[i].members==size,"Group/member ownership");
  for(int m=0;m<size;++m){auto &other=v[g*size+m];check(other.active&&other.age==v[i].age&&other.noteId==v[i].noteId&&other.note==v[i].note,"Partial group or ownership mismatch");}
 }
 check(!UnisonProbe::faults(e),"Numerical fault");
}
void groups(){
 for(int limit=0;limit<6;++limit)for(int unison=0;unison<4;++unison)for(int mode=0;mode<3;++mode){
  Engine e;setup(e,limit,unison,mode);int n=std::min(VoiceCounts[limit],UnisonCounts[unison]);
  auto before=allocations.load();
  for(int k=0;k<45;++k){e.note(36+k%24,true,.7,100+k);run(e,19);invariants(e,VoiceCounts[limit]);check(UnisonProbe::size(e)==n,"Effective unison rule");}
  for(int k=0;k<45;++k){e.note(36+k%24,false,0,100+k);run(e,7);invariants(e,VoiceCounts[limit]);}
  e.allOff();run(e,96000);check((e.usingLegacy()||e.activeVoices()==0)&&e.next()==0,"Stuck notes after group release");
  check(allocations.load()==before,"Realtime group allocation");
 }
 // Same-pitch IDs own distinct groups and matching Note Off releases exactly one.
 Engine e;setup(e,5,2);e.note(60,true,.7,17);e.note(60,true,.7,18);run(e,64);
 check(e.activeVoices()==8,"Overlapping same pitch lost");e.note(60,false,0,17);
 int gated=0;for(auto&v:UnisonProbe::voices(e))if(v.active&&v.gate){check(v.noteId==18,"Note ID release mismatch");++gated;}check(gated==4,"Partial ID group release");
 // Released-first, then whole oldest group. Requested 4 at limit 8 gives 2 groups.
 setup(e,3,2);e.note(60,true,.7,1);e.note(62,true,.7,2);e.note(62,false,0,2);e.note(64,true,.7,3);
 bool first=false,second=false,third=false;for(auto&v:UnisonProbe::voices(e))if(v.active){first|=v.noteId==1;second|=v.noteId==2;third|=v.noteId==3;}check(first&&!second&&third,"Released group stealing preference");
 e.note(65,true,.7,4);for(auto&v:UnisonProbe::voices(e))if(v.active)check(v.noteId!=1,"Oldest group not stolen completely");
 // Reconfiguration keeps physical limits and fixed ownership at every sample.
 setup(e,5,3);for(int k=0;k<12;++k)e.note(48+k,true,.7,k);
 auto before=allocations.load();float previous=e.next();
 for(int k=0;k<200;++k){option(e,Voices,k%6);option(e,Unison,k%4);option(e,VoiceMode,k%3);
  float next=e.next();check(std::abs(next-previous)<1.e-6,"Topology change hard step");previous=next;
  for(int i=0;i<13;++i){previous=e.next();invariants(e,VoiceCounts[k%6]);}
 }
 e.allOff();run(e,96000);check(!e.activeVoices(),"Reconfiguration stuck notes");
 e.reset(48000);check(!e.activeVoices()&&e.next()==0,"All Sounds Off reset");check(allocations.load()==before,"Topology RT allocation");
 std::cout<<"PASS physical limits, complete group stealing/release, note IDs, exhaustion, live topology, All Notes/Sounds Off, zero RT allocation\n";
}
void phasesAndPitch(){
 for(int count=1;count<4;++count)for(int clean:{0,1})for(int mode:{0,1,2})for(int cross:{0,1,2})for(int sync:{0,1}){
  Engine a,b;setup(a,5,count,mode,clean);setup(b,5,count,mode,clean);
  for(auto*e:{&a,&b}){e->set(Detune,1.);e->set(8,.7);option(*e,XMode,cross);e->set(XDepth,.2);e->set(27,sync);e->note(60,true,.7,1);}
  int size=UnisonCounts[count];auto &voices=UnisonProbe::voices(a);
  for(int m=0;m<size;++m){check(voices[m].phase[0]==(size==2?m*.21f:float(m*m)/(2*size)-std::floor(float(m*m)/(2*size)))&&voices[m].phase[1]==(size==2?m*.21f:float(m*m)/(2*size)-std::floor(float(m*m)/(2*size))),"Deterministic phase grid");check(voices[m].detunePosition==-voices[size-1-m].detunePosition,"Exact symmetric pitch pairs");}
  float sum=0;for(int m=0;m<size;++m)sum+=voices[m].detunePosition*25;check(std::abs(sum)<1.e-4,"Center pitch offset");
  for(int i=0;i<10000;++i){if(i==2000){for(auto*e:{&a,&b}){e->set(30,.7);e->set(MasterTune,.8);e->set(4,.5);e->set(5,.2);e->note(67,true,.7,2);}}
   if(i==5000)for(auto*e:{&a,&b})e->note(67,false,0,2);
   if(i==7000)for(auto*e:{&a,&b})e->note(60,true,.7,4);
   if(i%71==0)for(auto*e:{&a,&b})e->set(24,((i/71)%6)/5.);
   check(a.next()==b.next(),"Phase/retrigger output not deterministic");
  }
  if(mode==1){auto first=voices[0].phase[0];a.note(72,true,.7,3);check(voices[0].phase[0]==first,"Legato reset member phase");
   for(int m=1;m<size;++m)check(voices[m].hz==voices[0].hz&&voices[m].targetHz==voices[0].targetHz,"Group glide diverged");}
 }
 for(int glide=0;glide<4;++glide){Engine e;setup(e,5,2,1);option(e,GlideMode,glide);e.set(8,.7);e.note(60,true,.7,1);run(e,256);e.allOff();run(e,4096);e.note(72,true,.7,2);auto &v=UnisonProbe::voices(e);check(v[0].glide==(glide==1||glide==3),"Non-legato glide rule");e.note(76,true,.7,3);check(v[0].glide==(glide==1||glide==2),"Legato glide rule");for(int m=1;m<4;++m)check(v[m].hz==v[0].hz,"Glide trajectories diverged");}
 std::cout<<"PASS exact phase grids, centered cents, retrigger/mono/legato/poly, coherent glide/pitch/bend, sync/FM/PM/wave changes and both engines\n";
}
void compatibilityAndTiming(){
 for(int clean:{0,1})for(int mode:{0,2}){Engine a,b;setup(a,5,0,mode,clean);setup(b,5,0,mode,clean);b.set(Detune,1.);a.note(60,true,.7);b.note(60,true,.7);for(int i=0;i<8192;++i)check(a.next()==b.next(),"Detune changed Unison OFF output");}

 static_assert(hostId(Unison)==0x1052 && hostId(Detune)==0x1053 && hostId(DetuneRouteFirst)==0x1054,"New stable IDs");
 for(int version:{1,2,3,4}){auto state=defaults();state[21]=.31;auto bytes=encode(state);bytes[4]=uint8_t(version);bytes.resize(8+(version==4?PreUnisonCount:PreviousCount)*8);State s{};check(decode(bytes.data(),bytes.size(),s)&&s[Unison]==0&&s[Detune]==0,"Old preset unison defaults");}
 for(int slot=0;slot<6;++slot){Engine e;setup(e,5,3);int old=legacyDestinationId(slot),extended=ExpandedDestinationFirst+slot,added=DetuneRouteFirst+slot;
  for(int d=0;d<18;++d){e.set(added,1);e.set(old,d/17.);check(effectiveDestination(e.state,slot)==d,"Legacy automation broken by Detune route");}
  for(int d=0;d<41;++d){e.set(added,1);e.set(extended,(d+1)/41.);check(effectiveDestination(e.state,slot)==d&&stepsFor(extended)==41,"Established /41 automation changed");}
  e.set(added,1);check(effectiveDestination(e.state,slot)==41,"Appended destination inaccessible");auto b=encode(e.state);State s{};check(b[4]==6&&decode(b.data(),b.size(),s)&&s==e.state,"V5 route roundtrip");
 }
 for(double amount:{0.,1.}){Engine e;setup(e,5,3);e.set(Detune,.5);e.set(MatrixFirst,1.);e.set(DetuneRouteFirst,1.);e.set(MatrixFirst+2,amount);e.note(60,true,.7);run(e,4800);auto &v=UnisonProbe::voices(e);check(std::abs(v[0].spread-(amount==0?0:50))<.001,"Modulated spread clamp endpoint");for(int m=1;m<8;++m)check(v[m].detunePosition>v[m-1].detunePosition,"Member order changed");}
 for(int block:{32,64,128,256,512,1024}){
  Engine a,b;setup(a,5,3);setup(b,5,3);
  for(auto*e:{&a,&b}){e->set(MatrixFirst,.1);e->set(DetuneRouteFirst,1.);e->set(MatrixFirst+2,.9);e->set(Detune,.5);e->set(LfoClock,1.);e->set(LfoPhase,1.);e->note(60,true,.7);}
  for(int i=0;i<12000;++i){double beat=100000+i*2./48000;if(i%block==0)a.transport(120,true,beat,true,true);b.transport(120,true,beat,true,true);
   check(a.next()==b.next(),"Detune/host LFO block dependence");auto&v=UnisonProbe::voices(a);for(int m=0;m<8;++m){check(v[m].spread>=0&&v[m].spread<=50,"Detune clamp");check(v[m].spread==v[0].spread,"Group spread incoherent");}
  }
 }
 std::cout<<"PASS v1/v2/v3/v4 defaults, v5 recall, all old automation contracts, 42 destinations, smooth shared spread, host clock and six block sizes\n";
}
struct Metrics{double peak=0,rms=0,pre=0,filter=0,mean=0,acRms=0;int clipped=0;};
Metrics measure(Engine&e,int frames){Metrics m;double energy=0,sum=0;run(e,2048);for(int i=0;i<frames;++i){float x=e.next();check(std::isfinite(x),"Audit NaN/Inf");m.peak=std::max(m.peak,double(std::abs(x)));m.pre=std::max(m.pre,double(std::abs(UnisonProbe::raw(e))));m.filter=std::max(m.filter,double(UnisonProbe::filterPeak(e)));m.clipped+=std::abs(UnisonProbe::raw(e))>1;sum+=x;energy+=double(x)*x;}m.mean=sum/frames;m.rms=std::sqrt(energy/frames);m.acRms=std::sqrt(std::max(0.,energy/frames-m.mean*m.mean));check(!UnisonProbe::faults(e),"Audit numerical fault");return m;}
int beatSeconds(int count,double spread){
 if(count==1||spread==0)return 3;
 double base=440*std::exp2((60-69)/12.);
 double a=base*std::exp2(-spread/2400),b=base*std::exp2((-spread*.5+spread/(count-1))/1200);
 return std::max(3,int(std::ceil(3/(b-a))));
}
void loudness(const std::filesystem::path&folder){
 std::ofstream csv(folder/"average-loudness.csv");csv<<"wave,unison,spread_ct,window_seconds,peak,rms,delta_db_from_off,ac_rms,ac_delta_db\n";
 double worst=0;
 for(int wave=0;wave<6;++wave){double reference=0,referenceAC=0;
  for(int u=0;u<4;++u)for(double d:{0.,.5,1.}){
   Engine e;setup(e,5,u,2,1);e.set(24,wave/5.);e.set(7,0);e.set(Detune,d);if(wave==5)e.set(1,.25);e.note(60,true,.8);
   int seconds=beatSeconds(UnisonCounts[u],d*50);auto m=measure(e,48000*seconds);if(u==0){reference=m.rms;referenceAC=m.acRms;}
   double acDelta=20*std::log10(m.acRms/referenceAC);double delta=20*std::log10(m.rms/reference);worst=std::max({worst,std::abs(delta),std::abs(acDelta)});
   csv<<wave<<','<<UnisonCounts[u]<<','<<d*50<<','<<seconds<<','<<m.peak<<','<<m.rms<<','<<delta<<','<<m.acRms<<','<<acDelta<<'\n';
   check(std::abs(acDelta)<1.5&&std::abs(delta)<1.5,"Unison average normalization dip/rise");
  }
 }
 std::ofstream ring(folder/"ring-audible-level.csv");ring<<"wave,unison,spread_ct,ac_rms,ac_delta_db,mean\n";
 for(int wave:{0,1}){double reference=0;for(int u=0;u<4;++u)for(double d:{0.,.5,1.}){
  Engine e;setup(e,5,u,2,1);e.set(24,wave/5.);e.set(25,wave/5.);option(e,MixerMode,2);e.set(7,1);e.set(Detune,d);e.note(60,true,.8);
  auto m=measure(e,48000*beatSeconds(UnisonCounts[u],d*50));if(u==0)reference=m.acRms;double delta=20*std::log10(m.acRms/reference);
  ring<<wave<<','<<UnisonCounts[u]<<','<<d*50<<','<<m.acRms<<','<<delta<<','<<m.mean<<'\n';check(std::abs(delta)<1.5,"RING average normalization dip/rise");
 }}
 std::ofstream headroom(folder/"default-volume-headroom.csv");headroom<<"wave,unison,spread_ct,peak,rms,ac_rms,pre_output_peak,clipped_samples\n";
 int clippedCases=0;
 for(int wave:{0,2,5})for(int u=0;u<4;++u)for(double d:{0.,.5,1.}){Engine e;setup(e,5,u,2,1);e.set(23,.5);e.set(24,wave/5.);e.set(7,0);e.set(Detune,d);if(wave==5)e.set(1,.25);e.note(60,true,.8);
  auto m=measure(e,48000*beatSeconds(UnisonCounts[u],d*50));headroom<<wave<<','<<UnisonCounts[u]<<','<<d*50<<','<<m.peak<<','<<m.rms<<','<<m.acRms<<','<<m.pre<<','<<m.clipped<<'\n';clippedCases+=m.clipped>0;check(m.peak<=1,"Output ceiling changed");}
 std::cout<<"HEADROOM: default volume, 36 sine/saw/pulse cases; existing output-ceiling cases="<<clippedCases<<" (details retained)\n";
 std::cout<<"LOUDNESS: six waveforms, OFF/2/4/8, zero/half/max spread, full beat windows; worst RMS difference="<<worst<<" dB (total/AC RMS tolerance 1.5 dB; dynamic group normalization), natural beating retained\n";
}
void gainAudit(const std::filesystem::path&folder){
 std::ofstream csv(folder/"gain.csv");csv<<"unison,spread_ct,engine,wave,oscillators,note,mixer,output_peak,rms,pre_output_peak,max_filter_state,mean,ac_rms\n";
 double peak=0,pre=0;
 for(int unison=0;unison<4;++unison)for(double detune:{0.,.002,.5,1.})for(int clean:{0,1})for(int wave=0;wave<6;++wave)for(int osc=0;osc<3;++osc)for(int note:{36,60,84})for(int mixer=0;mixer<6;++mixer){
  Engine e;setup(e,5,unison,2,clean);e.set(Detune,detune);e.set(24,wave/5.);e.set(25,wave/5.);if(wave==5){e.set(1,.25);e.set(3,.25);}e.set(7,osc==0?0:osc==1?1:.5);option(e,MixerMode,mixer);e.note(note,true,.8);
  auto m=measure(e,4096);peak=std::max(peak,m.peak);pre=std::max(pre,m.pre);
  csv<<UnisonCounts[unison]<<','<<detune*50<<','<<clean<<','<<wave<<','<<osc<<','<<note<<','<<mixer<<','<<m.peak<<','<<m.rms<<','<<m.pre<<','<<m.filter<<','<<m.mean<<','<<m.acRms<<'\n';
 }
 // Long windows reveal slow near-zero beating that a short steady-state window misses.
 std::ofstream beating(folder/"beating.csv");beating<<"unison,spread_ct,window_seconds,output_peak,rms,pre_output_peak\n";
 for(int u=0;u<4;++u)for(double d:{0.,.02,.5,1.}){Engine e;setup(e,5,u,2,1);e.set(Detune,d);e.set(24,.4);e.set(7,0);e.note(60,true,.8);int seconds=beatSeconds(UnisonCounts[u],d*50);auto m=measure(e,48000*seconds);beating<<UnisonCounts[u]<<','<<d*50<<','<<seconds<<','<<m.peak<<','<<m.rms<<','<<m.pre<<'\n';}
 std::cout<<"GAIN audit: 10368 representative cases, maximum output peak="<<peak<<" pre-output="<<pre<<"; long beating windows retained\n";
 std::ofstream resonance(folder/"resonance.csv");resonance<<"unison,spread_ct,engine,type,slope,note,cutoff,peak,rms,pre_output_peak,max_filter_state,mean,ac_rms\n";
 for(int u=0;u<4;++u)for(double d:{0.,1.})for(int clean:{0,1})for(int type=0;type<4;++type)for(int slope:{0,1})for(int n:{36,84})for(double cutoff:{.1,1.}){
  Engine e;setup(e,5,u,2,clean);e.set(Detune,d);e.set(24,.4);e.set(7,0);e.set(21,cutoff);e.set(22,1.);option(e,FilterType,type);e.set(28,slope);e.note(n,true,.8);auto m=measure(e,4096);
  resonance<<UnisonCounts[u]<<','<<d*50<<','<<clean<<','<<type<<','<<slope<<','<<n<<','<<cutoff<<','<<m.peak<<','<<m.rms<<','<<m.pre<<','<<m.filter<<','<<m.mean<<','<<m.acRms<<'\n';
 }
}
void performance(const std::filesystem::path&folder){
 std::ofstream csv(folder/"cpu.csv");csv<<"rate,block,unison,physical_voices,median_ms_per_audio_second\n";
 for(double sr:{44100.,48000.,88200.,96000.})for(int block:{32,64,128,256,512,1024})for(int u=0;u<4;++u)for(int full:{0,1}){
  std::array<double,3> times{};int physical=full?32:UnisonCounts[u];
  for(auto&t:times){Engine e;setup(e,5,u,2,1,sr);e.set(24,.4);e.set(25,1);e.set(Detune,.5);for(int k=0;k<(full?32/UnisonCounts[u]:1);++k)e.note(36+k,true,.7,k);run(e,256);
   auto allocated=allocations.load();auto start=std::chrono::steady_clock::now();int frames=int(sr/8);
   for(int base=0;base<frames;base+=block){e.transport(120,true,base*2/sr,true,true);for(int i=base;i<std::min(frames,base+block);++i)e.next();}
   t=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()*sr/frames;check(allocated==allocations.load(),"Timed RT allocation");}
  std::sort(times.begin(),times.end());csv<<sr<<','<<block<<','<<UnisonCounts[u]<<','<<physical<<','<<times[1]<<'\n';
 }
 std::cout<<"PERFORMANCE: 192 load/rate/block configurations, median of three; Engine bytes="<<sizeof(Engine)<<" shared tables=1212 bytes; physical pool=32\n";
}
int main(int argc,char**argv){unsigned csr=_mm_getcsr();_mm_setcsr(csr|0x8040);try{
 std::filesystem::path folder=argc>1?argv[1]:".";std::filesystem::create_directories(folder);
 if(argc>2 && std::string(argv[2])=="--levels-only"){loudness(folder);_mm_setcsr(csr);return 0;}
 groups();phasesAndPitch();compatibilityAndTiming();gainAudit(folder);loudness(folder);performance(folder);_mm_setcsr(csr);return 0;
}catch(const std::exception&e){_mm_setcsr(csr);std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
