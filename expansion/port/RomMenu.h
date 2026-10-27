// GPL-2.0-or-later. UI order only; the bank and stored identities never move.
#pragma once
#include "DocumentAssets.h"
#include <algorithm>
#include <numeric>
namespace xs::romMenu {
inline constexpr const char* CategoryNames[]={"SPECTRAL","PULSE","FORMANT","DIGITAL","MODULATION","INSTRUMENTS","PAD","HARMONY","SFX"};
// Display taxonomy is independent of stored ROM IDs and provenance families.
inline int category(const table::Table&t){
  int id=t.id;
  if(id==0||id==1||id==7||(id>=36&&id<=37)||(id>=45&&id<=48))return 0;
  if(id==2||id==38||id==39)return 1;
  if(id==3||id==15||id==26)return 2;
  if(id==4||id==5||(id>=49&&id<=51))return 3;
  if(id==6||(id>=40&&id<=44))return 4;
  if((id>=8&&id<=17)||(id>=52&&id<=55))return 5;
  if((id>=18&&id<=28)||id==35)return 6;
  if((id>=29&&id<=34)||(id>=56&&id<=59)||id==61||(id>=66&&id<=68))return 7;
  return 8;
}
inline std::vector<int> order(const table::Bank&bank){
  std::vector<int> indices(bank.tables.size());std::iota(indices.begin(),indices.end(),0);
  auto first=std::find_if(indices.begin(),indices.end(),[&](int i){return bank.tables[i].id==20;});
  auto second=std::find_if(indices.begin(),indices.end(),[&](int i){return bank.tables[i].id==35;});
  if(first!=indices.end()&&second!=indices.end()){
    int index=*second;indices.erase(second);
    first=std::find_if(indices.begin(),indices.end(),[&](int i){return bank.tables[i].id==20;});
    indices.insert(first+1,index);
  }
  std::stable_sort(indices.begin(),indices.end(),[&](int a,int b){return category(bank.tables[a])<category(bank.tables[b]);});
  return indices;
}
} // namespace xs::romMenu
