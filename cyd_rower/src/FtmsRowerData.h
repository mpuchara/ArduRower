#pragma once
#include <math.h>
#include <stdint.h>

namespace ftmsrower {
constexpr uint8_t kMaxPacketSize=19;
constexpr uint8_t kFeatureByte0=0x26; // cadence, total distance, pace
constexpr uint8_t featureByte1(bool power) {return power?0x50:0x10;} // elapsed time, power

struct Sample {
  float cadence;
  uint32_t strokes;
  float averageCadence;
  float distance;
  float pace;
  float averagePace;
  float power;
  float averagePower;
  float elapsed;
};

inline void put16(uint8_t *&out,uint16_t value) {
  *out++=value&0xff;
  *out++=value>>8;
}

inline bool available(float value) {return isfinite(value) && value>=0;}

inline uint16_t rounded(float value,uint16_t maximum,uint16_t unavailable) {
  if(!available(value))return unavailable;
  return value>=maximum?maximum:static_cast<uint16_t>(roundf(value));
}

inline uint16_t paceSeconds(float value) {
  if(!available(value) || value<=0)return 0xffff;
  return rounded(value<1?1:value,0xfffe,0xffff);
}

// FTMS 4.8 and GSS 0x2AD1: little endian, pace in seconds / 500 m,
// power in signed 16-bit watts. Omit each power field until valid.
inline uint8_t encode(const Sample &sample,bool powerEnabled,uint8_t (&data)[kMaxPacketSize]) {
  const bool instantaneous=powerEnabled && available(sample.power);
  const bool average=powerEnabled && available(sample.averagePower);
  const uint16_t flags=0x081e | (instantaneous?0x20:0) | (average?0x40:0);
  uint8_t *out=data;
  put16(out,flags);
  *out++=rounded(sample.cadence*2,0xfe,0xff);
  put16(out,sample.strokes>0xfffe?0xfffe:sample.strokes);
  *out++=rounded(sample.averageCadence*2,0xfe,0xff);
  const uint32_t distance=!available(sample.distance)?0xffffff:
      sample.distance>=0xfffffe?0xfffffe:static_cast<uint32_t>(sample.distance);
  *out++=distance&0xff;
  *out++=(distance>>8)&0xff;
  *out++=distance>>16;
  put16(out,paceSeconds(sample.pace));
  put16(out,paceSeconds(sample.averagePace));
  if(instantaneous)put16(out,rounded(sample.power,0x7fff,0));
  if(average)put16(out,rounded(sample.averagePower,0x7fff,0));
  put16(out,rounded(sample.elapsed,0xfffe,0xffff));
  return out-data;
}
}
