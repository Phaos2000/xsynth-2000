// GPL-2.0-or-later. UI order only; the bank and stored identities never move.
#pragma once
#include "DocumentAssets.h"
#include <algorithm>
#include <numeric>
namespace xs::romMenu {
inline std::vector<int> order(const table::Bank&bank){
  std::vector<int> indices(bank.tables.size());std::iota(indices.begin(),indices.end(),0);
  auto first=std::find_if(indices.begin(),indices.end(),[&](int i){return bank.tables[i].id==20;});
  auto second=std::find_if(indices.begin(),indices.end(),[&](int i){return bank.tables[i].id==35;});
  if(first!=indices.end()&&second!=indices.end()){
    int index=*second;indices.erase(second);
    first=std::find_if(indices.begin(),indices.end(),[&](int i){return bank.tables[i].id==20;});
    indices.insert(first+1,index);
  }
  auto added=std::find_if(indices.begin(),indices.end(),[&](int i){return bank.tables[i].id>=36;});
  std::stable_sort(added,indices.end(),[&](int a,int b){return bank.tables[a].familyId<bank.tables[b].familyId;});
  return indices;
}
} // namespace xs::romMenu
