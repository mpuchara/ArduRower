#pragma once
// Application-level BLE maintenance scheduler for a dual-role ESP32.
// This does NOT manually time-slice BLE RF connection events; the ESP32
// controller does that. It gates expensive scans/discoveries to idle moments
// and limits retries to protect a working 1 Hz FTMS stream.
#include <stdint.h>

namespace cydble {
struct DutySchedule {
  static constexpr uint32_t kQuietAfterMotionMs=15000;
  static constexpr uint32_t kStableFtmsMs=8000;
  static constexpr uint32_t kRetryMs=90000;
  static constexpr uint32_t kGuardAfterDropMs=180000;
  volatile uint32_t lastMotionMs=0, ftmsSinceMs=0, lastAttemptMs=0, lastDropMs=0;
  volatile uint32_t attempts=0, ftmsDrops=0;
  volatile bool sawMotion=false, ftmsUp=false, oneAttemptWithFtms=false;
  volatile bool hrSetupRunning=false, collisionGuard=false;

  void onMotion(uint32_t ms) { lastMotionMs=ms; sawMotion=true; }
  void onFtms(bool connected,uint32_t now) {
    if(connected==ftmsUp)return;
    if(connected) {
      ftmsSinceMs=now;
      oneAttemptWithFtms=false;
    } else {
      ++ftmsDrops;
      lastDropMs=now;
      if(hrSetupRunning)collisionGuard=true;
    }
    ftmsUp=connected;
  }
  bool motionQuiet(uint32_t now) const {
    return !sawMotion || (uint32_t)(now-lastMotionMs)>=kQuietAfterMotionMs;
  }
  bool canStart(uint32_t now) const {
    if(!motionQuiet(now) || hrSetupRunning)return false;
    if(attempts && (uint32_t)(now-lastAttemptMs)<kRetryMs)return false;
    if(collisionGuard &&
       (uint32_t)(now-lastDropMs)<kGuardAfterDropMs)return false;
    // Once rowing has begun, never initiate another GATT discovery against
    // an already-connected Garmin. This protects FTMS even after a pause.
    // A previously established HR link can still receive notifications.
    if(ftmsUp && sawMotion)return false;
    if(ftmsUp && ((uint32_t)(now-ftmsSinceMs)<kStableFtmsMs ||
                  oneAttemptWithFtms))return false;
    return true;
  }
  void begin(uint32_t now) {
    ++attempts;lastAttemptMs=now;
    hrSetupRunning=true;
    if(ftmsUp)oneAttemptWithFtms=true; // no scan/connect storm during FTMS
  }
  void end() {hrSetupRunning=false;}
  bool shouldYield(uint32_t now) const {
    return !motionQuiet(now) ||
           (collisionGuard && (uint32_t)(now-lastDropMs)<kGuardAfterDropMs);
  }
};
} // namespace cydble
