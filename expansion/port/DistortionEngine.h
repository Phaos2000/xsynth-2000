// GPL-2.0-or-later. Included inside Engine after fixed voice banks exist.
  static constexpr int DistortionSlots=2*MemberCapacity+8*XSYNTH_RETIRE_SLOTS;
  // Keep delay storage out of Voice's hot oscillator/filter cache lines.
  // Allocate once at engine construction, never in render/events/reset.
  distortion::Storage<DistortionSlots> distortionBank;
  distortion::Control distortionControl;
  int distortionIndex(const Voice&v)const{
    auto address=reinterpret_cast<uintptr_t>(&v);
    auto locate=[&](const Voice*base,int count){auto first=reinterpret_cast<uintptr_t>(base);return address>=first&&address<first+sizeof(Voice)*count?int((address-first)/sizeof(Voice)):-1;};
    int index=locate(pool.data(),MemberCapacity);if(index>=0)return index;
    index=locate(topologyVoices.data(),MemberCapacity);if(index>=0)return MemberCapacity+index;
    for(int i=0;i<XSYNTH_RETIRE_SLOTS;++i){index=locate(retirePool[i].voices.data(),8);if(index>=0)return 2*MemberCapacity+i*8+index;}
    return -1;
  }
  void clearDistortion(Voice&v){int i=distortionIndex(v);if(i>=0)(*distortionBank)[i].clear();}
  void copyDistortion(Voice&to,const Voice&from){
    if(!distortionControl.live())return;int a=distortionIndex(to),b=distortionIndex(from);
    if(a>=0&&b>=0&&a!=b)(*distortionBank)[a]=(*distortionBank)[b];
  }
  void resetDistortion(){
    distortionControl.reset(sr,discrete(DistMode,state[DistMode]),effectiveDrive(state),state[DistMix],discrete(DistPlace,state[DistPlace]));
    for(auto&s:*distortionBank)s.clear();
  }
  void requestDistortion(){distortionControl.request(discrete(DistMode,state[DistMode]),effectiveDrive(state),state[DistMix],discrete(DistPlace,state[DistPlace]));}
  float distortedSample(Voice&v,float x){
    // Preserve Engine's existing logical-note fault retirement and counters.
    // A shaper must not conceal a nonfinite upstream filter/oscillator frame.
    if(!std::isfinite(x)){clearDistortion(v);return x;}
    int i=distortionIndex(v);return i<0?x:float((*distortionBank)[i].sample(x,distortionControl));
  }
