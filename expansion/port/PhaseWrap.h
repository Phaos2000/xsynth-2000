// GPL-2.0-or-later. Exact identity fast path; all boundaries use original wrap.
#pragma once
#include <cmath>
#include <emmintrin.h>
namespace xs::phaseWrap {
inline float canonical(float v){return v>0&&v<1?v:v-std::floor(v);}
template<int N> inline bool interior(const float*v){
 static_assert(N==3||N==4,"member batch width");auto x=_mm_load_ps(v);
 if constexpr(N==3)x=_mm_and_ps(x,_mm_castsi128_ps(_mm_set_epi32(0,-1,-1,-1)));
 constexpr int mask=(1<<N)-1;
 return (_mm_movemask_ps(_mm_and_ps(_mm_cmpgt_ps(x,_mm_setzero_ps()),_mm_cmplt_ps(x,_mm_set1_ps(1.f))))&mask)==mask;
}
template<int N> inline void advance(float*v,bool*wrapped){
 if(interior<N>(v)){for(int i=0;i<N;++i)wrapped[i]=false;}
 else for(int i=0;i<N;++i){wrapped[i]=v[i]>=1||v[i]<0;v[i]=canonical(v[i]);}
}
template<int N> inline void position(float*v){if(!interior<N>(v))for(int i=0;i<N;++i)v[i]=canonical(v[i]);}
}
