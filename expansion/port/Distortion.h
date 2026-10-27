// GPL-2.0-or-later. Original ten-mode 1x distortion, no oversampling/limiter.
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <memory>
namespace xs::distortion {
inline constexpr double Pi=3.14159265358979323846;
enum Mode {Off,SoftClip,SineFold,Harmonic,RectifyDC,Sub,FeedbackFold,Comb,Wrap,Rate,Sculpt,ModeCount};
inline constexpr const char* Names[]={"OFF","SOFT CLIP","SINE FOLD","HARMONIC","RECTIFY DC","SUB","FEEDBACK FOLD","COMB","WRAP","RATE","SCULPT"};
inline double zap(double x){return std::abs(x)<1e-20?0:x;}
inline double blend(double a,double b,double m){return m==0?a:m==1?b:a+(b-a)*m;}
struct History {
 double previous=0,phase=0,held=0,dcX=0,dcY=0;int position=0;bool armed=false,negative=false;
 std::array<float,768> delay{};
 void clear(int mode){previous=phase=held=dcX=dcY=0;position=0;armed=negative=false;if(mode==Comb)delay.fill(0);}
};
struct Configuration {
 int mode=Off;double drive=0,pre=1,k=Pi*.5,hold=1,delay=100,dcPole=0,combPole=0;
 double preparedDrive=-1;int preparedMode=-1;double sr=0;
 void prepare(int m,double d,double fs){
  if(m==preparedMode&&d==preparedDrive&&fs==sr)return;
  if(fs!=sr){dcPole=std::exp(-2*Pi*5/fs);combPole=std::exp(-2*Pi/fs);delay=std::clamp(fs/(160*std::pow(15.,.5)),2.,765.);}
  mode=m;drive=d;
  if(m==SoftClip||m==Wrap)pre=std::pow(16.,d);
  if(m==RectifyDC||m==Sub)pre=d>1?std::pow(4.,d-1):1;
  if(m==SineFold||m==FeedbackFold)k=Pi*.5*(1+10*d);
  if(m==Rate)hold=std::pow(2.,-7*d);
  sr=fs;preparedDrive=d;preparedMode=m;
 }
};
inline double delayed(const History&s,double delay){double q=s.position-delay;while(q<0)q+=768;int a=int(q),b=(a+1)%768;return s.delay[a]+(s.delay[b]-s.delay[a])*(q-a);}
inline double sculpt(double x){
 constexpr double knots[]={0,.25,.65,1},values[]={0,.45,-.45,.7},slopes[]={3,0,0,0};
 double u=std::min(1.,std::abs(x));int i=u<.25?0:u<.65?1:2;double h=knots[i+1]-knots[i],t=(u-knots[i])/h;
 double y=(2*t*t*t-3*t*t+1)*values[i]+(t*t*t-2*t*t+t)*h*slopes[i]+(-2*t*t*t+3*t*t)*values[i+1]+(t*t*t-t*t)*h*slopes[i+1];
 return std::copysign(1.,x)*y;
}
inline double process(double input,const Configuration&p,History&s){
 if(!std::isfinite(input)){s.clear(p.mode);return 0;}
 if(p.mode==Off)return input;
 double d=p.drive;
 if(d==0){if(p.mode==Comb){s.dcY=zap(-(1+p.combPole)*.5*s.dcX+p.combPole*s.dcY);s.dcX=0;}return input;}
 double x=std::clamp(input,-32.,32.),y=x;
 // The old0..1 domain is untouched. Above100%, the two identity/blend
 // modes gain a continuous pre-overdrive; their core identity stays full.
 const double color=d>1?1:d;
 if(d>1&&(p.mode==RectifyDC||p.mode==Sub)){
   double u=d-1,z=x*p.pre;x=blend(x,z/std::sqrt(1+z*z),u);y=x;
 }

 switch(p.mode){
 case SoftClip:{double z=x*p.pre;y=z/std::sqrt(1+z*z);break;}
 case SineFold:y=std::sin(p.k*x);break;
 case Harmonic:{double u=std::clamp(x*(1+3*d),-1.,1.);double t3=4*u*u*u-3*u,t5=16*std::pow(u,5)-20*u*u*u+5*u;y=.5*t3+.5*t5;break;}
 case RectifyDC:{double a=std::min(1.,2*color);y=color<.5?(1-a)*x+a*std::max(0.,x):(2-2*color)*std::max(0.,x)+(2*color-1)*std::abs(x);double q=y-s.dcX+p.dcPole*s.dcY;s.dcX=y;s.dcY=zap(q);y=q;break;}
 case Sub:if(x<-.015)s.armed=true;if(s.armed&&x>.015){s.negative=!s.negative;s.armed=false;}y=(s.negative?-1:1)*std::abs(x);break;
 case FeedbackFold:y=std::sin(p.k*x+.5*s.previous);s.previous=zap(y);break;
 case Comb:{double core=std::sin((1+5*d)*x+(.15+.65*d)*delayed(s,p.delay));s.delay[s.position]=float(zap(core));if(++s.position==768)s.position=0;
  // Explicit COMB-only 1Hz core-output DC removal; raw feedback is unchanged.
  double q=(1+p.combPole)*.5*(core-s.dcX)+p.combPole*s.dcY;s.dcX=core;s.dcY=zap(q);y=q;break;}
 case Wrap:{double z=x*p.pre;y=z-2*std::floor((z+1)/2);break;}
 case Rate:s.phase+=p.hold;if(s.phase>=1){s.phase-=1;s.held=x;}y=s.held;break;
 case Sculpt:y=sculpt(x*(1+6*d));break;
 default:break;
 }
 double out=x+(y-x)*color;if(!std::isfinite(out)){s.clear(p.mode);return 0;}return zap(out);
}
struct Ramp {
 double value=0,target=0,step=0;int remaining=0;
 void reset(double n){value=target=n;step=0;remaining=0;}
 void set(double n,int duration){if(n==target)return;target=n;remaining=std::max(1,duration);step=(target-value)/remaining;}
 double advance(){if(remaining){if(!--remaining)value=target;else value+=step;}return value;}
};
// Controls and coefficient preparation run once per ENGINE sample, not once
// per physical Unison member. Every member keeps independent signal history.
struct Control {
 Ramp drive,mix,placementGain;Configuration config[2];
 int mode[2]={Off,Off},active=0,pending=Off,fadeRemaining=0,fadeTotal=1;
 int place=1,requestedPlace=1,placementPhase=0,rampSamples=240,fadeSamples=96;
 unsigned generation[2]={1,1};double sr=48000,weight=0,effectiveMix=0;
 void reset(double fs,int m,double d,double wet,int location){
  sr=fs;rampSamples=std::max(1,int(fs*.005));fadeSamples=std::max(2,int(fs*.002));
  mode[0]=mode[1]=pending=m;active=0;fadeRemaining=0;fadeTotal=1;generation[0]=generation[1]=1;
  place=requestedPlace=location;placementPhase=0;placementGain.reset(1);drive.reset(d);mix.reset(wet);weight=0;effectiveMix=wet;
  for(auto&p:config){p=Configuration{};p.prepare(m,d,fs);}
 }
 void request(int m,double d,double wet,int location){pending=m;drive.set(d,rampSamples);mix.set(wet,rampSamples);requestedPlace=location;}
 void advance(){
  double d=drive.advance();mix.advance();
  if(placementPhase==0&&requestedPlace!=place){placementPhase=1;placementGain.set(0,rampSamples);}
  placementGain.advance();
  if(placementPhase==1&&!placementGain.remaining){place=requestedPlace;for(auto&g:generation)if(!++g)++g;placementPhase=2;placementGain.set(1,rampSamples);}
  else if(placementPhase==2&&!placementGain.remaining)placementPhase=0;
  if(!fadeRemaining&&pending!=mode[active]){int next=1-active;mode[next]=pending;if(!++generation[next])++generation[next];fadeRemaining=fadeTotal=fadeSamples;}
  weight=fadeRemaining?double(fadeTotal-fadeRemaining+1)/fadeTotal:0;
  for(int i=0;i<2;++i)config[i].prepare(mode[i],d,sr);
  effectiveMix=mix.value*placementGain.value;
 }
 // Called once after all primary/topology/retirement members for this frame.
 void finish(){if(fadeRemaining&&!--fadeRemaining)active=1-active;}
 bool live()const{return mode[active]!=Off||fadeRemaining!=0||pending!=Off;}
};
struct VoiceState {
 History history[2];unsigned generation[2]{};
 void clear(){generation[0]=generation[1]=0;}
 double sample(double input,const Control&c){
  auto one=[&](int i){if(generation[i]!=c.generation[i]){history[i].clear(c.mode[i]);generation[i]=c.generation[i];}return process(input,c.config[i],history[i]);};
  double wet=one(c.active);if(c.fadeRemaining)wet=blend(wet,one(1-c.active),c.weight);
  return blend(input,wet,c.effectiveMix);
 }
};
template<size_t N>struct Storage {
 std::unique_ptr<std::array<VoiceState,N>> data=std::make_unique<std::array<VoiceState,N>>();
 Storage()=default;
 Storage(const Storage&other):data(std::make_unique<std::array<VoiceState,N>>(*other.data)){}
 Storage&operator=(const Storage&other){if(this!=&other)*data=*other.data;return *this;}
 auto&operator*(){return *data;}const auto&operator*()const{return *data;}
};
}
