// GPL-2.0-or-later. RAW/CLEAN independence and control-cadence regressions.
#include "Core.h"
#include <array>
#include <iostream>
#include <stdexcept>
void check(bool condition,const char *message){if(!condition)throw std::runtime_error(message);}
void fixture(xs::Engine &e,bool expanded=false) {
  e.reset(48000);e.set(24,.4);e.set(25,.8);
  if(expanded){e.set(xs::VoiceMode,1);e.set(xs::Voices,0);}
}
double tone(const std::array<float,8192> &samples,double frequency) {
  double re=0,im=0;
  for(size_t i=0;i<samples.size();++i){double p=2*xs::Pi*frequency*i/48000;re+=samples[i]*std::cos(p);im+=samples[i]*std::sin(p);}
  return std::hypot(re,im)/samples.size();
}
int main() {
 try {
  for(bool expanded:{false,true})for(int active:{0,1})for(int clean:{0,1}) {
    xs::Engine a,b;fixture(a,expanded);fixture(b,expanded);
    for(auto *e:{&a,&b}){e->set(7,active);e->set(xs::Engine1+active,clean);e->note(84,true);}
    b.set(xs::Engine1+(1-active),1);
    for(int i=0;i<8192;++i)check(a.next()==b.next(),"Muted oscillator rendering mode changed active oscillator output");
  }
  for(int mode:{1,2})for(int direction:{0,1})for(int e1:{0,1})for(int e2:{0,1}) {
    xs::Engine e;fixture(e,true);e.set(xs::Engine1,e1);e.set(xs::Engine2,e2);
    e.set(xs::XMode,mode/2.);e.set(xs::XDirection,direction);e.set(xs::XDepth,.65);e.set(27,1);
    double energy=0;for(int n=0;n<8192;++n){if(n==0)e.note(84,true);float sample=e.next();check(std::isfinite(sample)&&std::abs(sample)<=1,"Asymmetric XMOD output invalid");energy+=sample*sample;}
    check(energy>0,"Asymmetric XMOD silent");
  }
  {
    xs::Engine e;fixture(e,true);e.set(24,0);e.set(7,0);e.set(xs::FilterType,1./3);e.set(21,.01);
    e.set(xs::MatrixFirst,1);e.set(xs::MatrixFirst+1,0);e.set(xs::MatrixFirst+2,1);e.note(96,true);
    for(int i=0;i<4096;++i)e.next();std::array<float,8192> samples{};for(auto &v:samples)v=e.next();
    double folded=48000-440*std::exp2(27./12)*16;
    auto fold=tone(samples,folded),oldCap=tone(samples,21600);
    check(fold>oldCap*10 && fold>.001,"RAW frequency was capped instead of folding");
    std::cout<<"RAW above-Nyquist fold amplitude="<<fold<<" former-clamp frequency="<<oldCap<<'\n';
  }
  {
    xs::Engine a,b;fixture(a,true);fixture(b,true);
    for(auto *e:{&a,&b}){e->set(4,1);e->set(xs::MatrixFirst,.1);e->set(xs::MatrixFirst+1,0);e->set(xs::MatrixFirst+2,.6);e->note(60,true);}
    for(int i=0;i<8192;++i){b.set(23,.5+(i&1?1.e-12:0));check(a.next()==b.next(),"Unrelated gain updates reset modulation interpolation cadence");}
  }
  for(int kind=0;kind<6;++kind)for(float increment:{-.8f,-.2f,.0f,.2f,.8f,2.f}) {
    for(int i=0;i<100;++i){float p=i/100.f,raw=kind==1?(p<.25f?4*p:p<.75f?2-4*p:4*p-4):0;
      check(std::isfinite(xs::oscillator::clean(p,increment,kind,.5f,raw)),"CLEAN correction invalid");}
  }
  for(float phase:{0.f,.1f,.5f,.999f}) {
    check(xs::oscillator::clean(phase,.1f,5,0,-1)==-1,"Zero pulse width not preserved");
    check(xs::oscillator::clean(phase,.1f,5,1,1)==1,"Full pulse width not preserved");
  }
  // The clean triangle is evaluated at the modulated phase, not integrated from
  // the unmodulated frequency. Quarter-cycle displacement must move its peak.
  check(xs::oscillator::clean(.25f,.01f,1,.5f,1)>.9f,"CLEAN triangle peak lost");
  check(std::abs(xs::oscillator::clean(.5f,.01f,1,.5f,0))<.01f,"CLEAN triangle phase geometry wrong");
  std::cout<<"PASS: oscillator mode isolation, all mixed FM/PM directions, RAW folding, pulse endpoints, triangle phase, fixed modulation cadence\n";
  return 0;
 }catch(const std::exception &error){std::cerr<<error.what()<<'\n';return 1;}
}
