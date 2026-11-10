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
  // Low-pass the *reported* speed over approximately one rowing stroke.
  // The flywheel model and integrated distance remain untouched.
  static constexpr float kReportedSpeedTauSeconds = 2.5f;
  static constexpr uint32_t kDeadTimeUs = 15000;
  static constexpr uint32_t kRearmGapUs = 1600000;
  static constexpr uint32_t kFastTurnMinGapUs = 36000;
  static constexpr uint32_t kFastPhaseMinUs = 160000;
  Phase phase=Phase::Waiting;
  uint32_t pulses=0, strokes=0, transitions=0;
  float speed=0, reportedSpeed=0, meters=0, spm=0, calibration=kDefaultCalibration;
  uint16_t phasePulses=0;
  uint32_t phaseStartUs=0, lastPulseUs=0, lastTickUs=0, lastStrokeUs=0;
  float averageGapUs=0;
  // Compare each period with the period one full four-magnet revolution ago:
  // a consistently uneven magnet spacing must not look like a turnaround.
  uint32_t previousGapsUs[4] = {};
  uint8_t gapIndex=0, gapCount=0;
  uint32_t pendingFastTurnGapUs=0;
  bool started=false, haveStroke=false;

  void reset(uint32_t nowUs) {
    phase=Phase::Waiting;
    pulses=strokes=transitions=0;
    speed=reportedSpeed=meters=spm=0;
    phasePulses=0;
    phaseStartUs=lastPulseUs=lastTickUs=nowUs;
    lastStrokeUs=0;
    averageGapUs=0;
    pendingFastTurnGapUs=0;
    gapIndex=gapCount=0;
    for(uint8_t i=0;i<4;++i) previousGapsUs[i]=0;
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
    // Queued ISR timestamps can predate a recent loop tick if BLE/TFT work
    // delayed processing. Never mistake a stale sample for a 71-minute gap.
    if(!duration || duration>0x7fffffffUL)return;
    // Cap a single integration window; after long sleep the speed should
    // decay rather than produce an enormous phantom distance.
    uint32_t integrate=duration>250000?250000:duration;
    if(started) {
      const uint32_t age=lastTickUs-lastPulseUs;
      const uint32_t remaining=age<5000000UL?5000000UL-age:0;
      if(integrate>remaining)integrate=remaining;
    }
    float dt=integrate/1000000.0f;
    if(started) {
      const uint32_t idle=nowUs-lastPulseUs;
      const float tau=(phase==Phase::Drive && idle<160000) ? 8.0f : 2.8f;
      const float decay=expf(-dt/tau);
      const float newSpeed=speed*decay;
      const float meanSpeed=(speed+newSpeed)*0.5f;
      meters += meanSpeed*dt;
      // Smooth the physical-model estimate for UI/FTMS without feeding the
      // filtered value back into the flywheel or distance integrator.
      // Time-aware alpha keeps output stable across TFT/BLE loop jitter.
      const float alpha=1.0f-expf(-dt/kReportedSpeedTauSeconds);
      reportedSpeed += alpha*(meanSpeed-reportedSpeed);
      speed=newSpeed;
      if(idle>=5000000) {
        speed=0;
        reportedSpeed=0;
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
      pendingFastTurnGapUs=0;
      gapIndex=gapCount=0;
      for(uint8_t i=0;i<4;++i)previousGapsUs[i]=0;
      if(gap>=kRearmGapUs)spm=0;
      return;
    }
    const bool hasBaseline=averageGapUs>0;
    const uint32_t elapsed=static_cast<uint32_t>(nowUs-phaseStartUs);
    // Keep the proven conservative detection for clearly slow turnarounds.
    const float threshold=fmaxf(105000.0f, fminf(360000.0f,
                               averageGapUs*2.65f));
    // At higher cadence both reversals can be shorter than 105 ms. Detect
    // a local slowdown, then confirm it only if the next magnet arrives
    // sooner. Comparing with the same gap one 4-magnet revolution earlier
    // rejects a fixed large spacing between two unevenly placed magnets.
    const uint32_t sameMagnetGap=(gapCount==4)?previousGapsUs[gapIndex]:0;
    const bool newSlowdown=sameMagnetGap==0 || gap>sameMagnetGap*1.35f;
    const bool longTurn=hasBaseline && gap>threshold && newSlowdown &&
                        phasePulses>=6 && elapsed>320000;
    const bool fastPhaseReady=phasePulses>=6 && elapsed>=kFastPhaseMinUs;
    const bool fastTurnConfirmed=fastPhaseReady && pendingFastTurnGapUs>0 &&
                                gap<pendingFastTurnGapUs*0.82f;
    const bool boundary=longTurn || fastTurnConfirmed;
    if(boundary) {
      if(phase==Phase::Drive) {
        // Count once when the driven pull finishes; the return is NOT a stroke.
        if(!haveStroke || static_cast<uint32_t>(nowUs-lastStrokeUs)>650000) {
          if(haveStroke) {
            const float interval=(nowUs-lastStrokeUs)/1000000.0f;
            const float instantaneous=60.0f/interval;
            if(instantaneous>=8 && instantaneous<=90)
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
      pendingFastTurnGapUs=0;
    } else {
      const bool fastTurnCandidate=hasBaseline && fastPhaseReady &&
          sameMagnetGap>0 &&
          gap>=kFastTurnMinGapUs && gap<250000 &&
          gap>averageGapUs*1.55f && newSlowdown;
      pendingFastTurnGapUs=fastTurnCandidate?gap:0;
      // Do not include the suspected reversal in the local running cadence.
      if(!fastTurnCandidate && gap<400000 &&
         (!hasBaseline || gap<averageGapUs*1.8f))
        averageGapUs=hasBaseline ? averageGapUs*0.82f+gap*0.18f : gap;
    }
    ++phasePulses;
    lastPulseUs=nowUs;
    previousGapsUs[gapIndex]=gap;
    gapIndex=(gapIndex+1)&3;
    if(gapCount<4)++gapCount;
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
