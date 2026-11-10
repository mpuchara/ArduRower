#pragma once
// Host-testable vertical scales for the two-unit CYD trend chart.
// Both scales use only points currently visible in the 10-minute window.
// Missing heart-rate notifications (0 BPM) do not contribute to the range.
#include <stdint.h>

namespace cydplot {
struct AutoScale {
  uint16_t speedMaxTenth=10; // 1 km/h when the graph is empty
  uint16_t heartMin=50, heartMax=100;
  bool hasHeartRate=false;

  void includeSpeed(uint16_t tenthKmh) {
    if(tenthKmh>maxSpeedObserved) maxSpeedObserved=tenthKmh;
  }
  void includeHeart(uint8_t bpm) {
    if(!bpm)return;
    if(!hasHeartRate || bpm<minHeartObserved)minHeartObserved=bpm;
    if(!hasHeartRate || bpm>maxHeartObserved)maxHeartObserved=bpm;
    hasHeartRate=true;
  }
  void finish() {
    // One easy-to-read whole km/h above the largest plotted sample. The
    // upper tick therefore always includes the maximum visible value.
    speedMaxTenth=static_cast<uint16_t>((maxSpeedObserved/10U+1U)*10U);
    if(!hasHeartRate)return;
    // Dynamic HR window: 10 BPM of readable padding on each side.
    const int lower=static_cast<int>(minHeartObserved/10U)*10-10;
    heartMin=static_cast<uint16_t>(lower>0?lower:0);
    heartMax=static_cast<uint16_t>((maxHeartObserved/10U+1U)*10U);
  }
  int speedY(uint16_t tenthKmh,int height) const {
    if(height<=1)return 0;
    const uint16_t bounded=tenthKmh>speedMaxTenth?speedMaxTenth:tenthKmh;
    return height-1-static_cast<int>(static_cast<uint32_t>(bounded)*(height-1)/speedMaxTenth);
  }
  int heartY(uint8_t bpm,int height) const {
    if(height<=1)return 0;
    const uint16_t bounded=bpm<heartMin?heartMin:(bpm>heartMax?heartMax:bpm);
    return height-1-static_cast<int>(
        static_cast<uint32_t>(bounded-heartMin)*(height-1)/(heartMax-heartMin));
  }
 private:
  uint16_t maxSpeedObserved=0;
  uint8_t minHeartObserved=0,maxHeartObserved=0;
};
} // namespace cydplot
