#pragma once
// Deterministic active-clock race logic, independent of the selected UI.
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <math.h>
#include "../../ArduRower/RowerMetrics.h"

namespace ghostrace {
static constexpr uint16_t kIntervalMs = 2000; // legacy 1 km sample interval
static constexpr uint16_t kSamples = 1024;
static constexpr uint32_t kMagic = 0x47525731UL;
static constexpr uint16_t kVersion = 4;
static const uint32_t kScaleMilli = 700;
static constexpr uint8_t kDistanceCount = 3;
static constexpr uint16_t kDistances[kDistanceCount] = {1000, 3000, 10000};

struct Record {
  uint32_t magic;
  uint16_t version;
  uint16_t targetMeters;
  uint32_t scaleMilli;
  uint32_t finishMs;
  uint16_t count;
  // 0.1 m for 1/3 km, 0.2 m for 10 km (100000 dm would overflow).
  uint16_t decimeters[kSamples];
  uint16_t intervalSeconds; // occupies the former v3 padding, same NVS layout
  uint32_t checksum;
};
static_assert(offsetof(Record,checksum)==2068 && sizeof(Record)==2072,
              "Preserve legacy 1 km record layout");

inline uint32_t hashRecord(const Record &r) {
  const uint8_t *bytes = reinterpret_cast<const uint8_t *>(&r);
  uint32_t h = 2166136261UL;
  for (size_t i=0; i<offsetof(Record,checksum); ++i) {
    h ^= bytes[i]; h *= 16777619UL;
  }
  return h;
}
inline bool allowedDistance(uint16_t d) { return d==1000 || d==3000 || d==10000; }
inline uint16_t baseIntervalSeconds(uint16_t d) { return d==10000 ? 10 : d==3000 ? 4 : 2; }
inline uint32_t intervalMs(const Record &r) {
  return r.version==3 ? kIntervalMs : static_cast<uint32_t>(r.intervalSeconds)*1000U;
}
inline uint16_t unitsPerMeter(uint16_t target) { return target==10000 ? 5 : 10; }
inline float sampleMeters(const Record &r,uint16_t index) {
  return static_cast<float>(r.decimeters[index])/unitsPerMeter(r.targetMeters);
}
inline bool valid(const Record &r,uint16_t expectedTarget,uint32_t expectedCalibration=kScaleMilli) {
  const bool legacy=r.version==3 && expectedTarget==1000;
  if (r.magic!=kMagic || (!legacy && r.version!=kVersion) ||
      r.targetMeters!=expectedTarget || !allowedDistance(r.targetMeters) ||
      r.scaleMilli!=expectedCalibration || !r.finishMs || !r.count ||
      r.count>kSamples || (!legacy && r.intervalSeconds<baseIntervalSeconds(expectedTarget)) ||
      r.checksum!=hashRecord(r)) return false;
  const uint32_t step=intervalMs(r);
  if (!step || r.count-1U>r.finishMs/step ||
      (legacy && r.finishMs>static_cast<uint32_t>(kSamples)*step) ||
      (!legacy && r.count!=r.finishMs/step+1U)) return false;
  if (r.decimeters[0]!=0) return false;
  const uint16_t limit=static_cast<uint16_t>(expectedTarget*unitsPerMeter(expectedTarget));
  for (uint16_t i=0;i<r.count;++i)
    if (r.decimeters[i]>limit || (i && r.decimeters[i]<r.decimeters[i-1])) return false;
  return true;
}
inline float interpolate(const Record &r,uint32_t ms) {
  if (ms>=r.finishMs) return static_cast<float>(r.targetMeters);
  if (!r.count || !intervalMs(r)) return 0;
  const uint32_t step=intervalMs(r),ix=ms/step;
  if (ix+1<r.count) {
    const float d0=sampleMeters(r,ix),d1=sampleMeters(r,ix+1);
    return d0+(d1-d0)*(ms%step/static_cast<float>(step));
  }
  const uint32_t lastMs=(r.count-1U)*step;
  const float last=sampleMeters(r,r.count-1);
  if (r.finishMs<=lastMs) return last;
  const float f=(ms-lastMs)/static_cast<float>(r.finishMs-lastMs);
  return last+(r.targetMeters-last)*f;
}
inline float elapsedAtDistance(const Record &r,float distanceMeters) {
  if (distanceMeters<=0 || !r.count) return 0;
  if (distanceMeters>=r.targetMeters) return r.finishMs;
  const uint32_t step=intervalMs(r);
  float previous=0;
  for (uint16_t i=1;i<r.count;++i) {
    const float d=sampleMeters(r,i);
    if (d>=distanceMeters && d>previous)
      return (i-1U)*step+(distanceMeters-previous)/(d-previous)*step;
    previous=d;
  }
  const float last=sampleMeters(r,r.count-1);
  const uint32_t lastMs=(r.count-1U)*step;
  if (r.targetMeters<=last || r.finishMs<=lastMs) return r.finishMs;
  return lastMs+(distanceMeters-last)/(r.targetMeters-last)*(r.finishMs-lastMs);
}

// Positive lead = faster. Full green/red at 5% of ghost time at the
// same distance, with a 3 s minimum span to keep the opening stable.
inline uint16_t leadColor565(float leadMs,float referenceMs) {
  const float span=fmaxf(3000.0f,referenceMs*0.05f);
  const float amount=fminf(1.0f,fabsf(leadMs)/span);
  const unsigned fade=static_cast<unsigned>(lroundf(255.0f*(1.0f-amount)));
  const unsigned red=leadMs>0 ? fade : 255U;
  const unsigned green=leadMs<0 ? fade : 255U;
  return static_cast<uint16_t>(((red&0xf8U)<<8)|((green&0xfcU)<<3));
}

enum class Phase : uint8_t { Idle, Armed, Running, Finished, Cancelled };
class Race {
 public:
  Record best = {},attempt = {};
  Phase phase=Phase::Idle;
  bool hasBest=false,personalBest=false,overflow=false;
  uint16_t targetMeters=0;
  uint32_t calibrationMilli=kScaleMilli,startMs=0,endMs=0;
  uint32_t lastMs=0;
  float meters=0;

