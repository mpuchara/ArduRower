#pragma once
#include "GhostRace.h"
namespace ghostrace {
struct HeartRecord {
  uint32_t magic=0x47485231;
  uint32_t raceChecksum=0;
  uint16_t count=0;
  uint16_t reserved=0; // sample interval in seconds; 0 = legacy 2 s
  uint8_t bpm[kSamples]={};
  uint32_t checksum=0;
};
inline uint32_t hashHeart(const HeartRecord &r) {
  const uint8_t *p=reinterpret_cast<const uint8_t*>(&r);
  uint32_t h=2166136261UL;
  for(size_t i=0;i<offsetof(HeartRecord,checksum);++i){h^=p[i];h*=16777619UL;}
  return h;
}
inline bool validHeart(const HeartRecord &r,const Record &race) {
  return r.magic==0x47485231 && r.raceChecksum==race.checksum &&
         r.count==race.count && r.count>0 && r.count<=kSamples &&
         (r.reserved?r.reserved*1000U:kIntervalMs)==intervalMs(race) &&
         r.checksum==hashHeart(r);
}
inline uint8_t replayHeart(const HeartRecord &r,uint32_t elapsed) {
  const uint32_t i=elapsed/(r.reserved?r.reserved*1000U:kIntervalMs);
  return i<r.count && i<kSamples?r.bpm[i]:0;
}
// Downsample the optional HR sidecar exactly like the distance trace.
// Do not invent HR samples for times between loop ticks.
inline void captureHeart(HeartRecord &heart,const Record &race,uint8_t bpm) {
  const uint16_t next=race.intervalSeconds;
  if(!heart.count) {heart.count=1;heart.reserved=next;heart.bpm[0]=bpm;}
  if(heart.reserved!=next) {
    const uint16_t factor=next/heart.reserved;
    const uint16_t retained=(heart.count+factor-1U)/factor;
    for(uint16_t i=0;i<retained;++i)heart.bpm[i]=heart.bpm[i*factor];
    memset(heart.bpm+retained,0,kSamples-retained);
    heart.count=retained;heart.reserved=next;
  }
  if(race.count>heart.count) {
    heart.bpm[race.count-1]=bpm;
    heart.count=race.count;
  }
}
}
