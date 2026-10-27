// GPL-2.0-or-later. Private Engine implementation; fixed bounded member storage.
  struct Intent {bool held=false;int note=0,id=-1;float velocity=0;uint32_t age=0;bool keyDown=true,pedalLatched=false;};
  struct Group {bool active=false,gate=false,fault=false,noteCoherent=false,noteShared=false,noteFinished=false,discreteShared=false;uint8_t topologyDominant=0;int intent=-1,fade=0,sharingDelay=0;uint32_t age=0,intentAge=0;float last=0,previous=0,lastLeft=0,lastRight=0,previousLeft=0,previousRight=0;float envelopeValue[2]{};};
  std::array<Intent,MemberCapacity> intents{};
  std::array<Group,MemberCapacity> groups{};
  bool groupLive=false;
  int groupSize=1,groupLimit=8,topologyRemaining=0;
  float topologyFrom=0,lastPreOutput=0;
  float topologyFromLeft=0,topologyFromRight=0;
  static float memberPhase(int n,int m){return n==1?0.f:n==2?m*.21f:wrap(float(m*m)/(2*n));}
  int priorityIntent() const {
    int best=-1;for(int i=0;i<MemberCapacity;++i)if(intents[i].held && (best<0 || intents[i].note>intents[best].note || (intents[i].note==intents[best].note&&intents[i].age>intents[best].age)))best=i;
    return best;
  }
  void refreshHeld(){held={};for(auto &i:intents)if(i.held){held[i.note]=true;velocities[i.note]=i.velocity;}key=highest();}
  void clearGroup(int g){for(int m=0;m<groupSize;++m)pool[g*groupSize+m]={};groups[g]={};}
  void startGroup(int g,int intent,bool legato,bool fresh){
    auto &owner=intents[intent];auto &group=groups[g];float previous=group.last,previousLeft=group.lastLeft,previousRight=group.lastRight;
    bool replacing=group.active&&fresh;uint32_t age=++serial;float oldHz=lastHz,sharedRandom=0;
    bool handoff=replacing&&retireNote(g*groupSize,groupSize);
    detachTopologyVoices(g*groupSize,groupSize);
    if(handoff)group.fade=0;
    for(int m=0;m<groupSize;++m){
      auto &v=pool[g*groupSize+m];if(fresh)v={};lastHz=oldHz;
      trigger(v,owner.note,owner.velocity,legato,owner.id);
      v.keyDown=owner.keyDown;v.pedalLatched=owner.pedalLatched;
      if(handoff)incomingHandoff(v);
      if(m==0)sharedRandom=v.random;v.random=sharedRandom;v.age=age;
      v.group=g;v.member=m;v.members=groupSize;
      v.detunePosition=groupSize==1?0.f:float(2*m-(groupSize-1))/(groupSize-1);
      if(fresh){float phase=memberPhase(groupSize,m);
        // Equal-pitch sine/triangle RING has a doubled carrier. Balance its
        // doubled phases rather than cancelling that carrier at note startup.
        int wave=discrete(24,state[24]);
        if(discrete(MixerMode,state[MixerMode])==2 && p[7]>.9f && wave<=1 && discrete(25,state[25])==wave && state[0]==state[2] && state[XMode]==0)phase*=.5f;v.phase[0]=v.phase[1]=phase;for(auto &p:v.topologyPhase)p={phase,phase};v.spread=float(state[Detune])*50;}
    }
    if(fresh)group.noteCoherent=true;group.noteShared=false;
    serial=age;group.active=group.gate=true;group.intent=intent;group.intentAge=owner.age;group.age=age;group.fault=false;
    if(replacing&&!handoff){group.previous=previous;group.previousLeft=previousLeft;group.previousRight=previousRight;group.fade=transitionSamples;}
  }
  int allocateGroup()const {
    for(int g=0;g<groupLimit;++g)if(!groups[g].active)return g;
    if(stealPolicy!=StealPolicy::Current)return groupVictim();
    int chosen=0;for(int g=1;g<groupLimit;++g)
      if((!groups[g].gate&&groups[chosen].gate)||(groups[g].gate==groups[chosen].gate&&groups[g].age<groups[chosen].age))chosen=g;
    return chosen;
  }
  void releaseGroup(int g){groups[g].gate=false;for(int m=0;m<groupSize;++m)if(pool[g*groupSize+m].active)release(pool[g*groupSize+m]);}
  void enableGroups(){
    intents={};int count=0;
    // Capture active note IDs first; held-but-unsounded pitches are retained when space permits.
    for(auto&v:pool)if(v.active&&v.gate&&!v.retiring&&count<MemberCapacity)intents[count++]={true,v.note,v.noteId,v.velocity,v.age,v.keyDown,v.pedalLatched};
    for(int n=21;n<=96&&count<MemberCapacity;++n)if(held[n]){bool found=false;for(auto&i:intents)found|=i.held&&i.note==n;if(!found)intents[count++]={true,n,-1,velocities[n],++serial};}
    groupLive=true;groupSize=0;
  }
  // A topology edit may replace the entire member pool. Keep a complete fixed
  // outgoing bank, independent of the two ordinary stealing slots. Rapid edits
  // queue the newest requested topology until this2ms waveform crossfade ends.
  std::array<Voice,MemberCapacity> topologyVoices{};
  std::array<Group,MemberCapacity> topologyGroups{};
  std::array<int,MemberCapacity> topologyVoiceGroups{};
  std::array<float,MemberCapacity> topologyGroupGains{};
  std::array<float,MemberCapacity> topologyWeights{},topologyLeft{},topologyRight{};
  std::array<int,MemberCapacity> topologyRetained{},topologyRetainedOld{};
  std::array<float,MemberCapacity> topologyPositionFrom{},topologyPositionTo{},topologyDetachedFrom{},topologyDetachedTo{};
  std::array<bool,MemberCapacity> topologyDetachedRamp{};
  uint64_t topologyEpoch=0;
  float topologyIncomingWeight=1;
  unsigned topologyExtraRenders=0;
  int topologyTailRemaining=0,topologyTailTotal=0;
  bool topologyPending=false;
  bool topologyStereo()const{if(topologyTailRemaining)for(int i=0;i<MemberCapacity;++i)if(topologyVoices[i].active&&topologyLeft[i]!=topologyRight[i])return true;return false;}
  unsigned topologyOwners()const{unsigned mask=0;if(topologyTailRemaining)for(const auto&v:topologyVoices)if(v.active)for(const auto&t:v.tables)mask|=t.owners();return mask;}
  float topologyProgress()const{return topologyTailRemaining?float(topologyTailTotal-topologyTailRemaining)/std::max(1,topologyTailTotal-1):1.f;}
  void detachTopologyVoices(int first,int count){
    if(!topologyTailRemaining)return;
    for(int i=first;i<first+count;++i){int old=topologyRetainedOld[i];if(old<0)continue;topologyVoices[old]=pool[i];copyDistortion(topologyVoices[old],pool[i]);topologyVoices[old].group=-1;topologyDetachedRamp[old]=true;topologyDetachedFrom[old]=topologyPositionFrom[i];topologyDetachedTo[old]=topologyPositionTo[i];topologyRetained[old]=topologyRetainedOld[i]=-1;}
  }
  bool rebuildGroups(int count,int mode){
    if(topologyTailRemaining||topologyRemaining){topologyPending=true;return false;}
    topologyGroups=groups;const auto&oldGroups=topologyGroups;topologyVoices=pool;
    if(distortionControl.live())for(int i=0;i<MemberCapacity;++i)copyDistortion(topologyVoices[i],pool[i]);
    topologyRetained.fill(-1);topologyRetainedOld.fill(-1);topologyDetachedRamp.fill(false);
    bool audible=false;for(int i=0;i<MemberCapacity;++i){const auto&v=pool[i];audible|=v.active;int members=std::max(1,v.members);topologyVoiceGroups[i]=v.group;topologyWeights[i]=v.group>=0?1.f/members:1;if(v.group>=0)topologyGroupGains[v.group]=stackCompensation(members);topologyLeft[i]=v.group>=0?panLeft[v.member]:1;topologyRight[i]=v.group>=0?panRight[v.member]:1;}
    pool={};poolEnd=0;groups={};groupSize=count;groupLimit=mode==2?voiceLimit/count:1;
    std::array<bool,MemberCapacity> selected{},usedOld{};
    auto restore=[&](int g,int first,int intent){
      int oldCount=std::max(1,topologyVoices[first].members),base=first-topologyVoices[first].member;
      auto oldGroup=topologyVoices[first].group;groups[g]=oldGroup>=0?oldGroups[oldGroup]:Group{};
      auto&group=groups[g];group.noteShared=false;if(oldCount==1||oldGroup<0)group.noteCoherent=true;group.active=true;group.gate=topologyVoices[first].gate;group.fade=0;group.fault=false;
      if(intent>=0){group.intent=intent;group.intentAge=intents[intent].age;}else{group.intent=-1;group.intentAge=0;}
      group.age=topologyVoices[first].age;
      for(int m=0;m<count;++m){
        int source=count==1?0:int(std::lround(float(m)*(oldCount-1)/(count-1)));
        bool retained=count<=oldCount;
        if(count>oldCount){retained=false;for(int k=0;k<oldCount;++k)if(m==(oldCount==1?0:int(std::lround(float(k)*(count-1)/(oldCount-1))))){source=k;retained=true;break;}}
        if(!topologyVoices[base+source].active)source=first-base;
        auto&v=pool[g*count+m];poolEnd=std::max(poolEnd,g*count+m+1);v=topologyVoices[base+source];copyDistortion(v,topologyVoices[base+source]);v.group=g;v.member=m;v.members=count;
        if(intent>=0){v.keyDown=intents[intent].keyDown;v.pedalLatched=intents[intent].pedalLatched;}
        v.detunePosition=count==1?0.f:float(2*m-(count-1))/(count-1);
        if(retained){int index=g*count+m,old=base+source;topologyRetained[old]=index;topologyRetainedOld[index]=old;topologyPositionFrom[index]=topologyVoices[old].detunePosition;topologyPositionTo[index]=v.detunePosition;v.detunePosition=topologyPositionFrom[index];}
        if(!retained){
          // New members inherit the running EG/filter/modulation trajectory,
          // then receive the established deterministic phase-grid offset.
          float shift=memberPhase(count,m)-memberPhase(oldCount,source);
          for(int o=0;o<2;++o){v.phase[o]=wrap(v.phase[o]+shift);for(int t=0;t<4;++t){v.topologyPhase[t][o]=wrap(v.topologyPhase[t][o]+shift);v.tables[o].current.phase[t]=table::wrap(v.tables[o].current.phase[t]+shift);v.tables[o].target.phase[t]=table::wrap(v.tables[o].target.phase[t]+shift);}}
        }
      }
      for(int m=0;m<oldCount;++m)usedOld[base+m]=true;
    };
    int g=0;
    // Same deterministic held-note capacity priority as before. Retain exact
    // note identities, rather than treating an old note-off as a new note.
    for(;g<groupLimit;++g){int chosen=-1;
      if(mode!=2)chosen=priorityIntent();
      else for(int i=0;i<MemberCapacity;++i)if(intents[i].held&&!selected[i]&&(chosen<0||intents[i].age>intents[chosen].age))chosen=i;
      if(chosen<0)break;selected[chosen]=true;int first=-1;
      for(int i=0;i<MemberCapacity;++i){const auto&v=topologyVoices[i];if(!v.active||usedOld[i]||v.note!=intents[chosen].note||v.noteId!=intents[chosen].id)continue;bool identity=v.group>=0?oldGroups[v.group].intentAge==intents[chosen].age:v.age==intents[chosen].age;if(identity){first=i;break;}}
      if(first>=0)restore(g,first,chosen);else startGroup(g,chosen,false,true);
    }
    // Release tails survive a topology edit when physical capacity permits.
    for(int i=0;i<MemberCapacity&&g<groupLimit;++i)if(topologyVoices[i].active&&!topologyVoices[i].gate&&!usedOld[i])restore(g++,i,-1);
    for(auto&v:topologyVoices)v.group=-1; // A retired fault cannot clear a new group.
    topologyTailRemaining=audible?std::max(2,transitionSamples):0;topologyTailTotal=topologyTailRemaining;
    if(audible&&++topologyEpoch==0)++topologyEpoch;
    topologyRemaining=audible?0:transitionSamples;if(!audible){topologyFrom=lastPreOutput;topologyFromLeft=preOutputLeft;topologyFromRight=preOutputRight;}topologyPending=false;refreshHeld();return true;
  }
  void renderTopologyTransition(float&mono,float&left,float&right,const std::array<float,MemberCapacity>&rendered){
    topologyIncomingWeight=1;topologyExtraRenders=0;if(!topologyTailRemaining)return;
    float t=topologyIncomingWeight=topologyProgress(),old=0,oldLeft=0,oldRight=0;
    std::array<float,MemberCapacity> groupMono{},groupLeft{},groupRight{};
    std::array<float,MemberCapacity> outgoing;BatchMask outgoingReady,outgoingTried;
    for(int i=0;i<MemberCapacity;++i)if(topologyVoices[i].active){int retained=topologyRetained[i];float sample=0;if(retained>=0)sample=rendered[retained];else{if(topologyDetachedRamp[i])topologyVoices[i].detunePosition=topologyDetachedFrom[i]+(topologyDetachedTo[i]-topologyDetachedFrom[i])*t;int owner=topologyVoiceGroups[i];if(owner>=0&&!outgoingTried.test(owner)){outgoingTried.set(owner,1u);renderTopologyGroupLanes(i,outgoing.data(),outgoingReady);}sample=outgoingReady.test(i)?outgoing[i]:renderVoice(topologyVoices[i]);++topologyExtraRenders;}float x=sample*topologyWeights[i];int g=topologyVoiceGroups[i];if(g>=0){groupMono[g]+=x;groupLeft[g]+=x*topologyLeft[i];groupRight[g]+=x*topologyRight[i];}else{old+=x;oldLeft+=x*topologyLeft[i];oldRight+=x*topologyRight[i];}}
    // Preserve any already-running handoff as part of the outgoing trajectory.
    // Its waveform continues rendering; it is never replaced by a held sample.
    for(int g=0;g<MemberCapacity;++g)if(topologyGroups[g].active){auto&group=topologyGroups[g];float value=groupMono[g]*topologyGroupGains[g],l=groupLeft[g]*topologyGroupGains[g],r=groupRight[g]*topologyGroupGains[g];if(group.fade){float u=1-float(group.fade)/transitionSamples;value=group.previous*(1-u)+value*u;l=group.previousLeft*(1-u)+l*u;r=group.previousRight*(1-u)+r*u;--group.fade;}old+=value;oldLeft+=l;oldRight+=r;}
    mono=old*(1-t)+mono*t;left=oldLeft*(1-t)+left*t;right=oldRight*(1-t)+right*t;
    for(int i=0;i<MemberCapacity;++i){int retained=topologyRetained[i];if(retained>=0&&!pool[retained].active){topologyVoices[i].active=false;topologyRetained[i]=topologyRetainedOld[retained]=-1;}}
    if(!--topologyTailRemaining&&topologyPending)changed=true;
  }
  void groupNote(int n,bool down,float velocity,int id){
    const int noteMode=discrete(VoiceMode,state[VoiceMode]);
    int oldKey=key;int previous=priorityIntent();bool connected=previous>=0;int added=-1;
    if(down){
      for(int i=0;i<MemberCapacity;++i)if(!intents[i].held){added=i;break;}
      if(added<0){added=0;for(int i=1;i<MemberCapacity;++i)if(intents[i].age<intents[added].age)added=i;
        for(int g=0;g<groupLimit;++g)if(groups[g].active&&groups[g].intent==added&&groups[g].intentAge==intents[added].age)releaseGroup(g);}
      intents[added]={true,n,id,std::clamp(velocity,0.f,1.f),++serial};
    } else for(int i=0;i<MemberCapacity;++i)if(intents[i].held&&intents[i].note==n&&(id<0||intents[i].id==id)){
      intents[i].keyDown=false;for(int g=0;g<groupLimit;++g)if(groups[g].active&&groups[g].intent==i&&groups[g].intentAge==intents[i].age)for(int m=0;m<groupSize;++m)pool[g*groupSize+m].keyDown=false;
      if(noteMode!=2||(!sustainDown&&!intents[i].pedalLatched)){intents[i].held=false;for(int g=0;g<groupLimit;++g)if(groups[g].active&&groups[g].intent==i&&groups[g].intentAge==intents[i].age&&noteMode==2)releaseGroup(g);}
    }
    refreshHeld();int newKey=key;key=oldKey;int chosen=priorityIntent();
    if(noteMode==2){if(down){int target=allocateGroup();
#ifdef XSYNTH_RETIRE_EXPERIMENTS
      if(studySamePitchReplace){int same=-1;for(int g=0;g<groupLimit;++g)if(groups[g].active&&pool[g*groupSize].note==n&&(same<0||groups[g].age>groups[same].age))same=g;
        if(same>=0){target=same;int old=groups[same].intent;if(old>=0&&intents[old].age==groups[same].intentAge)intents[old].held=false;}}
#endif
      startGroup(target,added,false,true);}}
    else if(chosen>=0){
      if(chosen!=previous || (down&&chosen==added))startGroup(0,chosen,noteMode==1&&connected,!groups[0].active);
    } else if(groups[0].active)releaseGroup(0);
    key=newKey;
  }


