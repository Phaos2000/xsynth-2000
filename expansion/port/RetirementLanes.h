// GPL-2.0-or-later. Exact coherent outgoing-member lanes; no new fade policy.
  bool retirementLanesEnabled=true;
  unsigned retirementLaneFrames=0;
  static bool retiredModesReady(const Voice&v){
    if(v.firstSample||v.retiring||v.topology.remaining||v.cross.remaining||v.mix.remaining||v.types.remaining||v.slope.remaining)return false;
    for(int o=0;o<2;++o)if(v.waves[o].remaining||v.engines[o].remaining)return false;
    return true;
  }
  bool retiredBatchReady(const RetireBatch&r,const Voice&note)const{
    if(!note.active||note.topology.target<0||note.types.target<0||note.slope.target<0||note.mix.target<0)return false;
    // Preserve the scalar leader's fault recovery if its histories are outside
    // a conservative, finite operating region. This never truncates a tail or
    // changes a sample: it selects the original scalar path in those cases.
    auto magnitude=[](__m128 x){return _mm_and_ps(x,_mm_castsi128_ps(_mm_set1_epi32(0x7fffffff)));};
    if(_mm_movemask_ps(_mm_cmplt_ps(magnitude(_mm_loadu_ps(note.highFilter)),_mm_set1_ps(1.e10f)))!=15)return false;
    auto coefficients=_mm_set_ps(0,note.highG,note.damping,0);
    if(_mm_movemask_ps(_mm_and_ps(_mm_cmpge_ps(coefficients,_mm_set_ps(0,0,.005f,0)),_mm_cmple_ps(coefficients,_mm_set_ps(1,128,2,1))))!=15)return false;
    auto steps=_mm_set_ps(0,note.highStep,note.dampingStep,0);
    if(_mm_movemask_ps(_mm_cmplt_ps(magnitude(steps),_mm_set1_ps(1024.f)))!=15)return false;
    for(int o=0;o<2;++o)if(sourceKind[o]&&!sourceSelection[o].data)return false;
    for(int i=0;i<r.count;++i){const auto&m=r.voices[i];if(!m.active||m.group!=note.group)return false;
      for(int o=0;o<2;++o){const auto&r=m.tables[o];if(sourceKind[o]){if(!r.ready||r.active||m.tableGeneration[o]!=sourceSelectionGeneration)return false;}else if(r.ready)return false;}
    }return true;
  }
  void advanceRetiredNote(Voice&v,float*env){
    v.hz=v.glide?v.hz+v.glideRate*(v.targetHz-v.hz):v.targetHz;
    if(sampleControl||v.modSmoothing||v.control==0)control(v,lfoValue);
    for(int o=0;o<2;++o){v.waves[o].advance();v.engines[o].advance();}
    v.cross.advance();v.topology.advance();v.mix.advance();v.types.advance();v.slope.advance();advanceSecondary(v);
    for(int e=0;e<2;++e){v.pitchRatio[e]+=v.pitchStep[e];for(int j=0;j<3;++j)v.envelopeRate[e][j]+=v.envelopeStep[e][j];}
    v.control=(v.control+1)&7;
    env[0]=envelope(v,0);env[1]=envelope(v,1);
    v.damping=std::clamp(v.damping+v.dampingStep,.005f,2.f);
    v.highG=std::max(0.f,v.highG+v.highStep);
  }
  template<int N,bool Ready=false> bool renderRetiredBatch(int first,Voice&note,float*output,Voice*external,const float*env){
    // noteShared excludes discrete modulation and unfinished mode blends.
    // A ready, inactive table head carrying the current generation has fully
    // accepted that generation's immutable selection. No field-by-field
    // selection comparison or follower mode comparison is needed per sample.
    if constexpr(!Ready){if(!note.active)return false;for(int o=0;o<2;++o)if(sourceKind[o]&&!sourceSelection[o].data)return false;}
    int path=note.topology.target;if constexpr(!Ready){if(path<0||note.types.target<0||note.slope.target<0||note.mix.target<0)return false;}
    Voice*v[N];table::Oscillator::Head*head[2][N];PreparedVoice prepared[N];
    for(int i=0;i<N;++i){v[i]=&external[first+i];auto&m=*v[i];if constexpr(!Ready){if(!m.active||m.group!=note.group)return false;}
      for(int o=0;o<2;++o){auto&r=m.tables[o];auto&leader=note.tables[o];
        if(sourceKind[o]){if constexpr(!Ready){if(!r.ready||r.active||m.tableGeneration[o]!=sourceSelectionGeneration)return false;}head[o][i]=&r.current;}
        else if constexpr(!Ready){if(r.ready)return false;}
      }
    }
    for(int i=0;i<N;++i){std::copy_n(v[i]->topologyPhase[path].data(),2,v[i]->phase);v[i]->masterWrapped=v[i]->topologyWrapped[path];}
    float env1=env[0],env2=env[1];
    float depth=std::clamp(float(state[XDepth])+note.mod[6],0.f,1.f),fm=8*depth*note.cross.weights[1],pm=4*depth*note.cross.weights[2];
    float lfoDepth=std::max(0.f,1+note.mod[11]);float directPitch=lfoValue*lfoDepth*note.lfoPitch;for(int l=1;l<4;++l)directPitch+=lfoValues[l]*float(state[lfoParam(l,1)]);
    float ratio=std::max(0.f,1+directPitch),shape[2]={std::clamp(p[1]+note.mod[1],0.f,1.f),std::clamp(p[3]+note.mod[4],0.f,1.f)};
    float balance=std::clamp(p[7]+note.mod[7],0.f,1.f),spread=50*std::clamp(float(state[Detune])+note.mod[41],0.f,1.f);
    alignas(16)float detune[4]{};
    for(int i=0;i<N;++i){auto&m=*v[i];if(m.renderedSamples!=UINT32_MAX)++m.renderedSamples;detune[i]=1;
      if(m.members>1||m.detunePosition!=0){m.spread+=(spread-m.spread)/transitionSamples;if(m.spread!=m.cachedSpread||m.detunePosition!=m.cachedDetunePosition){m.cachedSpread=m.spread;m.cachedDetunePosition=m.detunePosition;m.cachedDetuneRatio=std::exp2(m.detunePosition*m.spread/2400.f);}detune[i]=m.cachedDetuneRatio;}
    }
    auto lanes=_mm_load_ps(detune);
    auto hz1=_mm_mul_ps(_mm_mul_ps(_mm_set1_ps(note.hz*note.pitchRatio[0]),lanes),_mm_set1_ps(ratio));
    auto hz2=_mm_mul_ps(_mm_mul_ps(_mm_mul_ps(_mm_mul_ps(_mm_set1_ps(note.hz*note.pitchRatio[1]),lanes),_mm_set1_ps(std::max(0.f,1+env1*note.egPitch[0]))),_mm_set1_ps(std::max(0.f,1+env2*note.egPitch[1]))),_mm_set1_ps(ratio));
    float position[2]={std::clamp(float(state[oscParam(0,3)])+note.mod[42],0.f,1.f),std::clamp(float(state[oscParam(1,3)])+note.mod[43],0.f,1.f)};
    bool wa[N]{},wb[N]{};__m128 a,b;bool sync=path%2!=0,reversed=path/2!=0,skipB=sourceKind[1]!=0&&balance==0&&fm==0&&pm==0&&!sync;
    auto one=_mm_set1_ps(1.f),zero=_mm_setzero_ps();
    auto read=[&](int o,__m128 frequency,__m128 phaseMod,bool*wrapped,bool silent=false){
      return sourceKind[o]?table::MemberTableBatch::read<N>(head[o],path,frequency,phaseMod,position[o],sr,wrapped,&note.tables[o].current,silent):analyticBatch<N>(v,o,path,frequency,phaseMod,shape[o],note,wrapped);
    };
    if(!reversed){a=read(0,hz1,zero,wa);
      if(sync)for(int i=0;i<N;++i)if(wa[i]){v[i]->phase[1]=0;v[i]->tables[1].sync(path);}
      b=read(1,_mm_mul_ps(hz2,_mm_add_ps(one,_mm_mul_ps(_mm_set1_ps(fm),a))),_mm_mul_ps(_mm_set1_ps(pm),a),wb,skipB);
    }else{if(sync)for(int i=0;i<N;++i)if(v[i]->topologyWrapped[path]){v[i]->phase[1]=0;v[i]->tables[1].sync(path);}
      b=read(1,hz2,zero,wb,skipB);
      a=read(0,_mm_mul_ps(hz1,_mm_add_ps(one,_mm_mul_ps(_mm_set1_ps(fm),b))),_mm_mul_ps(_mm_set1_ps(pm),b),wa);
    }
    // Original path accumulation starts at +0 even for one settled path.
    a=_mm_add_ps(zero,a);b=_mm_add_ps(zero,b);
    a=_mm_mul_ps(a,_mm_set1_ps(std::clamp(1+note.mod[2],0.f,2.f)));b=_mm_mul_ps(b,_mm_set1_ps(std::clamp(1+note.mod[5],0.f,2.f)));
    __m128 input;
    switch(note.mix.target){
      case 0:input=_mm_add_ps(_mm_mul_ps(_mm_set1_ps(1-balance),a),_mm_mul_ps(_mm_set1_ps(balance),b));break;
      case 1:input=_mm_mul_ps(_mm_set1_ps(.5f),_mm_add_ps(_mm_mul_ps(_mm_set1_ps(std::min(1.f,2*(1-balance))),a),_mm_mul_ps(_mm_set1_ps(std::min(1.f,2*balance)),b)));break;
      case 2:input=_mm_add_ps(_mm_mul_ps(_mm_set1_ps(1-balance),a),_mm_mul_ps(_mm_mul_ps(_mm_set1_ps(balance),a),b));break;
      case 3:input=_mm_add_ps(_mm_mul_ps(_mm_set1_ps(1-balance),a),_mm_mul_ps(_mm_mul_ps(_mm_mul_ps(_mm_set1_ps(balance),a),_mm_add_ps(b,one)),_mm_set1_ps(.5f)));break;
      case 4:{auto sign=_mm_xor_ps(_mm_cmpge_ps(a,zero),_mm_cmpge_ps(b,zero));auto wave=table::MemberTableBatch::select(sign,one,_mm_set1_ps(-1.f));input=_mm_add_ps(_mm_mul_ps(_mm_set1_ps(1-balance),a),_mm_mul_ps(_mm_set1_ps(balance),wave));break;}
      default:input=_mm_sub_ps(_mm_mul_ps(_mm_set1_ps(1-balance),a),_mm_mul_ps(_mm_set1_ps(balance),b));
    }
    alignas(16)float mixed[4];_mm_store_ps(mixed,input);
    for(int i=0;i<N;++i){auto&m=*v[i];for(int o=0;o<2;++o){if(sourceKind[o])m.phase[o]=float(head[o][i]->phase[path]);m.topologyPhase[path][o]=m.phase[o];}m.masterWrapped=m.topologyWrapped[path]=wa[i];m.firstSample=false;prepared[i]={mixed[i],env1};}
    float filtered[N];networkBatch<N>(v,prepared,note,filtered);
    for(int i=0;i<N;++i)output[i]=finishVoice(*v[i],filtered[i]*env1,note);
    ++retirementLaneFrames;return true;
  }
  bool renderRetirementLanes(RetireBatch&r,float*output){
    if(!retirementLanesEnabled||!r.coherent||!noteSharingEnabled||!noteSharingSafe||r.topologyEpoch||(r.count!=4&&r.count!=8))return false;
    for(int m=0;m<r.count;++m)if(!retiredModesReady(r.voices[m])||r.positionRamp[m])return false;
    if(!retiredBatchReady(r,r.voices[0]))return false;
    Voice advanced=r.voices[0];float env[2];advanceRetiredNote(advanced,env);
    if((!advanced.gate&&advanced.eg[0].value<1.e-6f)||!retiredModesReady(advanced)||!retiredBatchReady(r,advanced))return false;
    r.voices[0]=advanced;
    renderRetiredBatch<4,true>(0,r.voices[0],output,r.voices.data(),env);
    if(r.count==8)renderRetiredBatch<4,true>(4,r.voices[0],output+4,r.voices.data(),env);
    // Keep cold note shadows current: a parameter/source/topology change may
    // require the original independent retirement path on the very next sample.
    for(int m=1;m<r.count;++m)if(r.voices[m].active)copyNoteState(r.voices[m],r.voices[0]);
    return true;
  }
