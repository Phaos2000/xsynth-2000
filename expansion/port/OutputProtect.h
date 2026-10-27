// GPL-2.0-or-later. Causal emergency-protection engineering core.
// Production output stage; constant latency, continuously warm linked detector.
#pragma once
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <xmmintrin.h>
namespace xs::protect {
template<int Capacity> struct Maximum {
 std::array<double,Capacity> value{};std::array<int64_t,Capacity> time{};
 int head=0,tail=0;
 void reset(){head=tail=0;}
 void push(int64_t t,double v,int window){
  while(head!=tail&&time[head]<t-window){if(++head==Capacity)head=0;}
  while(head!=tail){int prev=tail?tail-1:Capacity-1;if(value[prev]>v)break;tail=prev;}
  value[tail]=v;time[tail]=t;if(++tail==Capacity)tail=0;
 }
 double get()const{return head==tail?0:value[head];}
};
struct Engine {
 static constexpr int Taps=64,DetectorDelay=32,MaxLookahead=768,AudioCapacity=1024;
 alignas(16) float coefficients[3][Taps]{};
 std::array<float,128> left{},right{};
 std::array<float,AudioCapacity> audioL{},audioR{};
 Maximum<128> inputMaximum;
 Maximum<1024> predictedMaximum;
 int lookahead=48,latency=80;int64_t clock=0;
 double ceiling=std::pow(10.,-1./20),gain=1,recovery=0,linearStep=0;
 double coefficientBound=1,previousInterval=0,lastPredicted=0;
 bool linearRecovery=false;uint64_t reconstructedFrames=0;
 int switchSamples=96,switchRemaining=0;
 bool requestedEnabled=true;
 double switchMix=1,switchStart=1;
 static double bessel(double x){double sum=1,term=1;for(int k=1;k<40;++k){term*=x*x/(4*k*k);sum+=term;if(term<sum*1e-16)break;}return sum;}
 void prepare(double rate,double lookMs=1,double releaseMs=20,double db=-1,bool linear=false){
  lookahead=std::clamp(int(std::round(rate*lookMs*.001)),1,MaxLookahead);latency=DetectorDelay+lookahead;
  switchSamples=std::max(1,int(std::round(rate*.002)));
  ceiling=std::pow(10.,db/20);recovery=std::exp(-1/(rate*releaseMs*.001));linearStep=1/(rate*releaseMs*.001);linearRecovery=linear;
  coefficientBound=1;
  for(int p=0;p<3;++p){double sum=0,w[Taps]{};for(int k=0;k<Taps;++k){double tap=k-31,phase=(p+1)*.25,z=phase-tap,u=(tap-phase)/32;
    w[k]=(z==0?1:std::sin(3.141592653589793*z)/(3.141592653589793*z))*bessel(9*std::sqrt(std::max(0.,1-u*u)))/bessel(9);sum+=w[k];}
   double bound=0;for(int k=0;k<Taps;++k){coefficients[p][k]=float(w[k]/sum);bound+=std::abs(double(coefficients[p][k]));}coefficientBound=std::max(coefficientBound,bound);}
  reset();
 }
 void reset(){left.fill(0);right.fill(0);audioL.fill(0);audioR.fill(0);inputMaximum.reset();predictedMaximum.reset();clock=0;gain=1;previousInterval=lastPredicted=0;reconstructedFrames=0;switchRemaining=0;requestedEnabled=true;switchMix=switchStart=1;}
 static float sum4(__m128 a){__m128 b=_mm_add_ps(a,_mm_movehl_ps(a,a));b=_mm_add_ss(b,_mm_shuffle_ps(b,b,1));return _mm_cvtss_f32(b);}
 double reconstructed(int start,double maxInput){
  double peak=0;
  // Huge finite floats use double accumulation rather than overflowing SIMD.
  if(maxInput>1e20){for(int p=0;p<3;++p){double l=0,r=0;for(int k=0;k<Taps;++k){l+=double(left[start+k])*coefficients[p][k];r+=double(right[start+k])*coefficients[p][k];}peak=std::max({peak,std::abs(l),std::abs(r)});}return peak;}
  for(int p=0;p<3;++p){__m128 l=_mm_setzero_ps(),r=l;for(int k=0;k<Taps;k+=4){auto c=_mm_load_ps(coefficients[p]+k);l=_mm_add_ps(l,_mm_mul_ps(c,_mm_loadu_ps(left.data()+start+k)));r=_mm_add_ps(r,_mm_mul_ps(c,_mm_loadu_ps(right.data()+start+k)));}peak=std::max({peak,double(std::abs(sum4(l))),double(std::abs(sum4(r)))});}return peak;
 }
 // No allocation, locking, filesystem access or coefficient preparation here.
 void sample(float inL,float inR,float&outL,float&outR,bool enabled=true,bool delayedOff=true){
  int slot=int(clock&63);left[slot]=left[slot+64]=inL;right[slot]=right[slot+64]=inR;
  int audio=int(clock&(AudioCapacity-1));audioL[audio]=inL;audioR[audio]=inR;
  if(!enabled&&!delayedOff){outL=inL;outR=inR;++clock;return;}
  double inputPeak=std::max(std::abs(double(inL)),std::abs(double(inR)));
  inputMaximum.push(clock,inputPeak,63);
  {
   int start=(slot+1)&63;double interval=0;
   // This is a rigorous triangle bound on this finite FIR, not a heuristic gate.
   if(inputMaximum.get()*coefficientBound>ceiling){interval=reconstructed(start,inputMaximum.get());++reconstructedFrames;}
   int center=int((clock-DetectorDelay)&(AudioCapacity-1));
   double raw=std::max(std::abs(double(audioL[center])),std::abs(double(audioR[center])));
   predictedMaximum.push(clock,std::max({interval,previousInterval,raw}),lookahead);previousInterval=interval;
   lastPredicted=predictedMaximum.get();double required=lastPredicted>ceiling?ceiling/lastPredicted:1;
   // Full-scale bounded linear attack reaches any required gain within lookahead.
   // Window includes each predicted peak through the time its audio is emitted.
   if(required<gain)gain=std::max(required,gain-1./lookahead);
   else gain=std::min(required,linearRecovery?gain+linearStep:1-(1-gain)*recovery);
   if(gain>1-1e-7)gain=1;
  }
  int delayed=int((clock-latency)&(AudioCapacity-1));
  // Same delayed signal in both modes. A 2ms cubic crossfade removes the
  // abrupt control-gain step; rapid reversals continue from the current mix.
  // Fresh/reset OFF starts at unity before any delayed audio can emerge.
  if(clock==0){requestedEnabled=enabled;switchMix=enabled?1.:0.;}
  if(enabled!=requestedEnabled){requestedEnabled=enabled;switchStart=switchMix;switchRemaining=switchSamples;}
  if(switchRemaining){double t=double(switchSamples-switchRemaining+1)/switchSamples;
   switchMix=switchStart+((requestedEnabled?1.:0.)-switchStart)*t*t*(3-2*t);
   if(!--switchRemaining)switchMix=requestedEnabled?1.:0.;}
  const double applied=switchMix==1?gain:switchMix==0?1.:1+switchMix*(gain-1);
  outL=float(double(audioL[delayed])*applied);outR=float(double(audioR[delayed])*applied);++clock;
 }
};
}
