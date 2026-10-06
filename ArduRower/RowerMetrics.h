#pragma once
#include <stdint.h>
#include <math.h>

// One FALLING edge per magnet crossing, in either direction on the strap reel.
// Historical baseline: 3.156 pulses/m, including handle recovery.
// This is a virtual distance, not the circumference or measured rotor travel.
static const uint8_t MAGNETS_PER_REVOLUTION = 4;
static const float METERS_PER_REVOLUTION = 4.0f / 3.156f;
// Provisional scale chosen after implausibly high readings during easy rowing.
// Not a measured calibration: reed pulses contain no force/load information.
// Distance and speed scale linearly; Concept2-equivalent watts scale cubically.
static const float DISTANCE_SCALE = 0.65f;
static const uint32_t SENSOR_DEBOUNCE_US = 15000;
static const uint32_t PAUSE_AFTER_US = 5000000;

class RowerMetrics {
public:
  uint32_t pulses = 0, strokes = 0;
  uint64_t activeUs = 0;
  float speed = 0, strokeRate = 0;
  bool started = false, paused = false;

  void reset(uint32_t now) {
    pulses = strokes = 0; activeUs = 0;
    speed = strokeRate = 0; started = paused = false;
    lastTick = lastPulse = sampleTime = now;
    samplePulses = 0; oldPulseRate = 0; holdoffPulses = 0;
    haveStroke = false; lastStroke = 0;
    strokeIntervalCount = strokeIntervalIndex = 0;
    equivalentEnergy = 0;
  }

  void tick(uint32_t now) {
    const uint32_t dt = now - lastTick;
    if (started && !paused) {
      const uint32_t age = lastTick - lastPulse;
      uint32_t moving = age < PAUSE_AFTER_US ? PAUSE_AFTER_US - age : 0;
      if (moving > dt) moving = dt;
      activeUs += moving;
      equivalentEnergy += equivalentPower() * (moving / 1000000.0);
      paused = (uint32_t)(now - lastPulse) >= PAUSE_AFTER_US;
      if (paused) { speed = strokeRate = 0; }
    }
    lastTick = now;
    const uint32_t sampleDt = now - sampleTime;
    if (sampleDt >= 1000000) {
      const float raw = (pulses - samplePulses) * metersPerPulse() * 1000000.0f / sampleDt;
      // Time-aware low-pass filter: equal behaviour when OLED/BLE delays the loop.
      const float alpha = 1.0f - expf(-static_cast<float>(sampleDt) / 2000000.0f);
      speed += alpha * (raw - speed);
      if (paused) speed = 0;
      samplePulses = pulses; sampleTime = now;
    }
    if (haveStroke && (uint32_t)(now - lastStroke) >= PAUSE_AFTER_US) strokeRate = 0;
  }

  void pulse(uint32_t now) {
    tick(now);
    const uint32_t gap = now - lastPulse;
    if (!started || gap >= PAUSE_AFTER_US) {
      // Do not include a rest in the next speed/cadence sample.
      sampleTime = now; samplePulses = pulses;
      oldPulseRate = 0; holdoffPulses = 0;
      haveStroke = false; strokeIntervalCount = strokeIntervalIndex = 0;
    } else {
      if (gap > 0) {
        // Preserve the user's established heuristic (500 pulses/min increase,
        // 29-pulse holdoff at four magnets). A single reed cannot identify direction,
        // so this is an estimate, and may count acceleration during recovery too.
        const float pulseRate = 60000000.0f / gap * 4 / MAGNETS_PER_REVOLUTION;
        if (oldPulseRate > 0 && pulseRate - oldPulseRate > 500 && holdoffPulses == 0) {
          if (haveStroke) {
            strokeIntervals[strokeIntervalIndex] = now - lastStroke;
            strokeIntervalIndex = (strokeIntervalIndex + 1) % 4;
            if (strokeIntervalCount < 4) ++strokeIntervalCount;
            uint64_t sum = 0;
            for (uint8_t i = 0; i < strokeIntervalCount; ++i) sum += strokeIntervals[i];
            strokeRate = 60000000.0f * strokeIntervalCount / sum;
          }
          ++strokes; lastStroke = now; haveStroke = true;
          holdoffPulses = (29 * MAGNETS_PER_REVOLUTION + 3) / 4;
        }
        if (holdoffPulses > 0) --holdoffPulses;
        oldPulseRate = pulseRate;
      }
    }
    ++pulses; started = true; paused = false; lastPulse = now;
  }

  static float metersPerPulse() { return METERS_PER_REVOLUTION / MAGNETS_PER_REVOLUTION * DISTANCE_SCALE; }
  float distance() const { return pulses * metersPerPulse(); }
  float seconds() const { return activeUs / 1000000.0; }
  float averageStrokeRate() const { return activeUs ? strokes * 60000000.0 / activeUs : 0; }
  float pace() const { return speed > 0.05f && !paused ? 500.0f / speed : 0; }
  float averagePace() const { return pulses ? 500.0f * seconds() / distance() : 0; }
  // Concept2 pace-equivalent watts only; NOT measured water-rotor power.
  float equivalentPower() const { return 2.8f * speed * speed * speed; }
  float averageEquivalentPower() const { return activeUs ? equivalentEnergy / seconds() : 0; }

private:
  uint32_t lastTick = 0, lastPulse = 0, sampleTime = 0, samplePulses = 0;
  float oldPulseRate = 0;
  uint16_t holdoffPulses = 0;
  bool haveStroke = false;
  uint32_t lastStroke = 0, strokeIntervals[4] = {};
  uint8_t strokeIntervalCount = 0, strokeIntervalIndex = 0;
  double equivalentEnergy = 0;
};
