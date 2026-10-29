#pragma once
// Experimental movement model for a four-magnet strap reel + one-way clutch.
// A reed switch cannot report direction: stroke phases are inferred from two
// repeatable slowdowns, NEVER assumed to be mechanically measured.
// Independent of Arduino: host-compiled regression tests exercise timestamps.
#include <stdint.h>
#include <math.h>

namespace cydmotion {
enum class Phase : uint8_t { Waiting, Drive, Return };
class Engine {
 public:
  static constexpr float kMetersPerDrivePulse = 0.14f; // provisional; NOT S4-calibrated
  static constexpr float kDefaultCalibration = 0.70f;
  static constexpr uint32_t kDeadTimeUs = 15000;
  static constexpr uint32_t kRearmGapUs = 1600000;
  Phase phase=Phase::Waiting;
  uint32_t pulses=0, strokes=0, transitions=0;
  float speed=0, meters=0, spm=0, calibration=kDefaultCalibration;
  uint16_t phasePulses=0;
  uint32_t phaseStartUs=0, lastPulseUs=0, lastTickUs=0, lastStrokeUs=0;
  float averageGapUs=0;
  bool started=false, haveStroke=false;

  void reset(uint32_t nowUs) {
    phase=Phase::Waiting;
    pulses=strokes=transitions=0;
    speed=meters=spm=0;
    phasePulses=0;
    phaseStartUs=lastPulseUs=lastTickUs=nowUs;
    lastStrokeUs=0;
    averageGapUs=0;
    started=haveStroke=false;
  }
  void setCalibration(float value) {
    if(isfinite(value) && value>=0.3f && value<=1.5f) calibration=value;
  }
  uint32_t calibrationMilli() const {
    return static_cast<uint32_t>(calibration*1000.0f+0.5f);
  }
  void tick(uint32_t nowUs) {
    uint32_t duration=nowUs-lastTickUs;
    if(!duration)return;
    // Cap a single integration window; after long sleep the speed should
    // decay rather than produce an enormous phantom distance.
    float dt=(duration>250000 ? 250000 : duration)/1000000.0f;
    if(started) {
      const uint32_t idle=nowUs-lastPulseUs;
      const float tau=(phase==Phase::Drive && idle<160000) ? 8.0f : 2.8f;
      const float decay=expf(-dt/tau);
      const float newSpeed=speed*decay;
      meters += (speed+newSpeed)*0.5f*dt;
      speed=newSpeed;
      if(idle>=5000000) {
        speed=0;
        spm=0;
      }
    }
    lastTickUs=nowUs;
  }
  void onPulse(uint32_t nowUs) {
    tick(nowUs);
    const uint32_t gap=nowUs-lastPulseUs;
    if(started && gap<kDeadTimeUs)return;
    ++pulses;
    if(!started || gap>=kRearmGapUs) {
      started=true;
      phase=Phase::Drive;
      phasePulses=1;
      phaseStartUs=lastPulseUs=nowUs;
      averageGapUs=0;
      if(gap>=kRearmGapUs)spm=0;
      return;
    }
    const bool hasBaseline=averageGapUs>0;
    // A single slow section is not a stroke boundary: require sufficient
    // elapsed time, pulse count, and a slowdown relative to local cadence.
    const float threshold=fmaxf(105000.0f, fminf(360000.0f,
                               averageGapUs*2.65f));
    const bool boundary=hasBaseline && gap>threshold &&
                        phasePulses>=6 &&
                        static_cast<uint32_t>(nowUs-phaseStartUs)>320000;
    if(boundary) {
      if(phase==Phase::Drive) {
        // Count once when the driven pull finishes; the return is NOT a stroke.
        if(!haveStroke || static_cast<uint32_t>(nowUs-lastStrokeUs)>650000) {
          if(haveStroke) {
            const float interval=(nowUs-lastStrokeUs)/1000000.0f;
            const float instantaneous=60.0f/interval;
            if(instantaneous>=8 && instantaneous<=65)
              spm=(spm>0) ? spm*0.55f+instantaneous*0.45f : instantaneous;
          }
          ++strokes;lastStrokeUs=nowUs;haveStroke=true;
        }
        phase=Phase::Return;
      } else {
        phase=Phase::Drive;
      }
      ++transitions;
      phasePulses=0;
      phaseStartUs=nowUs;
      // Reset baseline at direction changes; this gap is deliberately excluded.
      averageGapUs=0;
    } else {
      // Never train baseline on a turnaround candidate: otherwise the
      // slowdown itself rapidly raises the threshold.
      if(gap<400000 && (!hasBaseline || gap<averageGapUs*1.8f))
        averageGapUs=hasBaseline ? averageGapUs*0.82f+gap*0.18f : gap;
    }
    ++phasePulses;
    lastPulseUs=nowUs;
    if(phase==Phase::Drive && gap<240000) {
      // Only driving pulses add energy. Reverse/return pulses never
      // accelerate the virtual flywheel. Virtual speed continues through
      // return with exponential water-drag decay.
      const float target=fminf(7.5f,(kMetersPerDrivePulse*calibration)/
                                      (gap/1000000.0f));
      speed=fminf(7.5f,speed*0.83f+target*0.17f);
    }
  }
};
} // namespace cydmotion
