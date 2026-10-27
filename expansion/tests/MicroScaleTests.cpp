// GPL-2.0-or-later. Full EDO steps, exact scalar/SIMD clocks, and old-ID isolation.
#include "Core.h"
#include <iostream>
#include <memory>
static void check(bool ok,const char*msg){if(!ok)throw std::runtime_error(msg);}
namespace xs {struct DspValidationProbe{
 static double frequency(const Engine&e){for(const auto&v:e.pool)if(v.active&&v.tables[0].ready)return v.tables[0].current.cycleHz;return -1;}
};}
int main(){try{using namespace xs::table;const auto&bank=factoryBank();check(bank.tables.size()==69,"Micro bank inventory");
 size_t exact=0;double maxCents=0;
 for(int id=0;id<66;++id)for(float p:{0.f,.3f,1.f})check(positionTuning::frequency(id,261.6255653005986,p)==261.6255653005986,"Old ROM clock changed");
 for(int id=66;id<=68;++id){const auto&t=bank.tables[id];int edo=positionTuning::divisions(id);check(t.id==id&&t.rootBin==1,"Micro descriptor");
  Selection sel;sel.source=1;sel.data=&t;Selection user=sel;user.source=2;check(user.positionFrequency(440,1)==440,"USER inherited factory pitch map");
  for(int degree=0;degree<=edo;++degree){float position=float(double(degree)/edo);double ratio=std::exp2(double(degree)/edo);
   check(positionTuning::degree(edo,position)==degree,"Scale degree mapping");
   for(float delta:{-.499f,.499f})if(degree>0&&degree<edo)check(positionTuning::degree(edo,float((degree+delta)/edo))==degree,"Scale boundary rounding");
   for(auto sample:{Sample::Hold,Sample::Linear,Sample::Band})for(auto frame:{Frame::Step,Frame::Morph}){
    sel.sample=sample;sel.frame=frame;Oscillator::Head scalar[4],vector[4],hold[4],holdRef[4];Oscillator::Head*heads[4],*holds[4];
    for(int i=0;i<4;++i){scalar[i].assign(sel);scalar[i].phase.fill(.13+i*.17);vector[i]=hold[i]=holdRef[i]=scalar[i];heads[i]=&vector[i];holds[i]=&hold[i];}
    alignas(16)float hz[4]={261.62555f,329.62756f,391.99542f,440.f},pm[4]={0,.1f,-.2f,.3f};auto freq=_mm_load_ps(hz),mod=_mm_load_ps(pm);
    for(int n=0;n<64;++n){bool wrapped[4]{};auto out=MemberTableBatch::read<4>(heads,0,freq,mod,position,48000,wrapped,nullptr);alignas(16)float values[4];_mm_store_ps(values,out);
     for(int i=0;i<4;++i){bool w=false;float x=scalar[i].read(0,hz[i],pm[i],position,0,48000,w,[](auto...){return 0.f;});check(!std::memcmp(&x,&values[i],4)&&w==wrapped[i]&&scalar[i].phase[0]==vector[i].phase[0],"Micro scalar/SIMD phase or sample mismatch");++exact;}
     if(sample==Sample::Hold){auto plan=HoldBatch::prepare<4>(holds,freq,position,48000);bool wraps[4]{};_mm_store_ps(values,HoldBatch::read(plan,0,wraps));
      for(int i=0;i<4;++i){bool w=false;float x=holdRef[i].read(0,hz[i],0,position,0,48000,w,[](auto...){return 0.f;});check(!std::memcmp(&x,&values[i],4)&&w==wraps[i],"Micro prepared HOLD mismatch");++exact;}}
    }
    double expected=double(hz[0])*ratio;maxCents=std::max(maxCents,std::abs(1200*std::log2(scalar[0].cycleHz/expected)));check(std::abs(scalar[0].cycleHz-expected)<1e-9,"Micro frequency accuracy");
    ClockTransition clock;clock.reset({&t,sample,frame},48000);clock.next(hz[0],position,48000);check(std::abs(clock.current.frequency-expected)<1e-9,"Clock-owning reader misses scale");
   }
  }
  for(int u:{0,3})for(int sample=0;sample<3;++sample)for(double sr:{44100.,48000.,96000.}){
   auto e=std::make_unique<xs::Engine>();e->setAssets(&bank);auto s=xs::defaults();s[xs::VoiceMode]=1;s[xs::Unison]=u/3.;s[xs::Detune]=0;s[7]=0;s[xs::oscParam(0,0)]=.5;s[xs::oscParam(0,1)]=id/65535.;s[xs::oscParam(0,4)]=sample/2.;e->state=s;e->reset(sr);e->note(60,true,.7);
   for(int i=0;i<512;++i)e->next();double base=xs::DspValidationProbe::frequency(*e);check(base>260&&base<263,"C4 base clock");
   for(int degree=0;degree<=edo;++degree){e->set(xs::oscParam(0,3),double(degree)/edo);for(int i=0;i<256;++i){e->next();check(std::isfinite(e->outputLeft())&&std::isfinite(e->outputRight()),"Micro engine nonfinite");}
    double actual=xs::DspValidationProbe::frequency(*e),wanted=base*std::exp2(double(degree)/edo);check(std::abs(1200*std::log2(actual/wanted))<.001,"Unison/sample-mode engine scale clock");}
  }
 }
 std::cout<<"PASS "<<exact<<" bit-exact scalar/SIMD/HOLD micro samples; every19/24/31 EDO degree, octave endpoints, Position cache changes, USER/old-ID isolation, U1/U8 x3 sample modes x3 rates; max ratio error "<<maxCents<<" cents\n";return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
