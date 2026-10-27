// GPL-2.0-or-later. Presentation only: leaf commands retain destination + 1.
#pragma once
#include "ExpansionParameters.h"
#include <windows.h>
namespace xs::destinationMenu {
inline constexpr const char* Groups[]={"VCO1","XMOD","VCO2","Mixer","A","B","LFO","EG1 (VCA)","EG2","Voice"};
inline constexpr int Group[]={0,0,0,2,2,2,1,3,4,4,6,6,7,7,7,8,8,8,
  0,2,1,1,3,4,4,4,6,6,6,6,7,7,7,7,8,8,8,8,8,9,9,9,0,2,5,5};
constexpr bool complete(){if(sizeof(Group)/sizeof(Group[0])!=DestinationCount)return false;for(int g:Group)if(g<0||g>=10)return false;return true;}
static_assert(complete(),"Every destination requires a module category");
inline constexpr int OffCommand=DestinationCount+1; // UI action; existing destination commands1..46 stay stable.
constexpr bool visible(int id){return id!=42&&id!=43;}
inline int step(int current,int delta,bool wrap=false){
 current=shapePositionIdentity(current);int position=current>=44?current-2:current;
 position=wrap?(position+delta+DestinationCount-2)%(DestinationCount-2):std::clamp(position+delta,0,DestinationCount-3);
 return position>=42?position+2:position;
}
inline HMENU createMenu(int current,bool off=false){
  current=shapePositionIdentity(current);
  HMENU root=CreatePopupMenu(),children[10]{};
  AppendMenuA(root,MF_STRING|(off?MF_CHECKED:0),OffCommand,"Off");
  for(auto&child:children)child=CreatePopupMenu();
  for(int id=0;id<DestinationCount;++id)if(visible(id))
    AppendMenuA(children[Group[id]],MF_STRING|(id==current&&!off?MF_CHECKED:0),id+1,Destinations[id]);
  for(int g=0;g<10;++g){
    if(g==4){HMENU filters=CreatePopupMenu();
      AppendMenuA(filters,MF_POPUP,reinterpret_cast<UINT_PTR>(children[4]),"A");
      AppendMenuA(filters,MF_POPUP,reinterpret_cast<UINT_PTR>(children[5]),"B");
      AppendMenuA(root,MF_POPUP,reinterpret_cast<UINT_PTR>(filters),"Filter");
    }else if(g!=5)AppendMenuA(root,MF_POPUP,reinterpret_cast<UINT_PTR>(children[g]),Groups[g]);
  }
  return root;
}
} // namespace xs::destinationMenu
