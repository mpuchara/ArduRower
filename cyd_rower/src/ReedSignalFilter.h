#pragma once
#include <stdint.h>

namespace cydreed {
struct Pulse {
  uint32_t timeUs=0, lowUs=0, highUs=0, gapUs=0;
};

// Debounce state transitions, not the period between magnet crossings.
// A closure is published only after a stable opening confirms its end.
class Filter {
 public:
  enum class Decision : uint8_t {
    Pending, Confirmed, ShortLow, ShortHigh, SameLevel, OutOfOrder, Initial
  };
  uint32_t accepted=0, shortLow=0, shortHigh=0, outOfOrder=0;
  Decision decision=Decision::Initial;
  explicit Filter(uint32_t lowUs=300, uint32_t highUs=300)
      : minLowUs(lowUs), minHighUs(highUs) {}

  void reset(uint32_t nowUs, bool high) {
    initialized=true;
    rawHigh=stableHigh=high;
    rawSince=stableSince=nowUs;
    haveClosure=havePulse=false;
    accepted=shortLow=shortHigh=outOfOrder=0;
    decision=Decision::Initial;
  }

  bool edge(uint32_t nowUs, bool high, Pulse &pulse) {
    if(!initialized) { reset(nowUs,high); return false; }
    if(nowUs-rawSince>0x7fffffffUL) {
      ++outOfOrder;
      decision=Decision::OutOfOrder;
      return false;
    }
    const bool emitted=advance(nowUs,pulse);
    decision=high==rawHigh?Decision::SameLevel:Decision::Pending;
    if(high!=rawHigh) {
      if(rawHigh!=stableHigh) {
        if(rawHigh) { ++shortHigh; decision=Decision::ShortHigh; }
        else { ++shortLow; decision=Decision::ShortLow; }
      }
      rawHigh=high;
      rawSince=nowUs;
    }
    return emitted;
  }

  bool poll(uint32_t nowUs, Pulse &pulse) {
    return initialized && advance(nowUs,pulse);
  }

 private:
  uint32_t minLowUs, minHighUs;
  uint32_t rawSince=0, stableSince=0, closureUs=0, openUs=0, lastPulseUs=0;
  bool initialized=false, rawHigh=true, stableHigh=true;
  bool haveClosure=false, havePulse=false;

  bool advance(uint32_t nowUs, Pulse &pulse) {
    const uint32_t age=nowUs-rawSince;
    if(age>0x7fffffffUL || rawHigh==stableHigh ||
       age<(rawHigh?minHighUs:minLowUs))return false;
    const uint32_t duration=rawSince-stableSince;
    stableHigh=rawHigh;
    stableSince=rawSince;
    if(!stableHigh) {
      closureUs=rawSince;
      openUs=duration;
      haveClosure=true;
      return false;
    }
    if(!haveClosure)return false; // started with a closed contact
    pulse.timeUs=closureUs;
    pulse.lowUs=duration;
    pulse.highUs=openUs;
    pulse.gapUs=havePulse?closureUs-lastPulseUs:0;
    lastPulseUs=closureUs;
    havePulse=true;
    haveClosure=false;
    ++accepted;
    decision=Decision::Confirmed;
    return true;
  }
};
} // namespace cydreed
