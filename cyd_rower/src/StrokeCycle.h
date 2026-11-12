#pragma once
#include <stdint.h>
#include <math.h>

namespace cydmotion {
enum class Phase : uint8_t { Waiting, Drive, Return };

// Direction is inferred from a slowdown/recovery envelope. Four intervals
// provide a local travel-time estimate; no interval is assigned to a magnet.
class StrokeCycle {
 public:
  Phase phase=Phase::Waiting;
  uint32_t pulses=0,strokes=0,transitions=0,rejectedTurns=0;
  uint32_t lastPulseUs=0,lastStrokeUs=0;
  float spm=0,reelSpeed=0,filteredReelSpeed=0,acceleration=0;
  bool started=false,haveStroke=false,turnCandidate=false;
  float confidence=0;

  void resetCycle(uint32_t nowUs) {
    phase=Phase::Waiting;
    pulses=strokes=transitions=rejectedTurns=0;
    lastStrokeUs=0;
    resetSynchronization(nowUs);
  }

  void resetSynchronization(uint32_t nowUs) {
    started=haveStroke=false;
    spm=0;
    periodCount=periodIndex=0;
    phase=Phase::Waiting;
    lastPulseUs=phaseStartUs=nowUs;
    phasePulses=gapCount=gapIndex=sizeCount=sizeIndex=0;
    minPeriodUs=peakPeriodUs=0;
    turnCandidate=false;
    confidence=0;
    reelSpeed=filteredReelSpeed=acceleration=0;
  }

  void onStrokePulse(uint32_t nowUs) {
    const uint32_t gap=nowUs-lastPulseUs;
    if(started && (!gap || gap>0x7fffffffUL))return;
    ++pulses;
    if(!started || gap>=1600000UL) {
      // A stop starts a new drive. Previous cadence is not a new stroke.
      resetSynchronization(nowUs);
      started=true;
      phase=Phase::Drive;
      phasePulses=1;
      return;
    }
    lastPulseUs=nowUs;
    ++phasePulses;
    gaps[gapIndex]=gap;
    gapIndex=(gapIndex+1)&3;
    if(gapCount<4)++gapCount;
    if(gapCount<4)return;
    uint32_t periodUs=0;
    for(uint8_t i=0;i<4;++i)periodUs+=gaps[i];
    const float previousSpeed=filteredReelSpeed;
    reelSpeed=4000000.0f/periodUs; // crossings/s, separate from boat speed
    const float alpha=1.0f-expf(-static_cast<float>(gap)/80000.0f);
    filteredReelSpeed+=alpha*(reelSpeed-filteredReelSpeed);
    acceleration=(filteredReelSpeed-previousSpeed)*1000000.0f/gap;

    if(!turnCandidate) {
      if(!minPeriodUs || periodUs<minPeriodUs)minPeriodUs=periodUs;
      if(periodUs>=minPeriodUs*1.15f) {
        turnCandidate=true;
        peakPeriodUs=periodUs;
        peakUs=nowUs;
      }
    } else if(periodUs>=peakPeriodUs) {
      peakPeriodUs=periodUs;
      peakUs=nowUs;
    }
    uint16_t required=8;
    if(sizeCount) {
      const uint16_t adaptive=static_cast<uint16_t>(median(sizes,sizeCount)*0.65f);
      if(adaptive>required)required=adaptive;
    }
    if(phasePulses<required) {
      // Track the fast part of the phase even while rearming. A peak left
      // over from the previous turn is not a new turnaround candidate.
      turnCandidate=false;
      return;
    }
    confidence=turnCandidate?fminf(1.0f,(peakPeriodUs-minPeriodUs)/
                                            (minPeriodUs*0.5f)):0;
    if(!turnCandidate || nowUs-phaseStartUs<160000UL ||
       periodUs>=peakPeriodUs*0.98f)return;

    // A rejected drive completion cannot switch DRIVE to RETURN.
    const uint32_t cycleUs=peakUs-lastStrokeUs;
    uint32_t minimumCycleUs=650000UL;
    if(periodCount) {
      const uint32_t adaptive=static_cast<uint32_t>(median(periods,periodCount)*0.48f);
      if(adaptive>minimumCycleUs)minimumCycleUs=adaptive;
    }
    if(phase==Phase::Drive && haveStroke && cycleUs<minimumCycleUs) {
      ++rejectedTurns;
      turnCandidate=false;
      minPeriodUs=periodUs;
      return;
    }
    if(phase==Phase::Drive) {
      if(haveStroke && cycleUs<=7500000UL) {
        periods[periodIndex]=cycleUs;
        periodIndex=(periodIndex+1)%5;
        if(periodCount<5)++periodCount;
        spm=60000000.0f/median(periods,periodCount);
      }
      ++strokes;
      lastStrokeUs=peakUs;
      haveStroke=true;
      phase=Phase::Return;
    } else {
      phase=Phase::Drive;
    }
    ++transitions;
    sizes[sizeIndex]=phasePulses;
    sizeIndex=(sizeIndex+1)&3;
    if(sizeCount<4)++sizeCount;
    phasePulses=0;
    phaseStartUs=nowUs;
    minPeriodUs=peakPeriodUs=periodUs;
    turnCandidate=false;
  }

 private:
  uint32_t gaps[4]={},sizes[4]={},periods[5]={};
  uint32_t phaseStartUs=0,peakUs=0,minPeriodUs=0,peakPeriodUs=0;
  uint16_t phasePulses=0;
  uint8_t gapCount=0,gapIndex=0,sizeCount=0,sizeIndex=0;
  uint8_t periodCount=0,periodIndex=0;

  static uint32_t median(const uint32_t *values,uint8_t count) {
    uint32_t sorted[5]={};
    for(uint8_t i=0;i<count;++i) {
      uint8_t j=i;
      while(j && sorted[j-1]>values[i]) {
        sorted[j]=sorted[j-1];
        --j;
      }
      sorted[j]=values[i];
    }
    return sorted[count/2];
  }
};
} // namespace cydmotion
