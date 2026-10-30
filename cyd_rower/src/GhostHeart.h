#pragma once
#include "GhostRace.h"
namespace ghostrace {
// Optional sidecar: existing distance records stay compatible. A torn or
// older sidecar never attaches its pulse trace to a different best race.
struct HeartRecord {
  uint32_t magic=0x47485231;
  uint32_t raceChecksum=0;
  uint16_t count=0;
  uint16_t reserved=0;
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
         r.checksum==hashHeart(r);
}
inline uint8_t replayHeart(const HeartRecord &r,uint32_t elapsed) {
  const uint32_t i=elapsed/kIntervalMs;
  return i<r.count && i<kSamples?r.bpm[i]:0; // missing HR remains a gap
}
}
