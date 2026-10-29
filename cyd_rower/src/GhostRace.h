#pragma once
// Deterministic race/ghost logic: no Arduino headers, BLE or display dependencies.
// Replay is time-based, including all pauses; rowing distance remains an estimate.
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <math.h>
#include "../../ArduRower/RowerMetrics.h" // one source of truth for distance calibration

namespace ghostrace {
static constexpr uint16_t kIntervalMs = 2000;
static constexpr uint16_t kSamples = 1024; // 34m 8s; 0.1m encoded as uint16
static constexpr uint32_t kMagic = 0x47525731UL; // GRW1
static constexpr uint16_t kVersion = 2; // new movement model: incompatible with previous pulse distances
static const uint32_t kScaleMilli = 700; // default CYD motion calibration

struct Record {
  uint32_t magic;
  uint16_t version;
  uint16_t targetMeters;
  uint32_t scaleMilli;
  uint32_t finishMs;
  uint16_t count;
  uint16_t decimeters[kSamples];
  uint32_t checksum;
};
static_assert(sizeof(Record) < 2200, "Ghost record must fit NVS blob");

inline uint32_t hashRecord(const Record &r) {
  const uint8_t *bytes = reinterpret_cast<const uint8_t *>(&r);
  uint32_t h = 2166136261UL; // FNV-1a, protects against torn/corrupted records
  for (size_t i = 0; i < offsetof(Record, checksum); ++i) {
    h ^= bytes[i];
    h *= 16777619UL;
  }
  return h;
}
inline bool allowedDistance(uint16_t d) { return d == 500 || d == 1000 || d == 2000; }
inline bool valid(const Record &r, uint16_t expectedTarget,
                  uint32_t expectedCalibration = kScaleMilli) {
  if (r.magic != kMagic || r.version != kVersion ||
      r.targetMeters != expectedTarget || !allowedDistance(r.targetMeters) ||
      r.scaleMilli != expectedCalibration || r.finishMs == 0 ||
      r.count == 0 || r.count > kSamples ||
      r.finishMs > static_cast<uint32_t>(kSamples) * kIntervalMs ||
      r.checksum != hashRecord(r)) return false;
  if (r.decimeters[0] != 0) return false;
  const uint16_t limit = static_cast<uint16_t>(expectedTarget * 10U);
  for (uint16_t i = 0; i < r.count; ++i) {
    if (r.decimeters[i] > limit ||
        (i && r.decimeters[i] < r.decimeters[i - 1])) return false;
  }
  return true;
}
inline float interpolate(const Record &r, uint32_t ms) {
  if (ms >= r.finishMs) return static_cast<float>(r.targetMeters);
  if (r.count == 0) return 0;
  const uint32_t ix = ms / kIntervalMs;
  if (ix + 1 < r.count) {
    const float d0 = r.decimeters[ix] * 0.1f;
    const float d1 = r.decimeters[ix + 1] * 0.1f;
    return d0 + (d1 - d0) * ((ms % kIntervalMs) / static_cast<float>(kIntervalMs));
  }
  const uint32_t lastMs = (r.count - 1U) * kIntervalMs;
  const float last = r.decimeters[r.count - 1] * 0.1f;
  if (r.finishMs <= lastMs) return last;
  const float f = (ms - lastMs) / static_cast<float>(r.finishMs - lastMs);
  return last + (r.targetMeters - last) * f;
}
inline float elapsedAtDistance(const Record &r, float distanceMeters) {
  if (distanceMeters <= 0) return 0;
  if (distanceMeters >= r.targetMeters) return r.finishMs;
  float previous = 0;
  for (uint16_t i = 1; i < r.count; ++i) {
    const float d = r.decimeters[i] * 0.1f;
    if (d >= distanceMeters && d > previous) {
      return (i - 1U) * kIntervalMs +
             (distanceMeters - previous) / (d - previous) * kIntervalMs;
    }
    previous = d;
  }
  const float last = r.decimeters[r.count - 1] * 0.1f;
  const uint32_t lastMs = (r.count - 1U) * kIntervalMs;
  if (r.targetMeters <= last || r.finishMs <= lastMs) return r.finishMs;
  return lastMs + (distanceMeters - last) / (r.targetMeters - last) *
         (r.finishMs - lastMs);
}

enum class Phase : uint8_t { Idle, Armed, Running, Finished, Cancelled };

class Race {
 public:
  Record best = {};
  Record attempt = {};
  Phase phase = Phase::Idle;
  bool hasBest = false, personalBest = false, overflow = false;
  uint16_t targetMeters = 0;
  uint32_t calibrationMilli = kScaleMilli;
  uint32_t startMs = 0;
  uint32_t endMs = 0;
  float meters = 0;

