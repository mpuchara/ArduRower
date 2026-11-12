#pragma once
#include <stdint.h>

namespace cydreed {
struct Edge {
  uint32_t timeUs;
  bool high;
};

// Single producer/consumer, called inside the platform's critical section.
template<uint16_t Capacity>
class EventQueue {
 public:
  volatile uint32_t dropped=0;

  inline __attribute__((always_inline)) void push(uint32_t timeUs,bool high) {
    const uint16_t next=(head+1)%Capacity;
    if(next==tail) { ++dropped; return; }
    events[head].timeUs=timeUs;
    events[head].high=high;
    head=next;
  }

  bool pop(Edge &out) {
    if(tail==head)return false;
    out.timeUs=events[tail].timeUs;
    out.high=events[tail].high;
    tail=(tail+1)%Capacity;
    return true;
  }

  void clear() { tail=head; }
  void reset() { clear(); dropped=0; }

 private:
  volatile Edge events[Capacity];
  volatile uint16_t head=0,tail=0;
};
} // namespace cydreed
