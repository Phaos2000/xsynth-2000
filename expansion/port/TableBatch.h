// GPL-2.0-or-later. Exact SSE2 member table lanes; independent clocks and histories.
#pragma once
#include "TableOscillator.h"
#include <emmintrin.h>
#if defined(_MSC_VER)
#define XS_MEMBER_VECTORCALL __vectorcall
#else
#define XS_MEMBER_VECTORCALL
#endif
namespace xs::table {
struct MemberTableBatch {
  static __m128 select(__m128 mask,__m128 yes,__m128 no){return _mm_or_ps(_mm_and_ps(mask,yes),_mm_andnot_ps(mask,no));}
  static double wrapAdvanced(double p){return p>=0&&p<1?p:wrap(p);}
  template<int N,bool Right=false> P8_INLINE static __m128 gather(const float*data,const Address*a){
    auto at=[&](int i){return data[Right?a[i].b:a[i].a];};
    if constexpr(N==4)return _mm_set_ps(at(3),at(2),at(1),at(0));else return _mm_set_ps(0,at(2),at(1),at(0));
  }
  template<int N> P8_INLINE static __m128 fractions(const Address*a){if constexpr(N==4)return _mm_set_ps(a[3].fraction,a[2].fraction,a[1].fraction,a[0].fraction);else return _mm_set_ps(0,a[2].fraction,a[1].fraction,a[0].fraction);}
  template<int N> P8_INLINE static __m128 linearFrame(const float*data,int length,int frame,const Address*a,__m128 fraction){
    const float*p=data+size_t(frame)*length;auto left=gather<N>(p,a);return _mm_add_ps(left,_mm_mul_ps(fraction,_mm_sub_ps(gather<N,true>(p,a),left)));
  }
  template<int N> static __m128 pack(const float*values){if constexpr(N==4)return _mm_loadu_ps(values);else return _mm_set_ps(0,values[2],values[1],values[0]);}
  template<int N> static void unpack(__m128 x,float*values){alignas(16)float v[4];_mm_store_ps(v,x);for(int i=0;i<N;++i)values[i]=v[i];}
  template<int N> static __m128 sample(const float**data,const Address*addresses,const bool*point){
    alignas(16)float a[4]{},b[4]{},f[4]{},bits[4]{};
    for(int i=0;i<N;++i){a[i]=data[i][addresses[i].a];if(point[i]){b[i]=a[i];uint32_t mask=~0u;std::memcpy(bits+i,&mask,4);}else{b[i]=data[i][addresses[i].b];f[i]=addresses[i].fraction;}}
    auto left=_mm_load_ps(a);auto value=_mm_add_ps(left,_mm_mul_ps(_mm_load_ps(f),_mm_sub_ps(_mm_load_ps(b),left)));
    return select(_mm_load_ps(bits),left,value);
  }
  template<int N> static __m128 XS_MEMBER_VECTORCALL read(Oscillator::Head**heads,int path,__m128 frequency,__m128 phaseMod,float position,double sr,bool*wrapped,const Oscillator::Head*prepared,bool silent=false){
    alignas(16)float inputHz[4]{},pm[4]{};_mm_store_ps(inputHz,frequency);_mm_store_ps(pm,phaseMod);double hz[4]{};
    const auto&selection=heads[0]->selection;const auto&t=*selection.data;
    for(int i=0;i<N;++i)hz[i]=selection.positionFrequency(inputHz[i],position);
    double root=selection.root();
    // Two independent IEEE double divisions per pair, never a reciprocal.
    for(int i=0;i<N;i+=2){bool second=i+1<N;auto&a=*heads[i];
      bool change=hz[i]!=a.lastRootHz||sr!=a.lastClockSr;
      bool changeB=second&&(hz[i+1]!=heads[i+1]->lastRootHz||sr!=heads[i+1]->lastClockSr);
      if(change&&changeB){
        auto f=_mm_div_pd(_mm_set_pd(hz[i+1],hz[i]),_mm_set1_pd(root));auto step=_mm_div_pd(f,_mm_set1_pd(sr));
        alignas(16)double cy[2],delta[2];_mm_store_pd(cy,f);_mm_store_pd(delta,step);
        for(int j=0;j<2;++j){auto&h=*heads[i+j];h.cycleHz=cy[j];h.increment=delta[j];h.lastRootHz=hz[i+j];h.lastClockSr=sr;}
      }else for(int j=i;j<std::min(N,i+2);++j){auto&h=*heads[j];if(hz[j]!=h.lastRootHz||sr!=h.lastClockSr){h.cycleHz=hz[j]/root;h.increment=h.cycleHz/sr;h.lastRootHz=hz[j];h.lastClockSr=sr;}}
    }
    double addressed[N];
    for(int i=0;i<N;++i){auto&h=*heads[i];double advanced=h.phase[path]+h.increment;wrapped[i]=advanced>=1||advanced<0;h.phase[path]=wrapAdvanced(advanced);addressed[i]=h.phase[path]+pm[i];}
    if(silent)return _mm_setzero_ps();
    // A frame choice depends on note position and table metadata, not phase.
    FrameChoice frames=chooseFrames(t,position,selection.frame);
    for(int i=0;i<N;++i){auto&h=*heads[i];h.frames=frames;h.lastPosition=position;addressed[i]=wrapAdvanced(addressed[i]);
      if(selection.sample==Sample::Band&&(h.cycleHz!=h.lastHz||sr!=h.lastSr)){
        if(prepared&&prepared->lastHz==h.cycleHz&&prepared->lastSr==sr&&prepared->selection.data==&t)h.band=prepared->band;
        else h.band=chooseBand(t,h.cycleHz,sr);
        h.lastHz=h.cycleHz;h.lastSr=sr;
      }
    }
    __m128 result;
    if(selection.sample!=Sample::Band){
      Address a[N];for(int i=0;i<N;++i)a[i]=address(addressed[i],t.length);
      if(selection.sample==Sample::Hold){result=gather<N>(t.pcm.data()+size_t(frames.a)*t.length,a);if(frames.blend!=0&&frames.a!=frames.b)result=_mm_add_ps(result,_mm_mul_ps(_mm_set1_ps(frames.blend),_mm_sub_ps(gather<N>(t.pcm.data()+size_t(frames.b)*t.length,a),result)));}
      else {auto f=fractions<N>(a);result=linearFrame<N>(t.pcm.data(),t.length,frames.a,a,f);if(frames.blend!=0&&frames.a!=frames.b)result=_mm_add_ps(result,_mm_mul_ps(_mm_set1_ps(frames.blend),_mm_sub_ps(linearFrame<N>(t.pcm.data(),t.length,frames.b,a,f),result)));}
    }else{
      auto&first=heads[0]->band;bool common=first.high&&first.blend!=0&&first.high->length>1&&first.low->length>1;
      for(int i=1;i<N&&common;++i)common=heads[i]->band.high==first.high&&heads[i]->band.low==first.low&&heads[i]->band.blend!=0;
      if(common){
        Address lo[N],hi[N];alignas(16)float weights[4]{};
        for(int i=0;i<N;++i){lo[i]=address(addressed[i],first.low->length);hi[i]=first.high->length==first.low->length?lo[i]:address(addressed[i],first.high->length);weights[i]=heads[i]->band.blend;}
        auto lf=fractions<N>(lo),hf=fractions<N>(hi),w=_mm_load_ps(weights);
        auto frame=[&](int f){auto l=linearFrame<N>(first.low->samples.data(),first.low->length,f,lo,lf);auto h=linearFrame<N>(first.high->samples.data(),first.high->length,f,hi,hf);return _mm_add_ps(l,_mm_mul_ps(w,_mm_sub_ps(h,l)));};
        result=frame(frames.a);if(frames.blend!=0&&frames.a!=frames.b)result=_mm_add_ps(result,_mm_mul_ps(_mm_set1_ps(frames.blend),_mm_sub_ps(frame(frames.b),result)));
      }else{
        alignas(16)float values[4]{};for(int i=0;i<N;++i)values[i]=planned(t,frames,plan(t,addressed[i],selection.sample,heads[i]->band));result=_mm_load_ps(values);
      }
    }
    return _mm_mul_ps(result,_mm_set1_ps(selection.gain));
  }
};
}
