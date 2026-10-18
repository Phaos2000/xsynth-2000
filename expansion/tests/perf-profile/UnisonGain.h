// GPL-2.0-or-later. DC-preserving equal-power group normalization.
// Included inside Engine. No audio-level follower, compressor or extra oscillator.
  float modeledInputMean(const Voice &v) const { profiler::Scope scope("group_dc_model");
    std::array<float,32> input{};
    float balance=std::clamp(p[7]+v.mod[7],0.f,1.f);
    int wave1=std::max(0,v.waves[0].target),wave2=std::max(0,v.waves[1].target),kind=std::max(0,v.mix.target);
    float shape1=std::clamp(p[1]+v.mod[1],0.f,1.f),shape2=std::clamp(p[3]+v.mod[4],0.f,1.f);
    float phaseDifference=wrap(v.phase[1]-v.phase[0]);
    float sumA=0,sumB=0,squareA=0,squareB=0,signA=0,signB=0,absoluteA=0;
    for(int t=0;t<32;++t){float phase=(t+.5f)/32,a=raw(phase,wave1,shape1)*std::clamp(1+v.mod[2],0.f,2.f),b=raw(wrap(phase+phaseDifference),wave2,shape2)*std::clamp(1+v.mod[5],0.f,2.f);
      sumA+=a/32;sumB+=b/32;squareA+=a*a/32;squareB+=b*b/32;signA+=(a>=0?1.f:-1.f)/32;signB+=(b>=0?1.f:-1.f)/32;absoluteA+=std::abs(a)/32;
      switch(kind){
      case 0:input[t]=(1-balance)*a+balance*b;break;
      case 1:input[t]=.5f*(std::min(1.f,2*(1-balance))*a+std::min(1.f,2*balance)*b);break;
      case 2:input[t]=(1-balance)*a+balance*a*b;break;
      case 3:input[t]=(1-balance)*a+balance*a*(b+1)*.5f;break;
      case 4:input[t]=(1-balance)*a+balance*((a>=0)!=(b>=0)?1.f:-1.f);break;
      default:input[t]=(1-balance)*a-balance*b;break;
      }
    }

    float mean=0;for(float x:input)mean+=x/32;
    // Linear mixer DC is known exactly, including continuously moving PWM.
    if(kind==0||kind==1||kind==5){
      float a=1-balance,b=balance;
      if(kind==1){a=.5f*std::min(1.f,2*(1-balance));b=.5f*std::min(1.f,2*balance);}
      if(kind==5)b=-b;
      float meanA=wave1==5?(2*shape1-1)*std::clamp(1+v.mod[2],0.f,2.f):0;
      float meanB=wave2==5?(2*shape2-1)*std::clamp(1+v.mod[5],0.f,2.f):0;
      return a*meanA+b*meanB;
    }
    float second=v.pitchRatio[1]*(1+float(v.eg[0].value)*v.egPitch[0])*(1+float(v.eg[1].value)*v.egPitch[1]);
    if(state[27]<.5 && std::abs(second-v.pitchRatio[0])>1.e-4f){
      float amplitudeA=std::clamp(1+v.mod[2],0.f,2.f),amplitudeB=std::clamp(1+v.mod[5],0.f,2.f);
      sumA=wave1==5?(2*shape1-1)*amplitudeA:0;sumB=wave2==5?(2*shape2-1)*amplitudeB:0;
      signA=amplitudeA==0?1:wave1==5?2*shape1-1:0;signB=amplitudeB==0?1:wave2==5?2*shape2-1:0;
      float a=1-balance,b=balance;
      if(kind==1){a=.5f*std::min(1.f,2*(1-balance));b=.5f*std::min(1.f,2*balance);}
      if(kind==0||kind==1||kind==5){if(kind==5)b=-b;mean=a*sumA+b*sumB;}
      else if(kind==2)mean=a*sumA+b*sumA*sumB;
      else if(kind==3)mean=sumA*(1-balance*.5f+balance*.5f*sumB);
      else mean=a*sumA-b*signA*signB;
    }
    return mean;
  }
  float normalizeGroup(int g,float average){ profiler::Scope scope("group_normalization");
    if(groupSize<=1)return average;
    auto &group=groups[g];const auto &v=pool[g*groupSize];
    if(group.powerTick==0){group.dcTarget=modeledInputMean(v);if(!group.dcInitialized){group.dcMean=group.dcTarget;group.dcInitialized=true;}}
    group.powerTick=(group.powerTick+1)&127;
    group.dcMean+=(group.dcTarget-group.dcMean)/transitionSamples;
    // Track only the control-derived DC term through the same linear response.
    // These four scalar states are normalization metadata, never another voice.
    float dc=group.dcMean,highInput=dc,first=0;int stages=v.slope.remaining?2:std::max(0,v.slope.target)+1;
    for(int i=0;i<stages;++i){auto &band=group.dcState[i*2],&low=group.dcState[i*2+1];
      low=flush(low+v.g*band);float high=dc-low-v.damping*band;band=flush(band+v.g*high);
      float outputs[]={low,high,band,low+high};dc=0;
      if(v.types.remaining)for(int t=0;t<4;++t)dc+=outputs[t]*v.types.weights[t];
      else dc=outputs[std::max(0,v.types.target)];
      if(i==0)first=dc;
    }
    if(v.slope.remaining)dc=first*v.slope.weights[0]+dc*v.slope.weights[1];
    float highFirst=0;
    for(int i=0;i<stages;++i){float outputs[4]{};highcut::stage(highInput,v.highG,v.damping,group.dcHigh.data()+2*i,outputs);highInput=0;
      if(v.types.remaining)for(int t=0;t<4;++t)highInput+=outputs[t]*v.types.weights[t];else highInput=outputs[std::max(0,v.types.target)];if(i==0)highFirst=highInput;}
    if(v.slope.remaining)highInput=highFirst*v.slope.weights[0]+highInput*v.slope.weights[1];
    dc=(dc*(1-v.highBlend)+highInput*v.highBlend)*group.envelope;float power=std::sqrt(float(groupSize));
    // Average = sum/N. AC uses 1/sqrt(N); coherent DC retains its 1/N scale.
    // Shared envelopes remain shared; dynamic normalization softens reinforcement.
    float ac=power*(average-dc);
    float reference=std::max(0.f,group.square-2*dc*average+dc*dc);
    group.referencePower+=normalPowerAlpha*(reference-group.referencePower);
    group.groupPower+=normalPowerAlpha*(ac*ac-group.groupPower);
    // Partial power normalization keeps the beat envelope alive. Unlike a
    // sample peak limiter it follows group energy relative to its own members.
    if((group.powerTick&7)==1)group.normalTarget=std::clamp(std::pow((group.referencePower+1.e-14f)/(group.groupPower+1.e-14f),.35f),.35f,2.f);
    float target=group.normalTarget;
    float alpha=target<group.normalGain?normalAttackAlpha:normalReleaseAlpha;
    group.normalGain+=alpha*(target-group.normalGain);
    float controlled=ac*group.normalGain;
    group.peakEnvelope=std::max(group.memberPeak,group.peakEnvelope*normalPeakRelease);
    float ceiling=std::max(std::abs(dc),1.41421356237f*group.peakEnvelope);
    float makeupApplied=group.makeup;
    float candidate=controlled*makeupApplied,crestTarget=1;
    if(candidate+dc>ceiling&&candidate>1.e-14f)crestTarget=std::clamp((ceiling-dc)/candidate,0.f,1.f);
    else if(candidate+dc<-ceiling&&candidate<-1.e-14f)crestTarget=std::clamp((-ceiling-dc)/candidate,0.f,1.f);
    if(crestTarget<group.crestGain)group.crestGain=crestTarget;
    else group.crestGain=std::min(crestTarget,group.crestGain+normalCrestReleaseAlpha*(1-group.crestGain));
    controlled*=group.crestGain;
    group.slowReference+=normalSlowAlpha*(reference-group.slowReference);
    group.slowOutput+=normalSlowAlpha*(controlled*controlled-group.slowOutput);
    if(group.powerTick==1)group.makeupTarget=std::clamp(std::sqrt((group.slowReference+1.e-14f)/(group.slowOutput+1.e-14f)),.8f,2.f);
    float makeup=group.makeupTarget;
    group.makeup+=normalReleaseAlpha*(makeup-group.makeup);
    return controlled*makeupApplied+dc;
  }
