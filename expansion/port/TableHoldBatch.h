// GPL-2.0-or-later. Exact HOLD preparation for the neutral-FM/PM path.
// Each member retains its own IEEE double phase and wrap history. Clocks and
// frame selection are prepared once per batch, independent of sync direction.
// No reciprocal approximation, interpolation change or horizontal reduction.
#pragma once
#include "TableBatch.h"
namespace xs::table {
struct HoldBatch {
 template<int N> struct Plan {Oscillator::Head**heads;const Table*table;FrameChoice frames;float position,gain;bool assigned=false;};
 template<int N> P8_INLINE static Plan<N> prepare(Oscillator::Head**heads,__m128 frequency,float position,double sr){
  alignas(16)float inputHz[4];_mm_store_ps(inputHz,frequency);double hz[4]{};
  const auto&selection=heads[0]->selection;const auto&t=*selection.data;double root=selection.root();
  for(int i=0;i<N;++i)hz[i]=selection.positionFrequency(inputHz[i],position);
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

  return {heads,&t,chooseFrames(t,position,selection.frame),position,selection.gain,false};
 }
 template<int N> P8_INLINE static __m128 read(Plan<N>&plan,int path,bool*wrapped,bool silent=false){
  static_assert(N==3||N==4);auto**h=plan.heads;const auto&t=*plan.table;
  auto a=_mm_add_pd(_mm_set_pd(h[1]->phase[path],h[0]->phase[path]),_mm_set_pd(h[1]->increment,h[0]->increment));
  auto b=_mm_add_pd(_mm_set_pd(N==4?h[3]->phase[path]:0,h[2]->phase[path]),_mm_set_pd(N==4?h[3]->increment:0,h[2]->increment));
  auto valid=[&](__m128d x){return _mm_movemask_pd(_mm_and_pd(_mm_cmpge_pd(x,_mm_setzero_pd()),_mm_cmplt_pd(x,_mm_set1_pd(1.))))==3;};
  alignas(16)int indices[4]{};
  if(valid(a)&&valid(b)){
   _mm_storel_pd(&h[0]->phase[path],a);_mm_storeh_pd(&h[1]->phase[path],a);_mm_storel_pd(&h[2]->phase[path],b);if constexpr(N==4)_mm_storeh_pd(&h[3]->phase[path],b);
   for(int i=0;i<N;++i)wrapped[i]=false;
   if(silent)return _mm_setzero_ps();
   auto ix=_mm_unpacklo_epi64(_mm_cvttpd_epi32(_mm_mul_pd(a,_mm_set1_pd(t.length))),_mm_cvttpd_epi32(_mm_mul_pd(b,_mm_set1_pd(t.length))));
   auto maximum=_mm_set1_epi32(t.length-1),mask=_mm_cmpgt_epi32(ix,maximum);
   ix=_mm_or_si128(_mm_and_si128(mask,maximum),_mm_andnot_si128(mask,ix));_mm_store_si128(reinterpret_cast<__m128i*>(indices),ix);
  }else{
   for(int i=0;i<N;++i){double advanced=h[i]->phase[path]+h[i]->increment;wrapped[i]=advanced>=1||advanced<0;h[i]->phase[path]=MemberTableBatch::wrapAdvanced(advanced);if(!silent)indices[i]=std::min(t.length-1,int(h[i]->phase[path]*t.length));}
   if(silent)return _mm_setzero_ps();
  }
  if(!plan.assigned){for(int i=0;i<N;++i){h[i]->frames=plan.frames;h[i]->lastPosition=plan.position;}plan.assigned=true;}
  auto gather=[&](int frame){const float*p=t.pcm.data()+size_t(frame)*t.length;if constexpr(N==4)return _mm_set_ps(p[indices[3]],p[indices[2]],p[indices[1]],p[indices[0]]);else return _mm_set_ps(0,p[indices[2]],p[indices[1]],p[indices[0]]);};
  auto result=gather(plan.frames.a);
  if(plan.frames.blend!=0&&plan.frames.a!=plan.frames.b)result=_mm_add_ps(result,_mm_mul_ps(_mm_set1_ps(plan.frames.blend),_mm_sub_ps(gather(plan.frames.b),result)));
  return _mm_mul_ps(result,_mm_set1_ps(plan.gain));
 }

};
}
