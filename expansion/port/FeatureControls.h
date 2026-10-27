// GPL-2.0-or-later. UI projection of legacy Loop2 plus appended sync policy.
#pragma once
#include "Layout.h"
#include "Eg2Sync.h"
namespace xs::features {
inline int loopMode(const State&s){return s[Loop2]==0?0:1+discrete(Eg2Sync,s[Eg2Sync]);}
inline int cycle(const State&s,int remembered=0){return s[Loop2]==0?std::clamp(remembered,0,1):discrete(Loop2,s[Loop2])-1;}
inline int filterId(int id,int selected){if(!selected)return id;return id==21?VcfBCutoff:id==22?VcfBResonance:id==Keytrack?VcfBKeytrack:id==FilterType?VcfBType:id==28?VcfBSlope:id;}
inline bool rateValid(const State&s,int division,double tempo,double sr,int remembered=0){
 float rates[3];for(int i=0;i<3;++i){float c=float(physical(i==0?15:i==1?16:18,s[i==0?15:i==1?16:18]));rates[i]=float(-std::expm1(std::log1p(-c)*44100/sr));}
 return eg2::valid(rates,discrete(Curve2,s[Curve2]),float(s[17]),cycle(s,remembered)+1,division,tempo,sr);
}
inline bool isFeatureSelector(int id){return id==Eg2Sync||id==Eg2Rate||id==Loop2||id==VcfRoute;}
}
