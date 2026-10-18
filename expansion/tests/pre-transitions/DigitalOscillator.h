// GPL-2.0-or-later. Optional local polynomial correction; never applied to RAW.
#pragma once
#include <algorithm>
#include <cmath>
namespace xs::oscillator {
inline float wrap(float phase) { return phase-std::floor(phase); }
inline float blep(float phase,float width) {
  if(width<=0)return 0;
  if(phase<width){float t=phase/width;return t+t-t*t-1;}
  if(phase>1-width){float t=(phase-1)/width;return t*t+t+t+1;}
  return 0;
}
// Integral of the two-sided quadratic BLEP residual, a cubic corner correction.
// See the BLAMP approach: Esqueda/Valimaki/Bilbao, DAFx 2016. This is the compact
// two-sided cubic, not a claim to reproduce that paper's four-point algorithm.
inline float blamp(float phase,float width) {
  if(width<=0)return 0;
  float distance=std::min(phase,1-phase);
  if(distance>=width)return 0;
  float t=1-distance/width;return width*(1.f/3)*t*t*t;
}
inline float clean(float phase,float increment,int kind,float shape,float raw) {
  // Keep actual phase/frequency. The correction width follows its sampled fold,
  // rather than clamping the oscillator's frequency below Nyquist.
  float dt=std::abs(increment-std::floor(increment+.5f));
  if(kind==1)return raw-4*blamp(wrap(phase-.25f),dt)+4*blamp(wrap(phase-.75f),dt);
  if(kind==2)return raw-blep(phase,dt);
  if(kind==3)return raw+blep(phase,dt);
  if(kind>=4) {
    shape=kind==4?.5f:std::clamp(shape,0.f,1.f);
    if(shape<=0)return -1;
    if(shape>=1)return 1;
    return raw+blep(phase,dt)-blep(wrap(phase-shape),dt);
  }
  return raw;
}
} // namespace xs::oscillator
