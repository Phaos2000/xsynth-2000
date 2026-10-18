// GPL-2.0-or-later. Bilinear/trapezoidal state-variable high-frequency path.
#pragma once
namespace xs::highcut {
inline float weight(float hz,float sr){return std::clamp((hz/sr-.08f)/.04f,0.f,1.f);}
inline float coefficient(float hz,float sr){return std::tan(float(3.14159265358979323846)*std::clamp(hz/sr,0.f,.49f));}
inline void stage(float input,float g,float damping,float *state,float *outputs){
 float band=(state[0]+g*(input-state[1]))/(1+g*(g+damping));
 float low=state[1]+g*band;
 state[0]=2*band-state[0];state[1]=2*low-state[1];
 float high=input-damping*band-low;
 outputs[0]=low;outputs[1]=high;outputs[2]=band;outputs[3]=low+high;
}
}
