// GPL-2.0-or-later. SPSC triple buffers; UI callers serialize with a UI-only mutex.
#pragma once
#include "Core.h"
#include <atomic>
#include <mutex>
namespace xs {
struct StateMessage { State value=defaults(); uint64_t generation=0; bool preservePerformance=false; };
class StateMailbox {
  std::array<StateMessage,3> buffers{};
  std::atomic<unsigned> middle{1};
  unsigned producer=2, consumer=0;
public:
  void publish(const StateMessage &message) {
    buffers[producer]=message;
    producer=middle.exchange(producer|4,std::memory_order_acq_rel)&3;
  }
  bool consume(StateMessage &message) {
    if(!(middle.load(std::memory_order_acquire)&4))return false;
    consumer=middle.exchange(consumer,std::memory_order_acq_rel)&3;
    message=buffers[consumer]; return true;
  }
};
class StateExchange {
  StateMailbox incoming,outgoing;
  std::mutex uiOnly;
  StateMessage ui;
  uint64_t requested=0;
public:
  void restore(const State &s,bool preservePerformance=false) {
    std::lock_guard<std::mutex> lock(uiOnly);
    ui={s,++requested,preservePerformance}; incoming.publish(ui);
  }
  State snapshot() {
    std::lock_guard<std::mutex> lock(uiOnly);
    StateMessage message;
    if(outgoing.consume(message) && message.generation>=requested)ui=message;
    return ui.value;
  }
  bool audioRestore(StateMessage &s) { return incoming.consume(s); }
  void audioPublish(const State &s,uint64_t generation) { outgoing.publish({s,generation}); }
};
static_assert(std::atomic<unsigned>::is_always_lock_free,"Realtime mailbox needs lock-free unsigned atomics");
} // namespace xs
