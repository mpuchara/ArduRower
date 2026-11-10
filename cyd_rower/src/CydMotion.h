#pragma once
// Experimental movement model for a four-magnet strap reel + one-way clutch.
// A reed switch cannot report direction: stroke phases are inferred from two
// repeatable slowdowns, NEVER assumed to be mechanically measured.
// Independent of Arduino: host-compiled regression tests exercise timestamps.
#include <stdint.h>
#include <math.h>

namespace cydmotion {
enum class Phase : uint8_t { Waiting, Drive, Return };
// A lone reed edge while paused is not enough to restart the clock.
class PauseResumeGate {
 public:
  void reset() { pending=false; }

  bool confirm(uint32_t nowUs, uint32_t &firstUs) {
    if(pending && nowUs-candidateUs<=350000UL) {
      firstUs=candidateUs;
      reset();
      return true;
    }
    candidateUs=nowUs;
    pending=true;
    return false;
  }

 private:
  uint32_t candidateUs=0;
  bool pending=false;
};

class Engine {
 public:
  // Both directions contribute to the total pulse rate. The virtual
  // distance per accepted interval is provisional, not S4-calibrated.
  static constexpr float kMetersPerPulse = 0.16f;
  static constexpr float kDefaultCalibration = 0.70f;
  static constexpr float kSpeedTauSeconds = 3.5f;
  static constexpr uint32_t kCoastUs = 3000000;
  static constexpr float kStopTauSeconds = 1.5f;
  static constexpr uint32_t kPauseAfterUs = 8000000;
  static constexpr uint32_t kDeadTimeUs = 15000;
  static constexpr uint32_t kDistanceDeadTimeUs = 10000;
  static constexpr uint32_t kRearmGapUs = 1600000;
  static constexpr uint32_t kFastTurnMinGapUs = 36000;
  static constexpr uint32_t kFastPhaseMinUs = 160000;
  Phase phase=Phase::Waiting;
  uint32_t pulses=0, strokes=0, transitions=0;
  float speed=0, meters=0, spm=0, calibration=kDefaultCalibration;
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
    speed=meters=spm=0;
    phasePulses=0;
    phaseStartUs=lastPulseUs=lastTickUs=nowUs;
    lastStrokeUs=0;
    averageGapUs=0;
    pendingFastTurnGapUs=0;
    gapIndex=gapCount=0;
    for(uint8_t i=0;i<4;++i) previousGapsUs[i]=0;
    started=haveStroke=distanceStarted=false;
    lastDistancePulseUs=nowUs;
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
  void onPulse(uint32_t nowUs, bool strokePulse=true) {
    tick(nowUs);
    const uint32_t distanceGap=nowUs-lastDistancePulseUs;
    if(!distanceStarted || distanceGap>=kDistanceDeadTimeUs) {
      if(distanceStarted && distanceGap<kRearmGapUs) {
        const float step=kMetersPerPulse*calibration;
        meters+=step;
        speed+=step/kSpeedTauSeconds;
      }
      distanceStarted=true;
      lastDistancePulseUs=nowUs;
    }
    // The stroke detector keeps its original input and debounce.
    if(!strokePulse)return;
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
      // A short gap followed by a normal longer gap can look like a turn
      // on an uneven reel. Require a slowdown versus both recent intervals.
      const float recentGap=fmaxf(previousGapsUs[(gapIndex+2)&3],
                                  previousGapsUs[(gapIndex+3)&3]);
      const bool fastTurnCandidate=hasBaseline && fastPhaseReady &&
          sameMagnetGap>0 &&
          gap>=kFastTurnMinGapUs && gap<250000 &&
          gap>averageGapUs*1.55f && gap>recentGap*1.10f && newSlowdown;
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
  }
 private:
  bool distanceStarted=false;
  uint32_t lastDistancePulseUs=0;
};
} // namespace cydmotion
