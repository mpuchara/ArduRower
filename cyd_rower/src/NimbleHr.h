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
static String hrAttemptAddress;
static uint8_t hrCandidateType=BLE_ADDR_PUBLIC;
static int hrCandidateScore=-200;
static bool hrPreferredSaved=false;
class HrAdvertisements : public NimBLEAdvertisedDeviceCallbacks {
  void onResult(NimBLEAdvertisedDevice *d) override {
    ++hrAdvertisers;
    if(!d || !d->haveServiceUUID() ||
       !d->isAdvertisingService(NimBLEUUID((uint16_t)0x180D)))return;
    ++hrMatches;
    String addr(d->getAddress().toString().c_str());
    const bool preferred=hrPreferredAddress.length() &&
                          addr.equalsIgnoreCase(hrPreferredAddress);
    const int score=(preferred?1000:0)+d->getRSSI();
    if(score>hrCandidateScore) {
      hrCandidateScore=score;
      hrCandidateAddress=addr;
      hrCandidateType=d->getAddressType();
      Serial.printf("HR: 180D addr=%s type=%u RSSI=%d\n",
        addr.c_str(),(unsigned)hrCandidateType,d->getRSSI());
    }
  }
};
static HrAdvertisements hrAdvertisementCallbacks;
static void hrNotify(NimBLERemoteCharacteristic*,uint8_t *p,size_t size,bool) {
  if(size<2)return;
  const bool wide=(p[0]&1)!=0;
  if(wide && size<3)return;
  const uint16_t bpm=wide?(uint16_t)(p[1]|(uint16_t(p[2])<<8)):p[1];
  if(bpm>=35 && bpm<=240) {
    receivedHr=bpm;
    receivedHrAt=millis();
    ++hrNotifyPackets;
    hrState=HrState::Receiving;
    if(hrNotifyPackets<=8)Serial.printf("HR: packet #%lu bpm=%u FTMS=%d\n",
       (unsigned long)hrNotifyPackets,(unsigned)bpm,deviceConnected?1:0);
  }
}
class HrClientCallbacks : public NimBLEClientCallbacks {
  void onConnect(NimBLEClient*) override {
    setHrState(HrState::GattConnected,"NimBLE connected");
  }
  void onDisconnect(NimBLEClient*) override {
    hrConnected=false;
    setHrState(HrState::Disconnected,"NimBLE HR disconnected");
  }
};
static HrClientCallbacks hrClientCallbacks;
void hrWorker(void*) {
  NimBLEScan *scan=NimBLEDevice::getScan();
  NimBLEClient *client=NimBLEDevice::createClient();
  if(!scan || !client) {
    setHrState(HrState::InitFailed,"NimBLE scan/client unavailable");
    vTaskDelete(nullptr);return;
  }
  scan->setAdvertisedDeviceCallbacks(&hrAdvertisementCallbacks,false);
  scan->setActiveScan(true);
  scan->setInterval(180);
  scan->setWindow(30); // 17% radio scan duty
  client->setClientCallbacks(&hrClientCallbacks,false);
  client->setConnectTimeout(6);
  client->setConnectionParams(24,48,0,120);
  Serial.println("HR: NimBLE dual-role started; FTMS never paused");
  while(true) {
    if(ESP.getFreeHeap()<30000) {
      setHrState(HrState::LowMemory,"HR heap low");
      vTaskDelay(pdMS_TO_TICKS(10000));continue;
    }
    hrCandidateAddress="";hrCandidateScore=-200;
    hrAdvertisers=hrMatches=0; ++hrScanRounds;
    setHrState(HrState::Scan,"NimBLE scan 2 seconds");
    scan->start(2,false);
    scan->stop();
    Serial.printf("HR: scan #%lu devices=%u matches=%u FTMS=%d tx=%lu\n",
                  (unsigned long)hrScanRounds,(unsigned)hrAdvertisers,
                  (unsigned)hrMatches,deviceConnected?1:0,
                  (unsigned long)ftmsNotifications);
    if(!hrCandidateAddress.length()) {
      scan->clearResults();
      setHrState(HrState::NotFound,"NimBLE 180D not found");
      vTaskDelay(pdMS_TO_TICKS(10000));continue;
    }
