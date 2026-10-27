// GPL-2.0-or-later. Fixed retirement storage; ranked allocation and full-trajectory retirement. No host policy controls.
#ifndef XSYNTH_RETIRE_SLOTS
#define XSYNTH_RETIRE_SLOTS 2
#endif
  enum class StealPolicy {Current,Oldest,ReleaseFirst,Quietest,Hybrid};
  StealPolicy stealPolicy=StealPolicy::Hybrid;
  bool smoothSteals=true,equalPowerSteals=false;
  float stealMilliseconds=2;
  int retireCapacity=XSYNTH_RETIRE_SLOTS;
  struct RetireBatch {bool coherent=false;std::array<Voice,8> voices;std::array<float,8> left{},right{},scale{},positionFrom{},positionTo{};std::array<bool,8> positionRamp{};uint64_t topologyEpoch=0;int first=0,count=0,remaining=0,total=0;float coefficient=1;};
  std::array<RetireBatch,XSYNTH_RETIRE_SLOTS> retirePool;
#ifdef XSYNTH_RETIRE_EXPERIMENTS
  bool studyImmediateAttack=false,studySamePitchReplace=false;
#endif
  unsigned steals=0,retireOverflows=0;bool batchSaturated=false;
  struct Candidate {int index;bool gate,keyDown;float amplitude;uint32_t age,rendered;};
  int chooseVictim(const Candidate*items,int count)const{
    auto category=[&](const Candidate&c){
      if(stealPolicy==StealPolicy::Oldest)return 0;
      if(stealPolicy==StealPolicy::Quietest)return 0;
      if(stealPolicy==StealPolicy::Hybrid){
        bool fresh=c.gate&&c.keyDown&&c.rendered<uint32_t(sr*.02f);
        return (fresh?4:0)+(c.amplitude<.0001f?0:!c.gate?1:!c.keyDown?2:3);
      }return c.gate?1:0;
    };
    int best=0;for(int i=1;i<count;++i){const auto&a=items[i];const auto&b=items[best];int ca=category(a),cb=category(b);
      bool quiet=stealPolicy==StealPolicy::Quietest||stealPolicy==StealPolicy::Hybrid||(stealPolicy==StealPolicy::ReleaseFirst&&!a.gate&&!b.gate);
      if(ca<cb||(ca==cb&&((quiet&&a.amplitude<b.amplitude)||(a.amplitude==b.amplitude||!quiet)&&a.age<b.age)))best=i;
    }return items[best].index;
  }
  float predictedAmplitude(const Voice&v)const{float sensitivity=v.selection[15]<0?velocitySensitivity[0]:v.egVelocity[0];return float(v.eg[0].value)*(1-sensitivity+sensitivity*v.velocity);}
  int polyVictim()const{
    std::array<Candidate,MemberCapacity> candidates{};for(int i=0;i<voiceLimit;++i)candidates[i]={i,pool[i].gate,pool[i].keyDown,predictedAmplitude(pool[i]),pool[i].age,pool[i].renderedSamples};
    return chooseVictim(candidates.data(),voiceLimit);
  }
  int groupVictim()const{
    std::array<Candidate,MemberCapacity> candidates{};for(int g=0;g<groupLimit;++g){float amplitude=0;uint32_t rendered=UINT32_MAX;for(int m=0;m<groupSize;++m){const auto&v=pool[g*groupSize+m];amplitude=std::max(amplitude,predictedAmplitude(v));rendered=std::min(rendered,v.renderedSamples);}candidates[g]={g,groups[g].gate,pool[g*groupSize].keyDown,amplitude,groups[g].age,rendered};}
    return chooseVictim(candidates.data(),groupLimit);
  }
  float handoffIn(float t)const{return equalPowerSteals?std::sin(float(Pi*.5)*t):t;}
  float handoffOut(float t)const{return equalPowerSteals?std::cos(float(Pi*.5)*t):1-t;}
  bool retireNote(int first,int members){
#ifdef XSYNTH_RETIRE_HOOK
    XSYNTH_RETIRE_HOOK(*this,first,members);
#endif
    std::array<float,8> from{},to{};std::array<bool,8> ramp{};
    if(topologyTailRemaining)for(int m=0;m<members;++m)if(topologyRetainedOld[first+m]>=0){ramp[m]=true;from[m]=topologyPositionFrom[first+m];to[m]=topologyPositionTo[first+m];}
    detachTopologyVoices(first,members);
    ++steals;
    if(!smoothSteals)return false;
    if(batchSaturated){++retireOverflows;return false;}
    int chosen=-1;for(int i=0;i<retireCapacity;++i)if(!retirePool[i].count){chosen=i;break;}
    if(chosen<0){
      ++retireOverflows;batchSaturated=true;
      // No sample has sounded from these new tails. Revert the complete onset
      // batch to its existing bounded baseline, never truncate an audible tail.
      for(auto&pending:retirePool)if(pending.count&&pending.remaining==pending.total){
        int g=pool[pending.first].group;
        if(g>=0){auto&group=groups[g];group.previous=group.last;group.previousLeft=group.lastLeft;group.previousRight=group.lastRight;group.fade=transitionSamples;}
        for(int m=0;m<pending.count;++m)pool[pending.first+m].handoffRemaining=0;
        pending.count=0;
      }
      return false;
    }
    auto&r=retirePool[chosen];r.first=first;r.count=members;r.remaining=r.total=std::max(2,int(sr*stealMilliseconds*.001f));r.coefficient=stackCompensation(members)/members;
    r.coherent=pool[first].group>=0&&groups[pool[first].group].noteCoherent&&groups[pool[first].group].sharingDelay==0;
    r.topologyEpoch=topologyTailRemaining?topologyEpoch:0;r.positionRamp=ramp;r.positionFrom=from;r.positionTo=to;
    for(int m=0;m<members;++m){r.voices[m]=pool[first+m];auto&v=r.voices[m];copyDistortion(v,pool[first+m]);v.group=-1;v.retiring=false;r.scale[m]=1;if(v.handoffRemaining){float t=1-float(v.handoffRemaining)/std::max(1,v.handoffTotal-1);r.scale[m]=handoffIn(std::clamp(t,0.f,1.f));v.handoffRemaining=0;}r.left[m]=members>1?panLeft[m]:1;r.right[m]=members>1?panRight[m]:1;}
    return true;
  }
  void incomingHandoff(Voice&v){
#ifdef XSYNTH_RETIRE_EXPERIMENTS
    if(studyImmediateAttack)return;
#endif
v.handoffTotal=std::max(2,int(sr*stealMilliseconds*.001f));v.handoffRemaining=v.handoffTotal;}
  void renderRetirements(float&mono,float&left,float&right){
    for(auto&r:retirePool)if(r.count){float rendered[8]{};bool lanes=renderRetirementLanes(r,rendered);float t=float(r.total-r.remaining)/std::max(1,r.total-1),topology=r.topologyEpoch&&r.topologyEpoch==topologyEpoch?topologyIncomingWeight:1.f,weight=handoffOut(t)*topology;for(int m=0;m<r.count;++m){if(r.positionRamp[m]){float u=r.topologyEpoch==topologyEpoch?topologyIncomingWeight:1.f;r.voices[m].detunePosition=r.positionFrom[m]+(r.positionTo[m]-r.positionFrom[m])*u;}float x=(lanes?rendered[m]:renderVoice(r.voices[m]))*r.scale[m]*r.coefficient*weight;mono+=x;left+=x*r.left[m];right+=x*r.right[m];}if(!--r.remaining)r.count=0;}
  }
  bool retiredStereo()const{for(const auto&r:retirePool)if(r.count&&r.left[0]!=r.right[0])return true;return false;}
  unsigned retireOwners()const{unsigned mask=0;for(const auto&r:retirePool)if(r.count)for(int m=0;m<r.count;++m)for(const auto&t:r.voices[m].tables)mask|=t.owners();return mask;}