  void arm(uint16_t target, const Record *loadedBest,
           uint32_t activeCalibration = kScaleMilli) {
    calibrationMilli = activeCalibration;
    targetMeters = target;
    hasBest = loadedBest && valid(*loadedBest, target, calibrationMilli);
    if (hasBest) memcpy(&best, loadedBest, sizeof(best));
    else memset(&best, 0, sizeof(best));
    memset(&attempt, 0, sizeof(attempt));
    attempt.magic = kMagic;
    attempt.version = kVersion;
    attempt.targetMeters = target;
    attempt.scaleMilli = calibrationMilli;
    attempt.count = 1;
    attempt.decimeters[0] = 0;
    phase = Phase::Armed;
    meters = 0;
    startMs = endMs = 0;
    personalBest = overflow = false;
  }

  void start(uint32_t nowMs) {
    if (phase != Phase::Armed) return;
    startMs = nowMs;
    phase = Phase::Running;
  }
  void cancel() {
    if (phase == Phase::Running || phase == Phase::Armed)
      phase = Phase::Cancelled;
  }
  uint32_t elapsed(uint32_t nowMs) const {
    if (phase == Phase::Running) return nowMs - startMs; // millis wrap safe
    return phase == Phase::Finished ? endMs : 0;
  }
  float ghostMeters(uint32_t nowMs) const {
    return hasBest ? interpolate(best, elapsed(nowMs)) : 0.0f;
  }
  float meterLead(uint32_t nowMs) const {
    return meters - ghostMeters(nowMs);
  }
  float timeLeadMs(uint32_t nowMs) const {
    if (!hasBest) return 0;
    // Positive = athlete reached the same distance faster than the ghost.
    return elapsedAtDistance(best, meters) - elapsed(nowMs);
  }

  // Call on every loop tick, *after* sensor pulses have been processed.
  // The caller supplies a nonnegative distance from the race start.
  // Returns true exactly when a new personal best is ready for persistence.
  bool advance(uint32_t nowMs, float distance) {
    if (phase != Phase::Running) return false;
    const uint32_t ms = nowMs - startMs;
    if (isfinite(distance) && distance > meters)
      meters = distance < targetMeters ? distance : (float)targetMeters;
    // Keep ghost track sampled even while the user is stopped, without
    // using metrics.seconds() (which pauses after five seconds).
    while (attempt.count < kSamples &&
           static_cast<uint32_t>(attempt.count) * kIntervalMs <= ms) {
      const float dm = meters * 10;
      attempt.decimeters[attempt.count++] =
          static_cast<uint16_t>(dm > targetMeters * 10U ?
                                targetMeters * 10U : dm + 0.5f);
    }
    if (meters < targetMeters && ms <= kSamples * kIntervalMs)
      return false;
    if (meters < targetMeters) {
      overflow = true;
      phase = Phase::Cancelled; // show cannot-save message, no corrupt record
      return false;
    }
    phase = Phase::Finished;
    endMs = ms ? ms : 1;
    if (endMs > kSamples * kIntervalMs) {
      overflow = true;
      return false;
    }
    attempt.finishMs = endMs;
    // A finish at the exact sampling boundary may have inserted a sample;
    // clamp it to the target. The final waypoint is given by finishMs.
    if (attempt.count > 1 && attempt.decimeters[attempt.count - 1] > targetMeters * 10U)
      attempt.decimeters[attempt.count - 1] = targetMeters * 10U;
    attempt.checksum = hashRecord(attempt);
    personalBest = (!hasBest || endMs < best.finishMs);
    return personalBest;
  }
};
} // namespace ghostrace