  void arm(uint16_t target,const Record *loadedBest,uint32_t activeCalibration=kScaleMilli) {
    calibrationMilli=activeCalibration;targetMeters=target;
    hasBest=loadedBest && valid(*loadedBest,target,calibrationMilli);
    if (hasBest) memcpy(&best,loadedBest,sizeof(best)); else memset(&best,0,sizeof(best));
    memset(&attempt,0,sizeof(attempt));
    attempt.magic=kMagic;attempt.version=kVersion;attempt.targetMeters=target;
    attempt.scaleMilli=calibrationMilli;attempt.count=1;
    attempt.intervalSeconds=baseIntervalSeconds(target);
    phase=allowedDistance(target)?Phase::Armed:Phase::Cancelled;
    meters=0;startMs=endMs=lastMs=0;personalBest=overflow=false;
  }
  void start(uint32_t nowMs) {
    if(phase!=Phase::Armed)return;
    startMs=nowMs;phase=Phase::Running;
  }
  void cancel() {
    if(phase==Phase::Running || phase==Phase::Armed)phase=Phase::Cancelled;
  }
  uint32_t elapsed(uint32_t nowMs) const {
    if(phase==Phase::Running)return nowMs-startMs;
    return phase==Phase::Finished?endMs:0;
  }
  float ghostMeters(uint32_t nowMs) const { return hasBest?interpolate(best,elapsed(nowMs)):0; }
  float meterLead(uint32_t nowMs) const { return meters-ghostMeters(nowMs); }
  float timeLeadMs(uint32_t nowMs) const {
    return hasBest ? elapsedAtDistance(best,meters)-elapsed(nowMs) : 0;
  }
  uint16_t timeLeadColor(uint32_t nowMs) const {
    return leadColor565(timeLeadMs(nowMs),hasBest?elapsedAtDistance(best,meters):0);
  }
  bool beatGhost() const { return phase==Phase::Finished && hasBest && personalBest && !overflow; }

  // Each distance is tracked from the same workout start. When the array
  // fills, retain every second sample and double the interval instead of
  // abandoning a slow record. The finish time is independent of sampling.
  bool advance(uint32_t nowMs,float distance) {
    if(phase!=Phase::Running)return false;
    uint32_t ms=nowMs-startMs;
    const float previous=meters;
    if(isfinite(distance) && distance>meters) {
      if(distance>=targetMeters) {
        if(ms>lastMs && distance>previous)
          ms=lastMs+static_cast<uint32_t>(lround((ms-lastMs)*
              (static_cast<double>(targetMeters)-previous)/(distance-previous)));
        meters=targetMeters;
      } else meters=distance;
    }
    if(meters>=targetMeters && !ms)ms=1;
    while(ms/intervalMs(attempt)>=kSamples) {
      const uint16_t retained=(attempt.count+1U)/2U;
      for(uint16_t i=0;i<retained;++i)attempt.decimeters[i]=attempt.decimeters[i*2U];
      memset(attempt.decimeters+retained,0,(kSamples-retained)*sizeof(uint16_t));
      attempt.count=retained;
      attempt.intervalSeconds*=2U;
    }
    const uint32_t step=intervalMs(attempt);
    const uint16_t limit=static_cast<uint16_t>(targetMeters*unitsPerMeter(targetMeters));
    while(attempt.count<=ms/step) {
      const uint32_t sampleMs=static_cast<uint32_t>(attempt.count)*step;
      float d=meters;
      if(ms>lastMs && sampleMs>=lastMs)
        d=previous+(meters-previous)*((sampleMs-lastMs)/static_cast<float>(ms-lastMs));
      else if(sampleMs<lastMs)d=previous;
      uint16_t encoded=static_cast<uint16_t>(lroundf(d*unitsPerMeter(targetMeters)));
      if(encoded>=limit)encoded=(meters>=targetMeters && sampleMs==ms)?limit:limit-1U;
      if(encoded<attempt.decimeters[attempt.count-1])encoded=attempt.decimeters[attempt.count-1];
      attempt.decimeters[attempt.count++]=encoded;
    }
    lastMs=ms;
    if(meters<targetMeters)return false;
    phase=Phase::Finished;endMs=ms;attempt.finishMs=endMs;
    attempt.checksum=hashRecord(attempt);
    personalBest=!hasBest || endMs<best.finishMs;
    return personalBest;
  }
};
} // namespace ghostrace
