#pragma once
// Pure host-testable policy; never disconnect FTMS from the HR worker.
#include <stdint.h>
namespace cydble {
struct CoexistPolicy {
  static constexpr uint32_t kFtmsJoinGraceMs=30000UL;
  static constexpr uint32_t kRetryCooldownMs=120000UL;
  static constexpr uint32_t kFtmsCollisionWindowMs=25000UL;
  static bool shouldYieldHrForFtms(uint32_t now,uint32_t connectedAt,
                                   bool hrConnected,bool ftmsConnected) {
    return hrConnected && !ftmsConnected && connectedAt &&
           (uint32_t)(now-connectedAt)>=kFtmsJoinGraceMs;
  }
  static bool inRetryCooldown(uint32_t now,uint32_t deadline) {
    return deadline && (int32_t)(now-deadline)<0;
  }
  static bool noCurrentSessionBpm(uint32_t now,uint32_t connectedAt,
                                  uint32_t received,uint32_t baseline) {
    return connectedAt && received==baseline &&
           (uint32_t)(now-connectedAt)>45000UL;
  }
  static bool ftmsCollisionPossible(uint32_t now,uint32_t attemptedAt,
                                    bool ftmsWasConnected) {
    return ftmsWasConnected && attemptedAt &&
           (uint32_t)(now-attemptedAt)<kFtmsCollisionWindowMs;
  }
};
} // namespace cydble
