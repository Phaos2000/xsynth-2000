// GPL-2.0-or-later. Fixed-storage, sample-clocked signal transitions.
#pragma once
#include <array>
#include <algorithm>
#include <cmath>
namespace xs {
// Re-target from the current mixture, including during rapid repeated edits.
// Partition of unity: this crossfades signals, never fades the synth to zero.
template<int N> struct SignalBlend {
  std::array<float,N> weights{},delta{};
  int target=-1,remaining=0;
  void reset(int k) {weights={};delta={};target=k;remaining=0;weights[k]=1;}
  void select(int k,int samples) {
    if(target<0){reset(k);return;}
    if(k==target)return;
    target=k;remaining=std::max(1,samples);
    for(int i=0;i<N;++i)delta[i]=((i==k?1.f:0.f)-weights[i])/remaining;
  }
  void advance(){if(remaining){for(int i=0;i<N;++i)weights[i]+=delta[i];if(!--remaining)reset(target);}}
};
inline int stableChoice(float normalized,int steps,int previous) {
  float p=std::clamp(normalized,0.f,1.f)*steps;
  if(previous<0)return int(std::round(p));
  // 0.08 of a cell beyond the ordinary half-cell boundary. No time/debounce.
  if(p>previous+.58f||p<previous-.58f)return int(std::round(p));
  return previous;
}
} // namespace xs
