// GPL-2.0-or-later. Private Engine implementation; fixed metadata, same 32 voices.
  struct Intent {bool held=false;int note=0,id=-1;float velocity=0;uint32_t age=0;};
  struct Group {bool active=false,gate=false,fault=false,dcInitialized=false;int intent=-1,fade=0;uint32_t age=0,intentAge=0;float last=0,previous=0,dcMean=0,dcTarget=0,envelope=0;float memberPeak=0,peakEnvelope=0,crestGain=1,square=0,referencePower=0,groupPower=0,normalGain=1,normalTarget=1,slowReference=0,slowOutput=0,makeup=1,makeupTarget=1;std::array<float,4> dcState{},dcHigh{};int powerTick=0;};
  std::array<Intent,32> intents{};
  std::array<Group,32> groups{};
  bool groupLive=false;
  int groupSize=1,groupLimit=8,topologyRemaining=0;
  float topologyFrom=0,lastPreOutput=0;
  static float memberPhase(int n,int m){return n==1?0.f:n==2?m*.21f:wrap(float(m*m)/(2*n));}
  int priorityIntent() const {
    int best=-1;for(int i=0;i<32;++i)if(intents[i].held && (best<0 || intents[i].note>intents[best].note || (intents[i].note==intents[best].note&&intents[i].age>intents[best].age)))best=i;
    return best;
  }
  void refreshHeld(){held={};for(auto &i:intents)if(i.held){held[i.note]=true;velocities[i.note]=i.velocity;}key=highest();}
  void clearGroup(int g){for(int m=0;m<groupSize;++m)pool[g*groupSize+m]={};groups[g]={};}
  void startGroup(int g,int intent,bool legato,bool fresh){
    auto &owner=intents[intent];auto &group=groups[g];float previous=group.last;
    bool replacing=group.active&&fresh;uint32_t age=++serial;float oldHz=lastHz,sharedRandom=0;
    for(int m=0;m<groupSize;++m){
      auto &v=pool[g*groupSize+m];if(fresh)v={};lastHz=oldHz;
      trigger(v,owner.note,owner.velocity,legato,owner.id);
      if(m==0)sharedRandom=v.random;v.random=sharedRandom;v.age=age;
      v.group=g;v.member=m;v.members=groupSize;
      v.detunePosition=groupSize==1?0.f:float(2*m-(groupSize-1))/(groupSize-1);
      if(fresh){float phase=memberPhase(groupSize,m);
        // Equal-pitch sine/triangle RING has a doubled carrier. Balance its
        // doubled phases rather than cancelling that carrier at note startup.
        int wave=discrete(24,state[24]);
        if(discrete(MixerMode,state[MixerMode])==2 && p[7]>.9f && wave<=1 && discrete(25,state[25])==wave && state[0]==state[2] && state[XMode]==0)phase*=.5f;v.phase[0]=v.phase[1]=phase;for(auto &p:v.topologyPhase)p={phase,phase};v.spread=float(state[Detune])*50;}
    }
    serial=age;group.active=group.gate=true;group.intent=intent;group.intentAge=owner.age;group.age=age;group.fault=false;group.powerTick=0;if(fresh){group.dcInitialized=false;group.dcState={};group.dcHigh={};group.dcMean=0;group.memberPeak=group.peakEnvelope=0;group.crestGain=1;group.square=group.referencePower=group.groupPower=group.slowReference=group.slowOutput=0;group.normalGain=group.normalTarget=group.makeup=group.makeupTarget=1;}
    if(replacing){group.previous=previous;group.fade=transitionSamples;}
  }
  int allocateGroup()const {
    for(int g=0;g<groupLimit;++g)if(!groups[g].active)return g;
    int chosen=0;for(int g=1;g<groupLimit;++g)
      if((!groups[g].gate&&groups[chosen].gate)||(groups[g].gate==groups[chosen].gate&&groups[g].age<groups[chosen].age))chosen=g;
    return chosen;
  }
  void releaseGroup(int g){groups[g].gate=false;for(int m=0;m<groupSize;++m)if(pool[g*groupSize+m].active)release(pool[g*groupSize+m]);}
  void enableGroups(){
    intents={};int count=0;
    // Capture active note IDs first; held-but-unsounded pitches are retained when space permits.
    for(auto&v:pool)if(v.active&&v.gate&&!v.retiring&&count<32)intents[count++]={true,v.note,v.noteId,v.velocity,v.age};
    for(int n=21;n<=96&&count<32;++n)if(held[n]){bool found=false;for(auto&i:intents)found|=i.held&&i.note==n;if(!found)intents[count++]={true,n,-1,velocities[n],++serial};}
    groupLive=true;groupSize=0;
  }
  void rebuildGroups(int count,int mode){
    topologyFrom=lastPreOutput;topologyRemaining=transitionSamples;
    pool={};groups={};groupSize=count;groupLimit=mode==2?voiceLimit/count:1;
    // Keep newest held intents under reduced capacity; mono retains highest-note priority.
    std::array<bool,32> selected{};
    for(int g=0;g<groupLimit;++g){int chosen=-1;
      if(mode!=2)chosen=priorityIntent();
      else for(int i=0;i<32;++i)if(intents[i].held&&!selected[i]&&(chosen<0||intents[i].age>intents[chosen].age))chosen=i;
      if(chosen<0)break;selected[chosen]=true;startGroup(g,chosen,false,true);
    }
    refreshHeld();
  }
  void groupNote(int n,bool down,float velocity,int id){
    int oldKey=key;int previous=priorityIntent();bool connected=previous>=0;int added=-1;
    if(down){
      for(int i=0;i<32;++i)if(!intents[i].held){added=i;break;}
      if(added<0){added=0;for(int i=1;i<32;++i)if(intents[i].age<intents[added].age)added=i;
        for(int g=0;g<groupLimit;++g)if(groups[g].active&&groups[g].intent==added&&groups[g].intentAge==intents[added].age)releaseGroup(g);}
      intents[added]={true,n,id,std::clamp(velocity,0.f,1.f),++serial};
    } else for(int i=0;i<32;++i)if(intents[i].held&&intents[i].note==n&&(id<0||intents[i].id==id)){
      intents[i].held=false;for(int g=0;g<groupLimit;++g)if(groups[g].active&&groups[g].intent==i&&groups[g].intentAge==intents[i].age&&voiceMode==2)releaseGroup(g);
    }
    refreshHeld();int newKey=key;key=oldKey;int chosen=priorityIntent();
    if(voiceMode==2){if(down)startGroup(allocateGroup(),added,false,true);}
    else if(chosen>=0){
      if(chosen!=previous || (down&&chosen==added))startGroup(0,chosen,voiceMode==1&&connected,!groups[0].active);
    } else if(groups[0].active)releaseGroup(0);
    key=newKey;
  }
