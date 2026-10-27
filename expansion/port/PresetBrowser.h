// GPL-2.0-or-later. UI-only browser state; factory data are immutable and shared.
#pragma once
#include "FactoryData.h"
#include "InitState.h"
#include <string>
namespace xs::factory {
inline State presetState(int index){auto s=Presets[index].state;for(int i=PreDualCount;i<xs::Count;++i)s[i]=extraDefault(i);return s;}
inline bool sameSound(const State &a, const State &b) {
  for (int id = 0; id < xs::Count; ++id) {
    if (id == 30 || id == 31 || id == AllNotesOff || id == AllSoundsOff || id == Pressure)
      continue;
    if (std::abs(a[id] - b[id]) > 1e-12)
      return false;
  }
  return true;
}
class Browser {
  int selected = -1;
  State baseline = defaults();
  std::string external;

public:
  int current() const { return selected; }
  int adjacent(int direction) const {
    if(selected==Init)return direction>0?0:Count-1;
    if(selected<0)return direction>0?Init:Count-1;
    if(direction<0&&selected==0)return Init;
    if(direction>0&&selected==Count-1)return Init;
    return selected+direction;
  }
  void select(int index) {
    if(index==Init){selected=Init;baseline=initState();external.clear();return;}
    if (index >= 0 && index < Count) {
      selected = index;
      baseline = presetState(index);
      external.clear();
    }
  }
  void loaded(const State &state, const std::string &name) {
    selected = -1;
    baseline = state;
    external = name;
  }
  std::string label(const State &state) {
    if(selected==-1&&external.empty()&&sameSound(state,initState()))select(Init);
    if (selected == -1 && external.empty())
      for (int i = 0; i < Count; ++i)
        if (sameSound(state, presetState(i))) {
          select(i);
          break;
        }
    std::string name = selected==Init?"INIT":selected >= 0                  ? Presets[selected].name
                       : !external.empty()            ? external
                       : (sameSound(state, defaults()) || sameSound(state, newInstanceState())) ? "DEFAULT"
                                                      : "CUSTOM";
    if ((selected != -1 || !external.empty()) && !sameSound(state, baseline))
      name += " *";
    return name;
  }
};
} // namespace xs::factory
