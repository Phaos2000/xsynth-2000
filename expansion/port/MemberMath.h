// GPL-2.0-or-later. Exact SSE2 waveform/correction lanes; included privately inside Engine.
  P8_INLINE static __m128 memberSelect(__m128 mask,__m128 yes,__m128 no){return _mm_or_ps(_mm_and_ps(mask,yes),_mm_andnot_ps(mask,no));}
  P8_INLINE static __m128 memberUnitOffset(__m128 p,float offset){
    auto q=_mm_sub_ps(p,_mm_set1_ps(offset)),zero=_mm_setzero_ps();
    auto result=memberSelect(_mm_cmplt_ps(q,zero),_mm_add_ps(q,_mm_set1_ps(1.f)),q);
    return memberSelect(_mm_cmpeq_ps(q,zero),zero,result);
  }
  P8_INLINE static __m128 memberBlep(__m128 p,__m128 width){
    auto zero=_mm_setzero_ps(),one=_mm_set1_ps(1.f);auto valid=_mm_cmpgt_ps(width,zero);
    auto early=_mm_and_ps(valid,_mm_cmplt_ps(p,width));
    auto late=_mm_andnot_ps(early,_mm_and_ps(valid,_mm_cmpgt_ps(p,_mm_sub_ps(one,width))));
    __m128 a=zero,b=zero;
    if(_mm_movemask_ps(early)){auto t=_mm_div_ps(p,memberSelect(early,width,one));a=_mm_sub_ps(_mm_sub_ps(_mm_add_ps(t,t),_mm_mul_ps(t,t)),one);}
    if(_mm_movemask_ps(late)){auto t=_mm_div_ps(_mm_sub_ps(p,one),memberSelect(late,width,one));b=_mm_add_ps(_mm_add_ps(_mm_add_ps(_mm_mul_ps(t,t),t),t),one);}
    return memberSelect(early,a,memberSelect(late,b,zero));
  }
  P8_INLINE static __m128 memberBlamp(__m128 p,__m128 width){
    auto zero=_mm_setzero_ps(),one=_mm_set1_ps(1.f),distance=_mm_min_ps(p,_mm_sub_ps(one,p));
    auto mask=_mm_and_ps(_mm_cmpgt_ps(width,zero),_mm_cmplt_ps(distance,width));
    if(!_mm_movemask_ps(mask))return zero;
    auto t=_mm_sub_ps(one,_mm_div_ps(distance,memberSelect(mask,width,one)));
    auto value=_mm_mul_ps(_mm_mul_ps(_mm_mul_ps(_mm_mul_ps(width,_mm_set1_ps(1.f/3)),t),t),t);
    return memberSelect(mask,value,zero);
  }
  template<int N> P8_INLINE static __m128 memberClean(__m128 phase,__m128 increment,int wave,float shape,__m128 raw){
    if(wave==0)return raw;
    alignas(16)float d[4]{},widths[4]{};_mm_store_ps(d,increment);
    for(int i=0;i<N;++i){float shifted=d[i]+.5f;widths[i]=shifted>=0&&shifted<1?std::abs(d[i]):std::abs(d[i]-std::floor(shifted));}
    auto width=_mm_load_ps(widths);
    if(wave==1)return _mm_add_ps(_mm_sub_ps(raw,_mm_mul_ps(_mm_set1_ps(4.f),memberBlamp(memberUnitOffset(phase,.25f),width))),_mm_mul_ps(_mm_set1_ps(4.f),memberBlamp(memberUnitOffset(phase,.75f),width)));
    if(wave==2)return _mm_sub_ps(raw,memberBlep(phase,width));
    if(wave==3)return _mm_add_ps(raw,memberBlep(phase,width));
    float duty=wave==4?.5f:oscillator::pulseDuty(shape);
    return _mm_sub_ps(_mm_add_ps(raw,memberBlep(phase,width)),memberBlep(memberUnitOffset(phase,duty),width));
  }
  template<int N> P8_INLINE static __m128 memberSine(__m128 phase){
    auto position=_mm_mul_ps(phase,_mm_set1_ps(100.f));
    alignas(16)float x[4]{},a[4]{},b[4]{},index[4]{};_mm_store_ps(x,position);
    const float*table=LegacyEngine::sineTable();
    for(int lane=0;lane<N;++lane){int i=std::min(99,int(x[lane]));index[lane]=float(i);a[lane]=table[i];b[lane]=table[i+1];}
    auto first=_mm_load_ps(a);return _mm_add_ps(first,_mm_mul_ps(_mm_sub_ps(position,_mm_load_ps(index)),_mm_sub_ps(_mm_load_ps(b),first)));
  }
