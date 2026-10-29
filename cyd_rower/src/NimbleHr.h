#pragma once
// NimBLE-based HR receiver for ESP32.

static String hrPreferredAddress;
static String hrCandidateAddress;
static volatile uint32_t hrScanRounds=0;
static constexpr uint32_t HR_BACKOFF_MS=20000;
static constexpr uint32_t HR_STALLED_AFTER_MS=12000;
static void setHrState(HrState state,const char *stage) {
  hrState=state; hrStageSinceMs=millis();
  hrLastReportedStall=HrState::Starting;
  Serial.printf("HR: %s FTMS=%d tx=%lu\n",stage,deviceConnected?1:0,
                (unsigned long)ftmsNotifications);
}
static HrState displayedHrState(uint32_t now) {
  if((hrState==HrState::Connecting || hrState==HrState::Service ||
      hrState==HrState::Subscribing) && hrStageSinceMs &&
      (uint32_t)(now-hrStageSinceMs)>HR_STALLED_AFTER_MS)
    return HrState::Stalled;
  return hrState;
}
static void checkHrStall(uint32_t now) {
  if(displayedHrState(now)==HrState::Stalled &&
     hrLastReportedStall!=hrState) {
    hrLastReportedStall=hrState;
    Serial.printf("HR: stage %s slow, FTMS tx=%lu\n",
                  hrStatus(hrState),(unsigned long)ftmsNotifications);
  }
}
