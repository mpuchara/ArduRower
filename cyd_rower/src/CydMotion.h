#pragma once
// Experimental movement model for a four-magnet strap reel + one-way clutch.
// A reed switch cannot report direction: stroke phases are inferred from two
// repeatable slowdowns, NEVER assumed to be mechanically measured.
// Independent of Arduino: host-compiled regression tests exercise timestamps.
#include <stdint.h>
#include <math.h>
#include "StrokeCycle.h"

namespace cydmotion {
// A short burst of reed edges while paused does not restart the clock.
class PauseResumeGate {
 public:
  static constexpr uint8_t kRequiredPulses=4;
  void reset() { count=0; }

  const uint32_t *confirm(uint32_t nowUs) {
    if(count && nowUs-timestamps[count-1]>350000UL)reset();
    timestamps[count++]=nowUs;
    if(count<kRequiredPulses)return nullptr;
    reset();
    return timestamps;
  }

 private:
  uint32_t timestamps[kRequiredPulses]={};
  uint8_t count=0;
};

class Engine : public StrokeCycle {
 public:
  // Both directions contribute to the total pulse rate. The virtual
  // distance per accepted interval is provisional, not S4-calibrated.
  static constexpr float kMetersPerPulse = 0.23f;
  static constexpr float kDefaultCalibration = 0.70f;
  static constexpr float kSpeedTauSeconds = 3.5f;
  static constexpr uint32_t kCoastUs = 3000000;
  static constexpr float kStopTauSeconds = 1.5f;
  static constexpr uint32_t kPauseAfterUs = 8000000;
  static constexpr uint32_t kDeadTimeUs = 15000;
  static constexpr uint32_t kDistanceDeadTimeUs = 10000;
  static constexpr uint32_t kRearmGapUs = 1600000;
  float speed=0, meters=0, calibration=kDefaultCalibration;
  uint32_t lastTickUs=0;

  void reset(uint32_t nowUs) {
    resetCycle(nowUs);
    speed=meters=0;
    lastTickUs=lastDistancePulseUs=nowUs;
    distanceStarted=false;
  }
  void setCalibration(float value) {
    if(isfinite(value) && value>=0.3f && value<=1.5f) calibration=value;
  }
  uint32_t calibrationMilli() const {
    return static_cast<uint32_t>(calibration*1000.0f+0.5f);
  }
  void tick(uint32_t nowUs) {
    uint32_t duration=nowUs-lastTickUs;
    // Queued ISR timestamps can predate a recent loop tick if BLE/TFT work
    // delayed processing. Never mistake a stale sample for a 71-minute gap.
    if(!duration || duration>0x7fffffffUL)return;
    const uint32_t idleUs=nowUs-lastDistancePulseUs;
    if(distanceStarted && idleUs>=kPauseAfterUs) {
      speed=0;
    } else if(speed>0) {
      // Keep inertia across the stroke. Once an extended break starts,
      // cross the coast boundary at the same time regardless of loop jitter.
      const uint32_t beyondCoastUs=idleUs>kCoastUs?idleUs-kCoastUs:0;
      const uint32_t stopUs=beyondCoastUs<duration?beyondCoastUs:duration;
      const float decay=(duration-stopUs)/kSpeedTauSeconds+stopUs/kStopTauSeconds;
      speed*=expf(-decay/1000000.0f);
    }
    if(started && nowUs-lastPulseUs>=kPauseAfterUs)spm=0;
    lastTickUs=nowUs;
  }
  void signalLost(uint32_t nowUs) {
    resetSynchronization(nowUs);
    distanceStarted=false;
    lastDistancePulseUs=nowUs;
    speed=0;
  }
  // Compatibility input for timestamp-only recordings from older firmware.
  void onPulse(uint32_t nowUs, bool strokePulse=true) {
    feedPulse(nowUs,strokePulse,false);
  }
  void onVerifiedPulse(uint32_t nowUs) {
    feedPulse(nowUs,true,true);
  }
 private:
  void feedPulse(uint32_t nowUs, bool strokePulse, bool verified) {
    tick(nowUs);
    const uint32_t distanceGap=nowUs-lastDistancePulseUs;
    if(!distanceStarted || verified || distanceGap>=kDistanceDeadTimeUs) {
      if(distanceStarted && distanceGap<kRearmGapUs) {
        const float step=kMetersPerPulse*calibration;
        meters+=step;
        const uint32_t lateUs=lastTickUs-nowUs;
        const float lateness=lateUs<=0x7fffffffUL?lateUs/1000000.0f:0;
        speed+=step/kSpeedTauSeconds*expf(-lateness/kSpeedTauSeconds);
      }
      distanceStarted=true;
      lastDistancePulseUs=nowUs;
    }
    if(strokePulse)onStrokePulse(nowUs);
  }

 private:
  bool distanceStarted=false;
  uint32_t lastDistancePulseUs=0;
};
} // namespace cydmotion
