// GPL-2.0-or-later. Three fixed document slots; only non-RT producer owns storage.
#pragma once
#include "StateExchange.h"
namespace xs {
class DocumentExchange {
  struct Slot {table::Document assets;StateMessage message;std::atomic<unsigned>status{0};};
  std::array<Slot,3>slots;
  std::atomic<int>pending{-1};
  std::mutex uiOnly;
  StateMailbox outgoing;
  StateMessage latest;
  uint64_t requested=0;
  int latestSlot=0;
public:
  DocumentExchange(){slots[0].status=2;}
  bool restore(const State&s,const table::Document&doc={},bool preserve=false){
    std::lock_guard<std::mutex>lock(uiOnly);
    int selected=-1;
    for(int i=0;i<3;++i){unsigned free=0;if(slots[i].status.compare_exchange_strong(free,3,std::memory_order_acq_rel)){selected=i;break;}}
    if(selected<0)return false; // Caller may retry off RT; never wait in render.
    auto&slot=slots[selected];slot.assets=doc;slot.message={s,++requested,preserve};latest=slot.message;latestSlot=selected;
    slot.status.store(1,std::memory_order_release);
    int superseded=pending.exchange(selected,std::memory_order_acq_rel);
    if(superseded>=0){slots[superseded].assets={};slots[superseded].status.store(0,std::memory_order_release);}
    return true;
  }
  bool audioRestore(StateMessage&message,const table::Document*&doc,int&owner){
    int index=pending.exchange(-1,std::memory_order_acq_rel);if(index<0)return false;
    slots[index].status.store(2,std::memory_order_release);message=slots[index].message;doc=&slots[index].assets;owner=index;return true;
  }
  void audioPublish(const State&s,uint64_t generation,unsigned owners,bool numericChanged=true){
    if(numericChanged)outgoing.publish({s,generation});
    for(int i=0;i<3;++i)if(!(owners&(1u<<i))&&slots[i].status.load(std::memory_order_acquire)==2)slots[i].status.store(0,std::memory_order_release);
  }
  std::pair<State,table::Document>snapshot(){
    std::lock_guard<std::mutex>lock(uiOnly);StateMessage message;
    if(outgoing.consume(message)&&message.generation>=requested)latest=message;
    return {latest.value,slots[latestSlot].assets};
  }
};
static_assert(std::atomic<int>::is_always_lock_free,"Document mailbox must be lock free");
}
