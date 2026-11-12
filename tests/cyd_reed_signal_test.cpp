#include "../cyd_rower/src/ReedSignalFilter.h"
#include "../cyd_rower/src/ReedTrace.h"
#include <assert.h>
#include <stdio.h>
#include <string>

int main() {
  cydreed::Filter filter;
  cydreed::Pulse pulse;
  filter.reset(0,true);
  // Contact chatter is grouped into one closure.
  assert(!filter.edge(1000,false,pulse));
  assert(!filter.edge(1080,true,pulse));
  assert(!filter.edge(1160,false,pulse));
  assert(!filter.edge(3000,true,pulse));
  assert(filter.poll(3300,pulse));
  assert(pulse.timeUs==1160 && pulse.lowUs==1840 && filter.accepted==1);
  assert(!filter.poll(4000,pulse));
  assert(!filter.edge(5000,false,pulse)); // electrical spike
  assert(!filter.edge(5100,true,pulse));
  assert(!filter.poll(6000,pulse) && filter.accepted==1);
  assert(filter.shortLow==2);
  // Genuine 13 ms periods remain intact, independent of a fixed dead time.
  filter.reset(0,true);
  for(uint32_t t=1000;t<40000;t+=13000) {
    assert(!filter.edge(t,false,pulse));
    assert(!filter.edge(t+1200,true,pulse));
    assert(filter.poll(t+1500,pulse));
    if(t>1000)assert(pulse.gapUs==13000);
  }
  assert(filter.accepted==3);
  cydreed::Filter narrow;
  narrow.reset(0,true);
  for(uint32_t t=1000;t<40000;t+=13000) {
    assert(!narrow.edge(t,false,pulse));
    assert(!narrow.edge(t+12360,true,pulse));
    assert(narrow.poll(t+12660,pulse)); // stable high only 640 us
  }
  assert(narrow.accepted==3);
  // A same-magnet return is valid if it opens stably.
  assert(!filter.edge(32000,false,pulse));
  assert(!filter.edge(33000,true,pulse));
  assert(filter.poll(33300,pulse) && pulse.gapUs==5000);
  assert(!filter.edge(32000,false,pulse)); // older than the last edge
  assert(filter.outOfOrder==1);
  // A held-closed contact emits no extra events at a reversal.
  assert(!filter.edge(40000,false,pulse));
  assert(!filter.poll(100000,pulse));
  assert(!filter.poll(200000,pulse));
  assert(!filter.edge(210000,true,pulse));
  assert(filter.poll(210300,pulse) && pulse.lowUs==170000);
  filter.reset(0xfffff000UL,true);
  assert(!filter.edge(0xfffffff0UL,false,pulse));
  assert(!filter.edge(1000,true,pulse));
  assert(filter.poll(1300,pulse) && pulse.lowUs==1016);
  struct Port {
    std::string output;
    int space=0;
    int availableForWrite() { return space; }
    size_t write(const uint8_t *data,size_t size) {
      assert(size<=static_cast<size_t>(space));
      output.append(reinterpret_cast<const char*>(data),size);
      return size;
    }
  } port;
  ReedTrace trace;
  trace.line("edge\n",5);
  trace.flush(port);
  assert(port.output.empty());
  port.space=3;trace.flush(port);assert(port.output.empty());
  port.space=5;trace.flush(port);
  assert(port.output=="edge\n");
  for(int i=0;i<2000;++i)trace.line("edge\n",5);
  assert(trace.dropped>0);
  puts("PASS: reed contact chatter, short glitches, 13ms periods, same magnet, held contact, wrap and bounded serial trace");
}
