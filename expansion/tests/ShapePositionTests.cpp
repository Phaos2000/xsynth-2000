// Source-aware destination and historical-route equivalence through the engine.
#include "Core.h"
#include <iostream>
namespace xs {struct DspValidationProbe{
 static const auto& modulation(const Engine&e){return e.pool[0].mod;}
 static bool positionFast(const Engine&e){return e.globalPositionOnly;}
};}
void check(bool x,const char*text){if(!x)throw std::runtime_error(text);}
void set(xs::Engine&e,const xs::State&s){for(int i=0;i<xs::Count;++i)e.set(i,s[i]);}
void route(xs::State&s,int slot,int oscillator,bool alias){
 s[xs::matrixId(slot)]=1;s[xs::matrixId(slot,2)]=.65;
 if(alias)s[xs::PositionRouteFirst+slot]=(oscillator+1)/2.;
 else if(slot<6)s[xs::matrixId(slot,1)]=(oscillator?4:1)/17.;
 else s[xs::matrixId(slot,1)]=(oscillator?4:1)/41.;
}
int main(){try{using namespace xs;const auto&bank=table::factoryBank();table::Document user;
 user.slots[0]=table::prepareUser(bank.tables[0].pcm,bank.tables[0].length,bank.tables[0].frames,"route fixture");
 unsigned traces=0;
 for(int kind=0;kind<3;++kind)for(int oscillator=0;oscillator<2;++oscillator)for(int slot:{0,11})for(int unison:{0,3})for(int sample=0;sample<3;++sample){
  auto a=defaults();a[VoiceMode]=1;a[Unison]=unison/3.;a[24]=a[25]=1;a[oscParam(oscillator,0)]=kind/2.;a[oscParam(oscillator,4)]=sample/2.;a[oscParam(oscillator,5)]=1;route(a,slot,oscillator,false);
  auto b=a;b[PositionRouteFirst+slot]=(oscillator+1)/2.;auto originalA=a,originalB=b;
  Engine x,y;for(auto*e:{&x,&y})e->setAssets(&bank,&user);set(x,a);set(y,b);x.reset(48000);y.reset(48000);x.note(60,true,.7);y.note(60,true,.7);
  for(int n=0;n<2048;++n){float left=x.next(),right=y.next();check(std::isfinite(left)&&!std::memcmp(&left,&right,4),"canonical/old Position alias audible mismatch");}
  int destination=kind?42+oscillator:oscillator?4:1,other=kind?(oscillator?4:1):42+oscillator;
  check(std::abs(DspValidationProbe::modulation(x)[destination]-.3f)<1.e-6,"combined destination reached wrong control");check(DspValidationProbe::modulation(x)[other]==0,"combined destination modulated both controls");
  check(routeDestination(x.state,slot)==(oscillator?4:1)&&routeDestination(y.state,slot)==42+oscillator,"saved route identity rewritten");
  for(auto s:{originalA,originalB}){auto bytes=encodeDocument(s,user);State restored;table::Document restoredAssets;check(decodeDocument(bytes.data(),bytes.size(),restored,restoredAssets)&&restored==s,"saved canonical/alias route roundtrip changed");}
  int nextKind=(kind+1)%3;x.set(oscParam(oscillator,0),nextKind/2.);y.set(oscParam(oscillator,0),nextKind/2.);
  for(int n=0;n<2048;++n){float left=x.next(),right=y.next();check(std::isfinite(left)&&!std::memcmp(&left,&right,4),"live source switch broke alias equivalence");}
  ++traces;
 }
 auto s=defaults();s[VoiceMode]=1;s[oscParam(0,0)]=.5;route(s,0,0,false);s[matrixId(0)]=.1;
 Engine e;e.setAssets(&bank);set(e,s);e.reset(48000);e.note(60,true);e.next();check(DspValidationProbe::positionFast(e),"combined route bypassed Position optimization");
 check(Count==202&&DestinationCount==46,"host schema changed");std::cout<<"PASS "<<traces<<" source-aware Shape/Position traces: WAVE/ROM/USER,both VCOs,old/new matrix slots,U1/U8,all sample modes,live switches,state and exact alias audio\n";return 0;
 }catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
