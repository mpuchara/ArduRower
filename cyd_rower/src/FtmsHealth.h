#pragma once
#include <stdint.h>
// FTMS link health is *not* the same as an established BLE ACL.
// Garmin Connect IQ receives Rower Data only after it enables 0x2AD1 CCCD.
namespace cydble {
struct FtmsHealth {
  static constexpr uint32_t kStableBeforeHrMs=18000UL;
  static constexpr uint32_t kMinimumNotifyRequests=10;
  static bool streamReady(bool acl,bool subscribed,uint32_t notifyRequests) {
    return acl && subscribed && notifyRequests>0;
  }
  static bool canTryHr(uint32_t now,bool acl,bool subscribed,
                       uint32_t subscribedAt,uint32_t notifyRequests,
                       bool motionQuiet) {
    return streamReady(acl,subscribed,notifyRequests) &&
           subscribedAt!=0 && motionQuiet &&
           (uint32_t)(now-subscribedAt)>=kStableBeforeHrMs &&
           notifyRequests>=kMinimumNotifyRequests;
  }
  static bool mustYieldHr(bool acl,bool subscribed) {
    return !acl || !subscribed;
  }
};
}
