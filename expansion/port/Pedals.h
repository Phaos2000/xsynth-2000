// GPL-2.0-or-later. Polyphonic MIDI pedal bookkeeping; never an amplitude follower.
  bool sustainDown=false,sostenutoDown=false;
  void pedalReleaseUnheld(){
    if(discrete(VoiceMode,state[VoiceMode])!=2)return;
    if(groupLive){for(int i=0;i<MemberCapacity;++i)if(intents[i].held&&!intents[i].keyDown&&!sustainDown&&!intents[i].pedalLatched){intents[i].held=false;for(int g=0;g<groupLimit;++g)if(groups[g].active&&groups[g].intent==i&&groups[g].intentAge==intents[i].age)releaseGroup(g);}refreshHeld();}
    else for(auto&v:pool)if(v.active&&v.gate&&!v.keyDown&&!sustainDown&&!v.pedalLatched)release(v);
  }
  void setPedal(int id,double value){
    bool down=value>=.5;state[id]=down?1:0;
    if(id==Sustain){if(sustainDown==down)return;sustainDown=down;}
    else{if(sostenutoDown==down)return;sostenutoDown=down;
      if(groupLive){for(auto&i:intents)i.pedalLatched=down&&discrete(VoiceMode,state[VoiceMode])==2&&i.held&&i.keyDown;}
      else for(auto&v:pool)v.pedalLatched=down&&discrete(VoiceMode,state[VoiceMode])==2&&v.active&&v.gate&&v.keyDown;
    }
    if(!down)pedalReleaseUnheld();
  }
