// GPL-2.0-or-later. Shared note calculation; member signal histories stay local.
  bool noteSharingEnabled=true;
  bool noteSharingNeedsSettle=true;
  bool noteSharingSafe=true;
  bool liveNoteSharingSafe=true;
  bool sharedDirectionControl=false;
  bool sharedDiscreteControls=false;
#ifdef XSYNTH_DISCRETE_DIAGNOSTICS
  bool discreteModulationEnabled=true;
#endif
  static void copyNoteState(Voice&to,const Voice&from){
    std::copy_n(from.eg,2,to.eg);to.selection=from.selection;to.syncedEg2=from.syncedEg2;if(from.dual.b.types.target>=0)copyFilterControl(to.dual.b,from.dual.b);
    std::copy_n(from.envCurve,2,to.envCurve);to.egLoop=from.egLoop;
    std::copy_n(from.egVelocity,2,to.egVelocity);std::copy_n(from.egPitch,2,to.egPitch);std::copy_n(from.egFilter,2,to.egFilter);
    to.lfoPitch=from.lfoPitch;to.lfoFilter=from.lfoFilter;to.tracking=from.tracking;to.tune=from.tune;to.glideRate=from.glideRate;
    to.mod=from.mod;to.stepTarget=from.stepTarget;to.stepValue=from.stepValue;to.stepDelta=from.stepDelta;to.stepRemaining=from.stepRemaining;
    to.modSmoothing=from.modSmoothing;to.controlDestinations=from.controlDestinations;
    std::copy_n(from.pitchRatio,2,to.pitchRatio);std::copy_n(from.pitchStep,2,to.pitchStep);
    for(int i=0;i<2;++i){std::copy_n(from.envelopeRate[i],3,to.envelopeRate[i]);std::copy_n(from.envelopeStep[i],3,to.envelopeStep[i]);}
    to.hz=from.hz;to.damping=from.damping;to.dampingStep=from.dampingStep;
    to.highG=from.highG;to.highStep=from.highStep;
    to.cachedCutoff=from.cachedCutoff;to.cachedHighCoefficient=from.cachedHighCoefficient;to.cachedDamping=from.cachedDamping;to.cachedCalibration=from.cachedCalibration;to.cachedFilterType=from.cachedFilterType;to.cachedFilterStages=from.cachedFilterStages;
    // Followers use the leader's transition weights while sharing. Materialize
    // those weights before independent processing resumes; never copy phases.
    to.topology=from.topology;
    to.control=from.control;
  }
  static int topologyDominant(const Voice&v){int result=0;for(int t=1;t<4;++t)if(v.topology.weights[t]>v.topology.weights[result])result=t;return result;}
  void adoptSharedTopology(Voice&v,const Voice&note){
    int target=note.topology.target;
    if(target<0||target==v.topology.target)return;
    int dominant=groups[v.group].topologyDominant;
    v.topologyPhase[target]=v.topology.target<0?std::array<float,2>{v.phase[0],v.phase[1]}:v.topologyPhase[dominant];
    seedPulsePath(v,target,dominant);
    v.topologyWrapped[target]=v.topology.target<0?v.masterWrapped:v.topologyWrapped[dominant];
    v.topology.target=target;
  }
  void unshareNotes(){
    if(groupSize<2)return;
    for(int g=0;g<std::min(groupLimit,renderGroups());++g)if(groups[g].noteShared){
      auto&leader=pool[g*groupSize];
      for(int m=1;m<groupSize;++m)if(pool[g*groupSize+m].active){auto&v=pool[g*groupSize+m];copyNoteState(v,leader);if(groups[g].discreteShared){v.cross=leader.cross;v.mix=leader.mix;v.choicesGeneration=leader.choicesGeneration;v.resonanceStrength=leader.resonanceStrength;v.resonanceStep=leader.resonanceStep;}}
      groups[g].noteShared=false;groups[g].discreteShared=false;
    }
  }
  void selectSharedNotes(){
    if(groupSize<2)return;
    if(!noteSharingEnabled||!liveNoteSharingSafe||topologyRemaining){if(!liveNoteSharingSafe)XS_DISCRETE_COUNT(2,1);unshareNotes();return;}
    for(int g=0;g<std::min(groupLimit,renderGroups());++g){auto&group=groups[g];auto&leader=pool[g*groupSize];
      // Ordinary events materialize first; a finished shared group is retired
      // after all members render their last sample, preserving this leader.
      if(group.noteShared&&!leader.active){unshareNotes();return;}
      if(group.sharingDelay){--group.sharingDelay;continue;}
      // The existing fade is applied after group summation. It does not
      // make otherwise coherent note controls or member histories independent.
      if(!group.active||group.noteShared||!group.noteCoherent)continue;
      bool ready=leader.active&&leader.control!=0&&!leader.firstSample;
      for(int m=0;m<groupSize&&ready;++m){const auto&v=pool[g*groupSize+m];ready=v.active&&!v.retiring&&(!v.topology.remaining||sharedDirectionControl)&&(!v.cross.remaining||sharedDiscreteControls)&&(!v.mix.remaining||sharedDiscreteControls)&&!v.types.remaining&&!v.slope.remaining;
        for(int o=0;o<2;++o)ready=ready&&!v.waves[o].remaining&&!v.engines[o].remaining;
        if(ready&&sharedDiscreteControls&&m){ready=v.resonanceStrength==leader.resonanceStrength&&v.resonanceStep==leader.resonanceStep&&v.selection==leader.selection&&!std::memcmp(&v.cross,&leader.cross,sizeof(v.cross))&&!std::memcmp(&v.mix,&leader.mix,sizeof(v.mix));if(!ready)XS_DISCRETE_COUNT(5,1);}}
      group.noteShared=ready;group.discreteShared=ready&&sharedDiscreteControls;
    }
  }
