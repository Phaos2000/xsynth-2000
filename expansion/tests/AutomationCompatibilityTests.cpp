// GPL-2.0-or-later. Public automation contract, independently of native migration.
#include "Core.h"
#include "HistoricalState.h"
#include "ProductIdentity.h"
#define xs pre_automation
#include "pre-automation/Core.h"
#undef xs
#define xs published_contract
#include "pre-transitions/ExpansionParameters.h"
#undef xs
#include <atomic>
#include <cstdlib>
#include <iostream>
#include <new>
#include <stdexcept>
std::atomic<size_t> allocations{0};
void* operator new(size_t n){++allocations;if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void* operator new[](size_t n){return ::operator new(n);}
void operator delete(void*p)noexcept{std::free(p);}void operator delete[](void*p)noexcept{std::free(p);}
void operator delete(void*p,size_t)noexcept{std::free(p);}void operator delete[](void*p,size_t)noexcept{std::free(p);}
namespace xs {struct PreUnisonProbe {
 static int destination(Engine&e,int slot){e.prepare();for(int i=0;i<e.routeCount;++i)if(e.activeRoutes[i].source==slot+1)return e.activeRoutes[i].destination;return -1;}
 static unsigned faults(Engine&e){return e.numericalFaults;}
};}
using namespace xs;
void check(bool b,const char*m){if(!b)throw std::runtime_error(m);}
void run(Engine&e,int n){while(n--)check(std::isfinite(e.next()),"NaN/Inf");}
// Literal historical contract copied from the retained 0.1.0 parameter list,
// then checked against that implementation rather than derived from new names.
constexpr const char* legacyNames[]={"VCO1 Pitch","VCO1 Shape","VCO1 Level","VCO2 Pitch","VCO2 Shape","VCO2 Level","XMOD Depth","Mixer Balance","VCF Cutoff","VCF Resonance","LFO Rate","LFO Depth","EG1 Attack","EG1 Decay","EG1 Release","EG2 Attack","EG2 Decay","EG2 Release"};
void automation(){
 for(int d=0;d<18;++d)check(!std::strcmp(legacyNames[d],published_contract::Destinations[d]),"Frozen destination contract mismatch");
 for(int slot=0;slot<6;++slot){
  int id=legacyDestinationId(slot),extended=ExpandedDestinationFirst+slot;
  check(hostId(id)==0x103bu+slot*3 && stepsFor(id)==17,"Historical IDs/steps changed");
  check(hostId(extended)==0x104cu+slot && stepsFor(extended)==41,"New IDs/steps changed");
  for(int block:{1,7,32,128,512}){
   Engine e;e.reset(48000);e.set(VoiceMode,1.);e.set(23,.1);
   for(int s=0;s<6;++s){e.set(MatrixFirst+s*3,(s+1)/10.);e.set(MatrixFirst+s*3+2,.6);}
   e.note(60,true,.7);run(e,512);
   auto before=allocations.load();
   for(int d:{0,17,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17})for(int repeat=0;repeat<3;++repeat){
    // No state migration involved, including override takeover on equal writes.
    e.set(extended,1.);e.set(id,d/17.);run(e,block);
    check(e.state[extended]==0 && effectiveDestination(e.state,slot)==d,"Legacy automation meaning changed");
    check(PreUnisonProbe::destination(e,slot)==d,"Engine did not resolve legacy destination");
    for(int other=0;other<6;++other)if(other!=slot)check(e.state[ExpandedDestinationFirst+other]==0 && e.state[legacyDestinationId(other)]==0,"Cross-slot automation leak");
   }
   for(int d=0;d<41;++d){e.set(extended,(d+1)/41.);run(e,block);check(PreUnisonProbe::destination(e,slot)==d,"Expanded automation inaccessible");}
   e.set(extended,0.);check(effectiveDestination(e.state,slot)==17,"Follow legacy mapping");
   check(allocations.load()==before,"Realtime allocation");check(!PreUnisonProbe::faults(e),"Numerical fault");
   e.allOff();run(e,96000);check(e.next()==0,"Stuck voice");
  }
 }
 std::cout<<"PASS six slots x 18 historical choices, adjacent/jump/repeated live writes, five block sizes; six x 41 new choices; no RT allocation, NaN/Inf or stuck notes\n";
}
void unchangedDsp(){
 // Compare the exact pre-fix engine under equivalent routing events. This
 // protects the preceding click/LFO work without regenerating any WAV baseline.
 for(int block:{1,7,32,128,512})for(double rate:{44100.,48000.,96000.}){
  Engine current;pre_automation::Engine previous;
  current.reset(rate);previous.reset(rate);
  for(int id:{VoiceMode,Engine1,Engine2}){current.set(id,1.);previous.set(id,1.);}
  current.set(23,.1);previous.set(23,.1);
  current.set(MatrixFirst,1.);previous.set(MatrixFirst,1.);
  current.set(MatrixFirst+2,.9);previous.set(MatrixFirst+2,.9);
  current.note(60,true,.7);previous.note(60,true,.7);
  for(int n=0;n<16000;++n){
   if(n%17==0){int d=(n/17)%41;current.set(ExpandedDestinationFirst,(d+1)/41.);previous.set(MatrixFirst+1,d/40.);}
   if(n%block==0){double beat=100000+n*2./rate;current.transport(120,true,beat,true,true);previous.transport(120,true,beat,true,true);}
   if(n==12000){current.allOff();previous.allOff();}
   float a=current.next(),b=previous.next();check(std::isfinite(a)&&std::isfinite(b),"Routed output nonfinite");
  }
  check(!PreUnisonProbe::faults(current),"Discrete modulation numerical fault");
 }
 std::cout<<"PASS finite current DSP and historical comparison under all 41 routed targets (filter/direct-LFO fixes intentionally differ) including Wave/XMOD/direction/mixer/filter/slope/curve/loop; five block sizes, three rates; retained WAVs untouched\n";
}
void migrations(){
 for(int version:{1,2,3})for(int slot=0;slot<6;++slot)for(int d=0;d<(version==3?41:18);++d){
  auto old=defaults();for(int i=0;i<PreviousCount;++i)old[i]=stepped(i,.371);
  int id=legacyDestinationId(slot);old[id]=d/double(version==3?40:17);
  auto bytes=historicalState(old);bytes.resize(8+PreviousCount*8);bytes[4]=uint8_t(version);
  State migrated{};check(decode(bytes.data(),bytes.size(),migrated),"Historical state rejected");
  check(effectiveDestination(migrated,slot)==d,"Native destination migration changed meaning");
  for(int i=0;i<PreviousCount;++i)if(!legacyDestination(i))check(migrated[i]==(version==1&&(i==0||i==2)?.5+.5*old[i]:old[i]),"Non-destination migration changed");
  auto current=encode(migrated);State again{};
  check(current[4]==7 && current.size()==StateBytes && decode(current.data(),current.size(),again)&&again==migrated,"V4 roundtrip");
  Engine e;e.state=migrated;e.reset(48000);e.set(id,17/17.);check(effectiveDestination(e.state,slot)==17,"Old automation after native load");
 }
 for(int slot=0;slot<6;++slot)for(int d=0;d<41;++d){auto s=defaults();s[ExpandedDestinationFirst+slot]=(d+1)/41.;State t{};auto b=encode(s);check(decode(b.data(),b.size(),t)&&t==s,"Expanded V4 recall");}
 std::cout<<"PASS XSED v1/v2 historical and v3 development migration; v4 all-destination roundtrip; non-destination preservation\n";
}
int main(){try{automation();migrations();unchangedDsp();return 0;}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
