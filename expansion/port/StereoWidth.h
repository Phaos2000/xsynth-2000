// GPL-2.0-or-later. Included inside Engine. Parameter-driven, never signal-driven.
  std::array<float,8> panLeft=[] {std::array<float,8>a{};a.fill(1);return a;}(),panRight=panLeft;
  std::array<float,8> panTargetLeft=panLeft,panTargetRight=panLeft,panStepLeft{},panStepRight{};
  float cachedWidth=-1,preOutputLeft=0,preOutputRight=0,outputLeftValue=0,outputRightValue=0;
  int cachedPanCount=-1,panRemaining=0;
  void widthTargets(){
    int count=groupLive?std::max(1,groupSize):1;float width=float(state[Width]);
    if(width==cachedWidth&&count==cachedPanCount)return;
    cachedWidth=width;cachedPanCount=count;panRemaining=transitionSamples;
    for(int m=0;m<8;++m){float position=count>1&&m<count?float(2*m-(count-1))/(count-1):0;
      // Each voice has L^2+R^2=2 at every target. Center is1/1, hard pan sqrt2/0.
      float p=width*position;panTargetLeft[m]=std::sqrt(1-p);panTargetRight[m]=std::sqrt(1+p);
      panStepLeft[m]=(panTargetLeft[m]-panLeft[m])/panRemaining;panStepRight[m]=(panTargetRight[m]-panRight[m])/panRemaining;
    }
  }
  void advanceWidth(){if(dualControlFrames>0)--dualControlFrames;if(panRemaining){for(int m=0;m<8;++m){panLeft[m]+=panStepLeft[m];panRight[m]+=panStepRight[m];}if(!--panRemaining){panLeft=panTargetLeft;panRight=panTargetRight;}}}
  void resetWidth(){panLeft.fill(1);panRight=panTargetLeft=panTargetRight=panLeft;panStepLeft.fill(0);panStepRight.fill(0);cachedWidth=-1;cachedPanCount=-1;panRemaining=0;preOutputLeft=preOutputRight=outputLeftValue=outputRightValue=0;}
