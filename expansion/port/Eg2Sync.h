// GPL-2.0-or-later. Bounded note-relative EG2 timing; legacy recurrence stays in Engine.
#pragma once
#include <algorithm>
#include <cmath>
#include <limits>
namespace xs::eg2 {
inline constexpr double Beats[]={16,8,4,2,1,.5,.25,.125,.0625,.03125};
inline constexpr const char* Rates[]={"4/1","2/1","1/1","1/2","1/4","1/8","1/16","1/32","1/64","1/128"};
inline double completionBound(const float*r,int curve,float sustain,int cycle){
 double result=0;
 for(int i=0;i<(cycle==2?3:2);++i){double c=std::clamp(r[i],0.f,1.f);if(c<=0)return std::numeric_limits<double>::infinity();
  if(curve!=1)result+=std::ceil(4.60517/c)+3;
  else {double distance=i==0?1:i==1?std::max(std::abs(1.-sustain),std::abs(.99-sustain)):std::min(1.,double(sustain)+1.e-5),threshold=i==0?double(.01f):double(1.e-5f);
   result+=distance<=threshold?1:c>=1?1:std::max(1.,std::ceil(std::log(threshold/distance)/std::log1p(-c))+3);}
 }return result;
}
inline bool valid(const float*r,int curve,float sustain,int cycle,int division,double bpm,double sr){return completionBound(r,curve,sustain,cycle)<=Beats[std::clamp(division,0,9)]*60*sr/bpm;}
struct Runtime {
 double phase=0,start=0,weights[3]{},total=0;
 int mode=0,division=-1,cycle=2;bool pending=false;
 void snapshot(const float*r,int topology){cycle=topology;total=0;for(int i=0;i<(cycle==2?3:2);++i){weights[i]=1./std::max(double(r[i]),1.e-30);total+=weights[i];}}
 template<class E>void leave(E&e){if(mode==2){e.start=e.value;e.progress=0;}mode=0;division=-1;pending=false;phase=0;}
 template<class E,class Absolute>float next(E&e,const float*r,int curve,float sustain,int topology,int policy,int rate,double bpm,double sr,bool gate,Absolute absolute){
  if(!gate){absolute();return float(e.value);}
  if(mode!=policy||division!=rate){mode=policy;division=rate;phase=0;pending=false;start=e.value;e.stage=0;e.start=e.value;e.progress=0;snapshot(r,topology);}
  if(pending){pending=false;
   if(mode==2){start=cycle==2?0:sustain;snapshot(r,topology);}
   else if(mode==1){e.stage=0;e.start=e.value;e.progress=0;}
   else {float target=e.stage==0?1:e.stage==1?sustain:0;bool done=curve==1?std::abs(e.value-target)<(e.stage==0?.01f:1.e-5f):e.progress>=1;
    if(e.stage==(topology==2?2:1)&&done&&valid(r,curve,sustain,topology,division,bpm,sr)){e.stage=0;e.start=e.value;e.progress=0;}}
  }
  if(mode==2){double t=phase*total;int stage=0,count=cycle==2?3:2;while(stage<count-1&&t>=weights[stage]){t-=weights[stage];++stage;}
   double u=std::clamp(t/weights[stage],0.,1.),v=curve==2?u*(2-u):curve==1?-std::expm1(-4.60517*u)/(-std::expm1(-4.60517)):u;
   double a=stage==0?start:stage==1?1:sustain,b=stage==0?1:stage==1?sustain:0;e.stage=stage;e.start=a;e.progress=u;e.value=a+(b-a)*v;
  }else {int before=e.stage;absolute();if(topology==2&&before==1&&e.stage==1){bool done=curve==1?std::abs(e.value-sustain)<1.e-5f:e.progress>=1;if(done){e.stage=2;e.start=e.value;e.progress=0;}}}
  phase+=bpm/(60*sr*Beats[division]);if(phase>=1){phase-=std::floor(phase);pending=true;}return float(e.value);
 }
};
}
