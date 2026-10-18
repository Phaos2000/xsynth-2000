// GPL-2.0-or-later. Diagnose authorized DSP changes against frozen source.
#define main validation_entry
#include "DspValidation.cpp"
#undef main
#define xs published_xs
#include "pre-transitions/Core.h"
#undef xs
Audio published(const Case&c){
 published_xs::Engine e;auto state=patch(c);for(int i=0;i<published_xs::Count;++i)e.state[i]=state[i];
 for(int k=0;k<6;++k){int id=xs::MatrixFirst+k*3+1;e.state[id]=xs::effectiveDestination(state,k)/17.;}
 e.reset(48000);Audio a(96000);
 for(int base=0;base<96000;base+=128){e.transport(123,true,base*123/(60.*48000),true,true);
 for(int i=base;i<std::min(96000,base+128);++i){if(i==0){e.note(60,true,.8f,1);if(c.chord){e.note(64,true,.65f,2);e.note(67,true,.7f,3);e.note(72,true,.6f,4);}}
 if(i==int(48000*.37))e.set(30,.53);if(i==int(48000*.71))e.set(30,.5);if(c.sweep)e.set(21,.002+.25*double(i)/96000);if(i==60000)e.allOff();a[i]=e.next();}}
 return a;
}
int main(){for(const auto&c:cases){auto old=published(c),now=render(c,48000,128);double max=0,energy=0;int first=-1;
 for(size_t i=0;i<old.size();++i){double d=std::abs(old[i]-now[i]);max=std::max(max,d);energy+=d*d;if(d>1.e-6&&first<0)first=int(i);}
 std::cout<<c.name<<" first="<<first<<" max="<<max<<" rms="<<std::sqrt(energy/old.size())<<'\n';}return 0;}
