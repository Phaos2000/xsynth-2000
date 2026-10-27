// GPL-2.0-or-later. Included inside Engine after the unchanged TPT batch arithmetic.
  float bCutoffBase=50,bNominalBase=2,bTrackingBase=1;int bTypeKind=0,bFilterStages=1;
  int filterRoute=0,dualControlFrames=0;bool dualEverUsed=false;
  template<class To,class From>static void copyFilterControl(To&to,const From&from){
    to.highG=from.highG;to.highStep=from.highStep;to.damping=from.damping;to.dampingStep=from.dampingStep;to.resonanceStrength=from.resonanceStrength;to.resonanceStep=from.resonanceStep;
    to.cachedCutoff=from.cachedCutoff;to.cachedHighCoefficient=from.cachedHighCoefficient;to.cachedDamping=from.cachedDamping;to.cachedCalibration=from.cachedCalibration;to.cachedFilterType=from.cachedFilterType;to.cachedFilterStages=from.cachedFilterStages;to.types=from.types;to.slope=from.slope;
  }
  void secondaryControl(Voice&v){
    if(!filterRoute&&!dualControlFrames)return;
    auto&b=v.dual.b;int type=bTypeKind,stages=bFilterStages;
    b.types.select(type,transitionSamples);b.slope.select(stages-1,transitionSamples);
    float tracking=bTrackingBase,hz=v.hz*tuneBend*v.tune;
    float tracked=tracking==1?hz:tracking==0?440:440*std::pow(std::max(.01f,hz/440),tracking);
    float cutoff=std::clamp(tracked*bCutoffBase*(v.mod[44]!=0?std::exp2(v.mod[44]*4):1.f),0.f,.49f*sr);
    float nominal=std::clamp(bNominalBase-v.mod[45]*1.5f,.005f,2.f);
    bool law=nominal!=b.cachedDamping||type!=b.cachedFilterType||stages!=b.cachedFilterStages;
    if(law)b.cachedCalibration=highcut::productionRatio(nominal,type,stages);
    if(law||cutoff!=b.cachedCutoff){b.cachedCutoff=cutoff;b.cachedDamping=nominal;b.cachedFilterType=type;b.cachedFilterStages=stages;float g=highcut::coefficient(cutoff,sr);b.cachedHighCoefficient=type==0?g/b.cachedCalibration:type==1?g*b.cachedCalibration:g;}
    b.highStep=(b.cachedHighCoefficient-b.highG)*.125f;b.dampingStep=(highcut::resonanceDamping(nominal)-b.damping)*.125f;b.resonanceStep=(highcut::resonanceStrength(nominal)-b.resonanceStrength)*.125f;
  }
  void advanceSecondary(Voice&v){if(!filterRoute&&!dualControlFrames)return;auto&b=v.dual.b;b.types.advance();b.slope.advance();b.highG=std::max(0.f,b.highG+b.highStep);b.damping=std::clamp(b.damping+b.dampingStep,-.04f,2.f);b.resonanceStrength=std::clamp(b.resonanceStrength+b.resonanceStep,0.f,1.f);}
  template<class C>static float filterHistory(float*history,float input,const C&coefficient){
    int stages=coefficient.slope.remaining?2:coefficient.slope.target+1;float highInput=input,highFirst=0;double alpha=coefficient.resonanceStrength>0?highcut::radialAlpha(coefficient.highG,coefficient.resonanceStrength):0;
    for(int i=0;i<stages;++i){float output[4]{};float damping=coefficient.damping;
      if(coefficient.resonanceStrength>0&&i==0&&stages==2){float firstWeight=coefficient.slope.remaining?coefficient.slope.weights[0]:0;damping=std::max(.005f,damping)*(1-firstWeight)+damping*firstWeight;}
      highcut::stage(highInput,coefficient.highG,damping,history+2*i,output);if(alpha>0)highcut::radialState(history+2*i,alpha);highInput=0;
      if(!coefficient.types.remaining)highInput=output[coefficient.types.target];else for(int j=0;j<4;++j)highInput+=output[j]*coefficient.types.weights[j];if(i==0)highFirst=highInput;
    }return coefficient.slope.remaining?highFirst*coefficient.slope.weights[0]+highInput*coefficient.slope.weights[1]:highInput;
  }
  static float secondarySample(SecondaryFilter&v,float input,const SecondaryFilter&coefficient){if(v.types.target<0)v.types.reset(0);if(v.slope.target<0)v.slope.reset(0);return filterHistory(v.highFilter,input,coefficient);}
  template<int R>float stableNetwork(Voice&v,float input,const Voice&note){
    if constexpr(R==0)return filter(v,input,&note);
    if constexpr(R==1)return secondarySample(v.dual.b,input,note.dual.b);
    if constexpr(R==2)return secondarySample(v.dual.b,filter(v,input,&note),note.dual.b);
    if constexpr(R==3)return filter(v,secondarySample(v.dual.b,input,note.dual.b),&note);
    if constexpr(R==4){float a=filter(v,input,&note),b=secondarySample(v.dual.b,input,note.dual.b);return .5f*a+.5f*b;}
  }
  using ScalarNetwork=float(Engine::*)(Voice&,float,const Voice&);
  ScalarNetwork networkScalar=&Engine::stableNetwork<0>;
  static float oldNetwork(int route,float*a,float*b,float in,const Voice&note){switch(route){case 0:return filterHistory(a,in,note);case 1:return filterHistory(b,in,note.dual.b);case 2:return filterHistory(b,filterHistory(a,in,note),note.dual.b);case 3:return filterHistory(a,filterHistory(b,in,note.dual.b),note);default:{float x=filterHistory(a,in,note),y=filterHistory(b,in,note.dual.b);return .5f*x+.5f*y;}}}
  void beginNetworkTransition(Voice&v,const Voice&note){
    auto&d=v.dual;if(d.remaining||d.route==filterRoute)return;
    std::copy_n(v.highFilter,4,d.oldA);std::copy_n(d.b.highFilter,4,d.oldB);
    d.from=d.route;d.route=filterRoute;d.remaining=transitionSamples;
    // Newly enabled histories start clean; the outgoing topology still evolves during its fade.
    if(d.from==0&&d.route!=0)std::fill_n(d.b.highFilter,4,0.f);
    if(d.from==1&&d.route!=1)std::fill_n(v.highFilter,4,0.f);
  }
  float plainNetworkSample(Voice&v,float input,const Voice*shared=nullptr){
    if(!dualEverUsed)return filter(v,input,shared);
    const Voice&note=shared?*shared:v;beginNetworkTransition(v,note);auto&d=v.dual;
    if(!d.remaining)return (this->*networkScalar)(v,input,note);
    float old=oldNetwork(d.from,d.oldA,d.oldB,input,note),fresh;
    // At most one bounded transition per member; dense edits queue only the latest route.
    switch(d.route){case 0:fresh=stableNetwork<0>(v,input,note);break;case 1:fresh=stableNetwork<1>(v,input,note);break;case 2:fresh=stableNetwork<2>(v,input,note);break;case 3:fresh=stableNetwork<3>(v,input,note);break;default:fresh=stableNetwork<4>(v,input,note);}
    float t=float(transitionSamples-d.remaining)/std::max(1,transitionSamples-1);--d.remaining;return old*(1-t)+fresh*t;
  }
  float networkSample(Voice&v,float input,const Voice*shared=nullptr){
    if(!distortionControl.live())return plainNetworkSample(v,input,shared);
    if(!distortionControl.place)input=distortedSample(v,input);
    float y=plainNetworkSample(v,input,shared);
    return distortionControl.place?distortedSample(v,y):y;
  }
  template<int N,int R>static void stableNetworkBatch(Voice**v,const PreparedVoice*p,const Voice&note,float*out){
    if constexpr(R==0){filterBatch<N>(v,p,note,out);return;}
    SecondaryFilter*b[N];for(int i=0;i<N;++i)b[i]=&v[i]->dual.b;
    if constexpr(R==1){filterBatch<N>(b,p,note.dual.b,out);}
    if constexpr(R==2){float a[N];PreparedVoice q[N];filterBatch<N>(v,p,note,a);for(int i=0;i<N;++i)q[i]={a[i],p[i].env1};filterBatch<N>(b,q,note.dual.b,out);}
    if constexpr(R==3){float a[N];PreparedVoice q[N];filterBatch<N>(b,p,note.dual.b,a);for(int i=0;i<N;++i)q[i]={a[i],p[i].env1};filterBatch<N>(v,q,note,out);}
    if constexpr(R==4){float a[N],c[N];filterBatch<N>(v,p,note,a);filterBatch<N>(b,p,note.dual.b,c);for(int i=0;i<N;++i)out[i]=.5f*a[i]+.5f*c[i];}
  }
  using NetworkBatch=void(*)(Voice**,const PreparedVoice*,const Voice&,float*);
  NetworkBatch networkThree=&Engine::stableNetworkBatch<3,0>,networkFour=&Engine::stableNetworkBatch<4,0>;
  void selectNetwork(){
    static constexpr ScalarNetwork scalar[]={&Engine::stableNetwork<0>,&Engine::stableNetwork<1>,&Engine::stableNetwork<2>,&Engine::stableNetwork<3>,&Engine::stableNetwork<4>};
    static constexpr NetworkBatch three[]={&Engine::stableNetworkBatch<3,0>,&Engine::stableNetworkBatch<3,1>,&Engine::stableNetworkBatch<3,2>,&Engine::stableNetworkBatch<3,3>,&Engine::stableNetworkBatch<3,4>};
    static constexpr NetworkBatch four[]={&Engine::stableNetworkBatch<4,0>,&Engine::stableNetworkBatch<4,1>,&Engine::stableNetworkBatch<4,2>,&Engine::stableNetworkBatch<4,3>,&Engine::stableNetworkBatch<4,4>};
    networkScalar=scalar[filterRoute];networkThree=three[filterRoute];networkFour=four[filterRoute];
  }
  template<int N>void plainNetworkBatch(Voice**v,const PreparedVoice*p,const Voice&note,float*out){
    if(!dualEverUsed){filterBatch<N>(v,p,note,out);return;}
    bool stable=!note.dual.b.types.remaining&&!note.dual.b.slope.remaining;
    for(int i=0;i<N;++i)stable=stable&&!v[i]->dual.remaining&&v[i]->dual.route==filterRoute;
    if(!stable){for(int i=0;i<N;++i)out[i]=plainNetworkSample(*v[i],p[i].input,&note);return;}
    if constexpr(N==4)networkFour(v,p,note,out);else networkThree(v,p,note,out);
  }
  template<int N>void networkBatch(Voice**v,const PreparedVoice*p,const Voice&note,float*out){
    if(!distortionControl.live()){plainNetworkBatch<N>(v,p,note,out);return;}
    if(!distortionControl.place){PreparedVoice q[N];for(int i=0;i<N;++i)q[i]={distortedSample(*v[i],p[i].input),p[i].env1};plainNetworkBatch<N>(v,q,note,out);}
    else {plainNetworkBatch<N>(v,p,note,out);for(int i=0;i<N;++i)out[i]=distortedSample(*v[i],out[i]);}
  }
