// Exact new waveform-control sharing against the frozen unshared-wave engine.
#include "Core.h"
#define Engine WaveSharingReference
#include "fixtures/WaveSharingReference.h"
#undef Engine
#include <fstream>
#include <memory>
#include <iostream>
namespace xs{struct DspValidationProbe{template<class E>static void dump(const E&e){for(int i=0;i<4;++i){const auto&v=e.pool[i];std::cerr<<"m"<<i<<" shared="<<e.groups[0].noteShared<<" discrete="<<e.groups[0].discreteShared<<" control="<<v.control<<" wave="<<v.waves[0].target<<" rem="<<v.waves[0].remaining<<" phase="<<v.phase[0]<<" cross="<<v.cross.target<<" cw="<<v.cross.weights[0]<<','<<v.cross.weights[1]<<','<<v.cross.weights[2]<<" mix="<<v.mix.target<<" ww=";for(auto f:v.waves[0].weights)std::cerr<<f<<',';std::cerr<<'\n';}}};}
template<class E>void set(E&e,const xs::State&s){for(int i=0;i<xs::Count;++i)e.set(i,s[i]);}
void check(bool x,const char*m){if(!x)throw std::runtime_error(m);}
uint64_t exact=0;
template<class A,class B>void sample(A&a,B&b,int trace,int n){float x=a.next(),y=b.next();if(!std::isfinite(x)||std::memcmp(&x,&y,4)||a.outputRight()!=b.outputRight()){std::cerr<<"trace="<<trace<<" frame="<<n<<" old="<<x<<" new="<<y<<'\n';xs::DspValidationProbe::dump(a);xs::DspValidationProbe::dump(b);throw std::runtime_error("wave sharing audio differs");}++exact;}
int main(int argc,char**argv){try{using namespace xs;if(argc!=2)return 2;const auto&bank=table::factoryBank();table::Document doc;doc.slots[0]=table::prepareUser(bank.tables[0].pcm,bank.tables[0].length,bank.tables[0].frames,"wave sharing fixture");int traces=0;
 for(int rate:{44100,48000,96000})for(int u:{1,2,3})for(int source=1;source<SourceCount;++source)for(int renderer=0;renderer<3;++renderer)for(int oscillator=0;oscillator<2;++oscillator){
  auto s=defaults();s[VoiceMode]=1;s[Voices]=.8;s[Unison]=u/3.;s[Detune]=.7;s[24]=(traces%6)/5.;s[25]=((traces+2)%6)/5.;s[Engine1]=s[Engine2]=renderer==1;s[RasterFirst]=s[RasterFirst+1]=renderer==2;s[XMode]=(traces%3)/2.;s[XDepth]=.08;s[XDirection]=traces%2;s[27]=traces%2;s[Width]=(traces%4)/3.;s[12]=.8;s[Loop2]=1;s[Eg2Sync]=2/3.;s[Eg2Rate]=6/9.;
  int other=1-oscillator;s[oscParam(other,0)]=(traces%3)/2.;s[oscParam(other,4)]=1;s[oscParam(other,5)]=1;
  s[matrixId(0)]=source<=10?source/10.:0;s[SourceExtensionFirst]=source>=11?(source-10)/3.:0;s[matrixId(0,2)]=.91;s[ExpandedDestinationFirst]=(19+oscillator)/41.;
  // Exercise concurrent existing discrete optimizations while the Wave route
  // enables note sharing: XMOD mode/direction, mixer mode and LFO polarity.
  if(traces%5!=4){int d=std::array<int,4>{20,21,22,27}[traces%5];s[SourceExtensionFirst+1]=1/3.;s[matrixId(1,2)]=traces%2?.2:.8;s[ExpandedDestinationFirst+1]=(d+1)/41.;s[lfoParam(1,0)]=1;}
  auto a=std::make_unique<WaveSharingReference>();auto b=std::make_unique<Engine>();a->setAssets(&bank,&doc);b->setAssets(&bank,&doc);set(*a,s);set(*b,s);a->reset(rate);b->reset(rate);a->transport(120,true,0,true);b->transport(120,true,0,true);a->note(60,true,.7,0);b->note(60,true,.7,0);
  for(int n=0;n<2048;++n){auto events=[&](auto&e){if(n==257){e.set(31,.7);e.set(Pressure,.2);e.set(30,.8);}if(n==513)e.set(matrixId(0,2),.13);if(n==769)e.set(oscParam(other,0),((traces+1)%3)/2.);if(n==1025)e.note(64,true,.5,1);if(n==1281)e.note(60,false,.7,0);if(n==1537){e.set(matrixId(0),0);e.set(SourceExtensionFirst,0);}if(n==1793){e.set(matrixId(0),.3);e.set(ExpandedDestinationFirst,(19+oscillator)/41.);}};events(*a);events(*b);sample(*a,*b,traces,n);}++traces;
 }
 // The exact supplied patch, different pitches and overlapping releases.
 std::ifstream f(argv[1],std::ios::binary);std::vector<uint8_t>bytes{std::istreambuf_iterator<char>(f),{}};State s;table::Document assets;check(decodeDocument(bytes.data(),bytes.size(),s,assets),"patch decode");auto a=std::make_unique<WaveSharingReference>();auto b=std::make_unique<Engine>();a->setAssets(&bank,&assets);b->setAssets(&bank,&assets);set(*a,s);set(*b,s);a->reset(48000);b->reset(48000);a->transport(120,true,0,true);b->transport(120,true,0,true);int pitches[]={48,60,64,55,67,62,72,57};
 for(int n=0;n<288000;++n){auto events=[&](auto&e){if(n%12000==0&&n/12000<8){int i=n/12000;e.note(pitches[i],true,.7,i);}if(n>=9600&&(n-9600)%12000==0&&(n-9600)/12000<8){int i=(n-9600)/12000;e.note(pitches[i],false,.7,i);}};events(*a);events(*b);sample(*a,*b,traces,n);}
 std::cout<<"PASS waveform sharing "<<exact<<" exact left samples + equal right output;"<<traces<<" matrix traces,all13 sources,U2/U4/U8,CLASSIC/CLEAN/RASTER,44.1/48/96k,FM/PM/sync,table source switches,width,route removal/re-enable,and exact supplied different-note patch\n";return 0;
 }catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
