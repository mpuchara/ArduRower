#pragma once
#include <stdint.h>
#include <stddef.h>

namespace watchhr {
// v1, valid flag, sequence LE16, BPM LE16. Unknown HR is an explicit
// heartbeat with flag=0/BPM=0, never a fabricated physiological value.
static const char *const kService="00001826-0000-1000-8000-00805f9b34fb";
static const char *const kMeasurement="c9f10002-6f50-4fca-9b8d-3f8d90ae1b65";
struct Receiver {
  bool haveSequence=false;
  uint16_t sequence=0;
  void reset(){haveSequence=false;sequence=0;}
  bool accept(const uint8_t *p,size_t n,uint16_t &bpm) {
    if(!p || n!=6 || p[0]!=1 || p[1]>1)return false;
    const uint16_t next=p[2]|(uint16_t(p[3])<<8);
    const uint16_t value=p[4]|(uint16_t(p[5])<<8);
    if(p[1] ? (value<35 || value>240) : value!=0)return false;
    const uint16_t delta=next-sequence;
    if(haveSequence && (delta==0 || delta>=0x8000))return false;
    sequence=next;haveSequence=true;bpm=value;return true;
  }
};
}
