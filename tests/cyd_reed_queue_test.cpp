#include "../cyd_rower/src/ReedEventQueue.h"
#include <assert.h>
#include <stdio.h>

int main() {
  cydreed::EventQueue<8> queue;
  cydreed::Edge edge;
  assert(!queue.pop(edge));
  for(uint32_t i=0;i<10;++i)queue.push(0xfffffff0UL+i,i&1);
  assert(queue.dropped==3);
  for(uint32_t i=0;i<7;++i) {
    assert(queue.pop(edge));
    assert(edge.timeUs==0xfffffff0UL+i && edge.high==bool(i&1));
  }
  assert(!queue.pop(edge));
  // Ring wraps while the timestamp also wraps.
  for(uint32_t i=0;i<30;++i) {
    queue.push(0xfffffff0UL+i,true);
    assert(queue.pop(edge));
    assert(edge.timeUs==uint32_t(0xfffffff0UL+i) && edge.high);
  }
  queue.push(123,false);
  queue.reset();
  assert(!queue.pop(edge) && queue.dropped==0);
  puts("PASS: raw edge FIFO order, overflow accounting, ring wrap and reset");
}
