// GPL-2.0-or-later. Editor presentation only; source identities stay stable.
#pragma once
#include "ExpansionParameters.h"
namespace xs::sourceMenu {
inline const char* label(int source){return source?Sources[source]:"Off";}
inline constexpr int Order[]={0,1,11,12,13,2,3,4,5,6,7,8,9,10};
constexpr int identity(int displayed){return Order[displayed];}
constexpr int position(int source){for(int i=0;i<SourceCount;++i)if(Order[i]==source)return i;return 0;}
static_assert(sizeof(Order)/sizeof(Order[0])==SourceCount,"Every source must appear in the editor menu");
} // namespace xs::sourceMenu
