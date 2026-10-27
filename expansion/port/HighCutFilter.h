// GPL-2.0-or-later. Calibrated bilinear/trapezoidal state-variable filter.
#pragma once
namespace xs::highcut {
// Historical weighting helper retained for offline audit references only.
inline float weight(float hz,float sr){return std::clamp((hz/sr-.08f)/.04f,0.f,1.f);}
inline float coefficient(float hz,float sr){return std::tan(float(3.14159265358979323846)*std::clamp(hz/sr,0.f,.49f));}
// Complete cascade half-power calibration. x is analog frequency / pole frequency.
// LP: |H|^(2m)=1/2 at x=r; HP uses reciprocal x. BP/NOTCH use x=1.
inline float halfPowerRatio(float damping,int stages){
 double b=double(damping)*damping-2, c=stages==1?1.:0.4142135623730950488;
 // Rationalized positive root avoids cancellation at damping=2.
 double root=std::sqrt(b*b+4*c);
 double square=b>=0?2*c/(root+b):(root-b)*.5;
 return float(std::sqrt(square));
}
inline float calibrated(float hz,float sr,float damping,int type,int stages){
 float g=coefficient(hz,sr);
 if(type==0)return g/halfPowerRatio(damping,stages);
 if(type==1)return g*halfPowerRatio(damping,stages);
 return g;
}
// Approved A: preserve the existing law through Q5; only the upper region
// acquires bounded autonomous resonance and a playable pole-frequency target.
inline float resonanceStrength(float nominal){return std::clamp((.2f-nominal)/.195f,0.f,1.f);}
inline float resonanceDamping(float nominal){float t=resonanceStrength(nominal);return t>0?.2f*(1-t)-.04f*t:nominal;}
inline float productionRatio(float nominal,int type,int stages){
 float ratio=type<2?halfPowerRatio(nominal,stages):1.f,t=resonanceStrength(nominal);
 return t>0?float(std::pow(double(ratio),1.-double(t))):ratio;
}
inline float productionCoefficient(float hz,float sr,float nominal,int type,int stages){
 float g=coefficient(hz,sr),ratio=productionRatio(nominal,type,stages);
 return type==0?g/ratio:type==1?g*ratio:g;
}
inline double radialAlpha(float g,float strength){double q=1.+double(g)*g;return 2*g*.04*strength/(.35*.35*q*q);}
inline void radialState(float *state,double alpha){
 double energy=double(state[0])*state[0]+double(state[1])*state[1];
 float contraction=float(1/std::sqrt(1+alpha*energy));state[0]*=contraction;state[1]*=contraction;
}
inline void stage(float input,float g,float damping,float *state,float *outputs){
 float band=(state[0]+g*(input-state[1]))/(1+g*(g+damping));
 float low=state[1]+g*band;
 state[0]=2*band-state[0];state[1]=2*low-state[1];
 float high=input-damping*band-low;
 outputs[0]=low;outputs[1]=high;outputs[2]=band;outputs[3]=low+high;
}
}
