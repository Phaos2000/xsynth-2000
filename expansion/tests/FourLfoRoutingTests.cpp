// GPL-2.0-or-later. Direct-depth isolation and all twelve matrix lanes.
#include "Core.h"
#include <iostream>
#include <stdexcept>
namespace xs {struct FourProbe {
 static auto&voice(Engine&e){return e.pool[0];}
 static auto values(Engine&e){return e.lfoValues;}
 static auto&groups(Engine&e){return e.groups;}
 static void converge(Engine&e){for(auto &v:e.pool)if(v.active){v.phase[0]=v.phase[1]=0;for(auto &phase:v.topologyPhase)phase={0,0};}}
 static int destination(Engine&e){e.prepare();return e.routeCount==1?e.activeRoutes[0].destination:-1;}
};}
void check(bool value,const char*text){if(!value)throw std::runtime_error(text);}
int main(){try{using namespace xs;
 for(int slot=0;slot<12;++slot)for(int destination=0;destination<DestinationCount;++destination){Engine e;e.set(VoiceMode,1);e.set(matrixId(slot),1);e.set(matrixId(slot,2),.6);if(slot<6){if(destination==41)e.set(DetuneRouteFirst+slot,1);else e.set(ExpandedDestinationFirst+slot,(destination+1)/41.);}else e.set(matrixId(slot,1),destination/41.);check(FourProbe::destination(e)==destination,"Destination inaccessible on matrix lane");e.note(60,true);for(int n=0;n<256;++n)check(std::isfinite(e.next()),"Destination render nonfinite");}
 for(double sr:{44100.,48000.,88200.,96000.})for(int clean:{0,1})for(int lfo=0;lfo<4;++lfo){Engine pitch,filter,flat;for(auto*e:{&pitch,&filter,&flat}){e->set(VoiceMode,1);e->set(Engine1,clean);e->set(Engine2,clean);e->set(21,.1);e->set(lfoParam(lfo,0),normalized(4,1.3));e->reset(sr);}pitch.set(lfoParam(lfo,1),.1);filter.set(lfoParam(lfo,2),.1);for(auto*e:{&pitch,&filter,&flat})e->note(60,true);double largestFilter=0;
  for(int n=0;n<int(sr/4);++n){auto &v=FourProbe::voice(pitch);float phase[2]={v.phase[0],v.phase[1]};pitch.next();filter.next();flat.next();auto values=FourProbe::values(pitch);float expected=v.hz*float(physical(0,pitch.state[0]))*std::max(0.f,1+values[lfo]*.1f)/float(sr);for(int o=0;o<2;++o){float delta=v.phase[o]-phase[o];if(delta<0)delta+=1;check(std::abs(delta-expected)<2e-6,"Direct pitch rate law or sample smoothness differs between VCOs");}largestFilter=std::max(largestFilter,double(std::abs(FourProbe::voice(filter).g-FourProbe::voice(flat).g)));}
  check(largestFilter>.001,"Dedicated filter depth did not reach cutoff");
 }
 Engine e;e.set(VoiceMode,1);for(int l=0;l<4;++l)e.set(lfoParam(l,0),.13*(l+1));int destinations[]={0,8,1,41};for(int slot=0;slot<12;++slot){e.set(matrixId(slot),.1);if(slot%4)e.set(SourceExtensionFirst+slot,(slot%4)/3.);e.set(matrixId(slot,2),.6);int destination=destinations[slot%4];if(slot<6){if(destination==41)e.set(DetuneRouteFirst+slot,1);else e.set(ExpandedDestinationFirst+slot,(destination+1)/41.);}else e.set(matrixId(slot,1),destination/41.);}e.note(60,true);for(int n=0;n<4096;++n){e.next();auto values=FourProbe::values(e);for(int l=0;l<4;++l)check(std::abs(FourProbe::voice(e).mod[destinations[l]]-values[l]*.6f)<2e-6,"LFO source alias or non-sample-clock summing");}
 for(double sr:{44100.,48000.,88200.,96000.})for(int count=1;count<=3;++count)for(int wave=0;wave<6;++wave){Engine group;group.set(VoiceMode,1);group.set(Unison,count/3.);group.set(24,wave/5.);group.set(25,wave/5.);group.set(7,.5);group.set(21,.1);group.set(Detune,.5);group.reset(sr);group.note(60,true);for(int n=0;n<10000;++n){if(n==4096||n==6000)FourProbe::converge(group);group.next();if(n>2048)for(auto &g:FourProbe::groups(group))if(g.active)check(std::abs(g.last)<=1.414214f*g.peakEnvelope+2e-5f,"Group crest bound exceeded under forced convergence");}}
 std::cout<<"PASS all 42 destinations on all 12 lanes; 4 isolated direct pitch/filter depths at 4 rates in CLASSIC/CLEAN; exact simultaneous four-source summing; 2/4/8 crest bound under forced convergence at all rates\n";return 0;
 }catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
