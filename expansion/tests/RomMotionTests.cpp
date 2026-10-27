// GPL-2.0-or-later. Native tuning/level/motion checks for the reviewed ROM revision.
#include "DocumentAssets.h"
#include "RomMenu.h"
#include <array>
#include <complex>
#include <iostream>
static void check(bool ok,const char*msg){if(!ok)throw std::runtime_error(msg);}
int main(){try{
 const auto&bank=xs::table::factoryBank();check(bank.tables.size()==69,"ROM inventory");
 const auto&tilt=bank.tables[60];check(tilt.id==60&&tilt.rootBin==16,"TILT pitch reference changed");
 const auto&rail=bank.tables[51];check(rail.id==51&&rail.rootBin==3,"RAIL pitch reference changed");
 double minMargin=100,minRms=1,maxRms=0;size_t reads=0;
 for(int id:{51,56,57,58,59,60,61,66,67,68}){
  const auto&t=bank.tables[id];check(t.id==id&&t.frames==16,"Revised table identity/geometry");
  for(auto frame:{xs::table::Frame::Step,xs::table::Frame::Morph})for(int j=0;j<=240;++j){
   float p=j/240.f;
   // Root-bin population in the actual selected/interpolated native PCM frames.
   auto f=xs::table::chooseFrames(t,p,frame);double sum=0;std::vector<std::complex<double>> spectrum(t.length/2+1);
   for(int n=0;n<t.length;++n){float a=t.pcm[f.a*t.length+n];float x=a+f.blend*(t.pcm[f.b*t.length+n]-a);sum+=double(x)*x;
    if(id==60)for(int k=1;k<=t.length/2;++k){double theta=-6.283185307179586*k*n/t.length;spectrum[k]+=double(x)*std::complex<double>(std::cos(theta),std::sin(theta));}}
   double rms=std::sqrt(sum/t.length);minRms=std::min(minRms,rms);maxRms=std::max(maxRms,rms);
   if(id==60){double other=0;for(int k=1;k<=t.length/2;++k)if(k!=t.rootBin)other=std::max(other,std::abs(spectrum[k]));double margin=20*std::log10(std::abs(spectrum[t.rootBin])/other);minMargin=std::min(minMargin,margin);check(margin>=8.,"TILT root loses prominence over Position");}
   for(double rootHz:{130.8127826502993,261.6255653005986,523.2511306011972})for(auto sample:{xs::table::Sample::Hold,xs::table::Sample::Linear,xs::table::Sample::Band})for(int n=0;n<512;++n){float x=xs::table::read(t,n/512.,p,sample,frame,rootHz/t.rootBin,48000);check(std::isfinite(x)&&std::abs(x)<1,"Invalid revised ROM playback");++reads;}
  }
 }
 auto order=xs::romMenu::order(bank);std::array<int,9> counts{};std::array<bool,69> seen{};int prior=-1,first=-1,second=-1;
 for(size_t j=0;j<order.size();++j){const auto&t=bank.tables[order[j]];check(t.id>=0&&t.id<69&&!seen[t.id],"ROM identity missing/duplicate");seen[t.id]=true;int group=xs::romMenu::category(t);check(group>=prior&&group<9,"ROM group order");prior=group;++counts[group];if(t.id==20)first=int(j);if(t.id==35)second=int(j);}
 check(counts==std::array<int,9>{9,3,3,5,6,13,11,14,5}&&second==first+1,"ROM grouping or ethereal adjacency");
 std::cout<<"PASS "<<reads<<" native ROM lookups; TILT C4 root margin >= "<<minMargin<<" dB over all STEP/MORPH positions; PCM interpolated RMS "<<minRms<<".."<<maxRms<<"; all69 identities in9 shared folders\n";
 return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
