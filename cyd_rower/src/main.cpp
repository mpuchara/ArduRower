// ArduRower for ESP32-2432S028 CYD.
// TFT configuration, rotation and backlight follow HA_UBOX_bridge/cyd_monitor.
// Workout calculation is shared directly with original ArduRower v0.07.
// BLE FTMS wire contract follows original ArduRower.ino and must stay unchanged.
#include <Arduino.h>
#include <NimBLEDevice.h>
#include "esp32-hal-bt.h"  // force Arduino BT retention link before initArduino()
#include "esp_bt.h"

// Arduino-ESP32 3.3.12's initArduino() frees BLE controller memory unless
// bleInUse() or an explicitly overridden btInUse() returns true. NimBLE-
// Arduino 1.4.3 does not automatically install those retention hooks in this
// PlatformIO framework, and the released memory cannot be reallocated during
// the same boot. These strong C symbols override Arduino's WEAK defaults
// before initArduino() runs, preventing esp_bt_controller_init() error 0x103.
extern "C" bool btInUse(void) { return true; }
extern "C" bool bleInUse(void) { return true; }
#include <TFT_eSPI.h>
#include <SPI.h>
#include <XPT2046_Touchscreen.h>
#include <math.h>
#include <Preferences.h>
#include "CydMotion.h"
#include "BleDutySchedule.h"
#include "TouchGestures.h"
#include <esp_system.h> // ESP32 reset diagnostics
#include "GhostRace.h"
#include "../../ArduRower/RowerMetrics.h"

static constexpr const char *FIRMWARE_VERSION = "0.09.15";
static constexpr int ROWERINPUT = 22; // P3: GPIO22 + GND, reed contact; NOT GPIO2 (TFT_DC)
static constexpr int BUTTONSPIN = 0;  // onboard BOOT, press AFTER the ESP32 starts
static constexpr int BACKLIGHT_PIN = TFT_BL;
static constexpr uint8_t DEFAULT_ROTATION = 3; // working HA_UBOX_bridge landscape
static constexpr uint32_t DISPLAY_INTERVAL_MS = 500;
static constexpr uint32_t HISTORY_INTERVAL_MS = 1000;
static constexpr uint8_t HISTORY_LENGTH = 120; // last 120 seconds
static constexpr uint16_t QUEUE_SIZE = 256;
static constexpr bool SEND_EQUIVALENT_POWER = false; // original FTMS payload default
static constexpr bool LOG_SENSOR_PULSES = false;
static constexpr uint32_t TREND_SAMPLE_MS=2000;
static constexpr uint16_t TREND_SAMPLES=300; // 10 minutes, full-width shared plot
static constexpr uint32_t HR_TIMEOUT_MS=6500;

// Original FTMS IDs, advertised name, service/characteristic structure and
// notification frequency: do not change without an opt-in compatibility test.
#define BLE_SERVICE_NAME "WR S4BL3"
#define FitnessMachineService 0x1826
#define FitnessMachineRowerData 0x2AD1
#define FitnessMachineFeature 0x2ACC
#define FitnessMachineControlPoint 0x2AD9
#define FitnessMachineStatus 0x2ADA
#define batteryLevel 0x2A19
#define DEVICE_INFORMATION 0x180A

TFT_eSPI tft;
TFT_eSprite scratch(&tft); // reusable 160x55 off-screen buffer (~17 KiB)
bool scratchReady = false;
SPIClass touchSPI(HSPI);
XPT2046_Touchscreen touch(33,36); // identical IRQ wiring to working HA_UBOX_bridge
bool touchReady = false;
bool touchedLast = false;
uint8_t touchReleasedSamples=0;
uint32_t touchLastCompletedMs=0;
static constexpr uint32_t TOUCH_REARM_MS=550;
static constexpr uint32_t TOUCH_PRESS_LIMIT_MS=2500;
uint32_t touchEvents=0;
uint32_t touchSwipeEvents=0;
uint32_t touchLastLogMs=0;
uint32_t touchLastPollMs=0;
uint32_t touchPressedAt = 0;
int16_t rawStartX = 0, rawStartY = 0, rawEndX = 0, rawEndY = 0;
float touchA=0, touchB=0, touchC=0, touchD=0, touchE=0, touchF=0;
bool touchCalibrated=false, pendingCalibration=false;
uint8_t touchRotation=DEFAULT_ROTATION;

RowerMetrics metrics;
cydmotion::Engine motion;
cydble::DutySchedule bleDuty;
volatile uint32_t ftmsNotifications=0;
volatile uint32_t ftmsConnectedAtMs=0;
volatile uint32_t ftmsDisconnectedAtMs=0;
volatile uint16_t receivedHr=0;
volatile uint32_t receivedHrAt=0;
volatile bool hrConnected=false;
volatile bool deviceConnected=false;
bool hrTaskCreated=false;
// Independent HR link status. "BLE OK" refers ONLY to Garmin's working
// FTMS connection, not to the reverse HR connection.
enum class HrState : uint8_t {
  Starting, Scan, NotFound, Connecting, GattConnected,
  Service, Characteristic, Subscribing, Descriptor, Receiving, NoData,
  Disconnected, LowMemory, InitFailed, Stalled, Paused
};
volatile HrState hrState=HrState::Starting;
volatile uint16_t hrAdvertisers=0;
volatile uint16_t hrMatches=0;
volatile uint32_t lastHrScanMs=0;
volatile uint32_t hrStageSinceMs=0;
volatile uint32_t hrNotifyPackets=0;
volatile HrState hrLastReportedStall=HrState::Starting;
const char *hrStatus(HrState s) {
  switch (s) {
    case HrState::Starting: return "HR INIT";
    case HrState::Scan: return "HR SKAN";
    case HrState::NotFound: return "HR BRAK";
    case HrState::Connecting: return "HR POL";
    case HrState::GattConnected: return "HR GATT";
    case HrState::Service: return "HR USL";
    case HrState::Characteristic: return "HR CHAR";
    case HrState::Subscribing: return "HR SUB";
    case HrState::Descriptor: return "HR CCCD";
    case HrState::Stalled: return "HR ZAW";
    case HrState::Paused: return "HR FTMS";
    case HrState::Receiving: return "HR OK";
    case HrState::NoData: return "HR DANE";
    case HrState::Disconnected: return "HR ROZ";
    case HrState::LowMemory: return "HR RAM";
    case HrState::InitFailed: return "HR BLAD";
  }
  return "HR --";
}

struct TrendPoint { uint16_t tenthKmh; uint8_t heartBpm; uint8_t reserved; };
TrendPoint trends[TREND_SAMPLES] = {};
uint16_t trendHead=0,trendCount=0;
uint32_t trendLastMs=0,trendRevision=0,trendDrawn=UINT32_MAX;
bool calibrationScreen=false;
float speedCalibration=cydmotion::Engine::kDefaultCalibration;
uint8_t lastCalibrationPaint=255;
uint16_t liveHr(uint32_t nowMs) {
  return (receivedHrAt && nowMs-receivedHrAt<HR_TIMEOUT_MS) ? receivedHr : 0;
}
void loadMotionCalibration() {
  Preferences p;
  if(p.begin("rower-motion",true)) {
    speedCalibration=p.getFloat("scale",cydmotion::Engine::kDefaultCalibration);
    p.end();
  }
  if(!isfinite(speedCalibration) || speedCalibration<0.3f || speedCalibration>1.5f)
    speedCalibration=cydmotion::Engine::kDefaultCalibration;
  motion.setCalibration(speedCalibration);
}
void saveMotionCalibration() {
  Preferences p;
  if(p.begin("rower-motion",false)) {
    p.putFloat("scale",speedCalibration);
    p.end();
  }
}
#include "SharedLinkHr.h"

NimBLEServer *pServer = nullptr;
NimBLECharacteristic *pCtrCharacteristic = nullptr, *pDtCharacteristic = nullptr;
NimBLECharacteristic *pFmfCharacteristic = nullptr, *pStCharacteristic = nullptr;
NimBLECharacteristic *pBatCharacteristic = nullptr;
bool oldDeviceConnected = false;
uint32_t lastFtmsAdvertCheckMs=0, lastFtmsAdvertReportMs=0;
uint32_t lastGhostMenuHeartbeatMs=0;

portMUX_TYPE pulseMux = portMUX_INITIALIZER_UNLOCKED;
volatile uint32_t pulseQueue[QUEUE_SIZE];
volatile uint16_t queueHead = 0, queueTail = 0;
volatile uint32_t lastEdgeUs = 0, droppedPulses = 0;
volatile bool haveEdge = false;
uint32_t lastDisplayMs = 0, lastBleMs = 0, lastHistoryMs = 0;
bool detailPage = false, buttonDown = false, longPressDone = false;
bool rawButtonDown = false;
uint32_t buttonChangedMs = 0, buttonPressedMs = 0;
// Two quick BOOT clicks open Ghost Race even without touch calibration.
// A single click still toggles the dashboard details after a short delay.
uint8_t dashboardBootClicks=0;
uint32_t dashboardBootClickAt=0;

struct HistorySample { float power; float speedKmh; };
HistorySample history[HISTORY_LENGTH] = {};
uint8_t historyHead = 0;
uint8_t historyCount = 0;
uint16_t backgroundColor, panelColor, faintColor, gridColor;
uint16_t powerColor, speedColor, labelColor, whiteColor, heartColor;
uint32_t historyRevision = 0, plottedRevision = UINT32_MAX;
bool pageRendered = false, displayedDetails = false;
bool displayedGhostWorkout=false;
// Text slots 0..11 = dashboard, 12..20 = Ghost Race. The old
// 12-element String array was indexed at 12..20 in race mode, overwriting
// adjacent global memory and causing spontaneous ESP32 resets.
static constexpr uint8_t kTextSlotCount = 26;
static constexpr uint8_t kMaxAssignedTextSlot = 25;
static_assert(kTextSlotCount > kMaxAssignedTextSlot, "Ghost Race fields exceed text cache");
String previousField[kTextSlotCount];
bool previousValid[kTextSlotCount] = {};
static_assert(sizeof(previousField) / sizeof(previousField[0]) ==
              sizeof(previousValid) / sizeof(previousValid[0]),
              "Text cache and validity flags must have matching sizes");
// Race mode has independent elapsed time, including stationary intervals.
enum class Mode : uint8_t { Dashboard, Race };
Mode activeMode = Mode::Dashboard;
ghostrace::Race ghost;
ghostrace::Record storedGhost;
uint32_t personalBestMs[3] = {};
uint8_t selectedDistance=0;
bool raceLayoutReady=false;
uint8_t displayedRaceScreen=255;
uint32_t lastRacePaintMs=0;
bool recordSaveFailed=false;
uint8_t lastRaceSelection=255;

class MyServerCallbacks : public NimBLEServerCallbacks {
 public:
  void onConnect(NimBLEServer *) override {
    deviceConnected=true;
    ftmsConnectedAtMs=millis();
    Serial.printf("FTMS: CONNECTED at %lu, HR=%s, packets=%lu\n",
                  (unsigned long)ftmsConnectedAtMs,
                  hrStatus(hrState),(unsigned long)ftmsNotifications);
  }
  void onConnect(NimBLEServer *,ble_gap_conn_desc *desc) override {
    if(desc)onFtmsHandleReady(desc->conn_handle);
  }
  void onDisconnect(NimBLEServer *) override {
    deviceConnected=false;
    ftmsDisconnectedAtMs=millis();
    Serial.printf("FTMS: DISCONNECTED at %lu, HR=%s, packets=%lu\n",
                  (unsigned long)ftmsDisconnectedAtMs,
                  hrStatus(hrState),(unsigned long)ftmsNotifications);
  }
  void onDisconnect(NimBLEServer *,ble_gap_conn_desc *) override {
    onFtmsHandleClosed();
  }
};
// Intentionally no-op: as in the existing firmware, FTMS Control Point procedures
// are NOT implemented. Preserving the existing Connect IQ integration is paramount.
class MyCallbacks : public NimBLECharacteristicCallbacks {
 public:
  void onWrite(NimBLECharacteristic *) override {}
};

void initBLE() {
  Serial.print(F("Init BLE:"));
  NimBLEDevice::init(BLE_SERVICE_NAME);
  NimBLEDevice::setCustomGapHandler(onHrGapEvent);
  pServer = NimBLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  NimBLEService *pService =
      pServer->createService(NimBLEUUID((uint16_t)FitnessMachineService));
  pDtCharacteristic = pService->createCharacteristic(
      NimBLEUUID((uint16_t)FitnessMachineRowerData),
      NIMBLE_PROPERTY::NOTIFY);
  // NimBLE creates CCCD for notify characteristics automatically.
  pDtCharacteristic->setCallbacks(new MyCallbacks());
  pFmfCharacteristic = pService->createCharacteristic(
      NimBLEUUID((uint16_t)FitnessMachineFeature),
      NIMBLE_PROPERTY::READ);
  pCtrCharacteristic = pService->createCharacteristic(
      NimBLEUUID((uint16_t)FitnessMachineControlPoint),
      NIMBLE_PROPERTY::WRITE);
  pCtrCharacteristic->setCallbacks(new MyCallbacks());
  NimBLEService *pBatteryService = pServer->createService(NimBLEUUID((uint16_t)0x180F));
  pBatCharacteristic = pBatteryService->createCharacteristic(
      NimBLEUUID((uint16_t)batteryLevel),
      NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
  pStCharacteristic = pService->createCharacteristic(
      NimBLEUUID((uint16_t)FitnessMachineStatus),
      NIMBLE_PROPERTY::NOTIFY);
  pService->start();
  pBatteryService->start();
  NimBLEService *pService2 = pServer->createService(NimBLEUUID((uint16_t)DEVICE_INFORMATION));
  NimBLECharacteristic *pCharacteristic24 = pService2->createCharacteristic(
      NimBLEUUID((uint16_t)0x2A24), NIMBLE_PROPERTY::READ);
  NimBLECharacteristic *pCharacteristic25 = pService2->createCharacteristic(
      NimBLEUUID((uint16_t)0x2A25), NIMBLE_PROPERTY::READ);
  NimBLECharacteristic *pCharacteristic26 = pService2->createCharacteristic(
      NimBLEUUID((uint16_t)0x2A26), NIMBLE_PROPERTY::READ);
  NimBLECharacteristic *pCharacteristic27 = pService2->createCharacteristic(
      NimBLEUUID((uint16_t)0x2A27), NIMBLE_PROPERTY::READ);
  NimBLECharacteristic *pCharacteristic28 = pService2->createCharacteristic(
      NimBLEUUID((uint16_t)0x2A28), NIMBLE_PROPERTY::READ);
  NimBLECharacteristic *pCharacteristic29 = pService2->createCharacteristic(
      NimBLEUUID((uint16_t)0x2A29), NIMBLE_PROPERTY::READ);
  pService2->start();
  // Preserve original device-information characteristic strings, not new IDs.
  pCharacteristic24->setValue("4");
  pCharacteristic25->setValue("0000");
  pCharacteristic26->setValue("0.07");
  pCharacteristic27->setValue("2.2BLE");
  pCharacteristic28->setValue("4.3");
  pCharacteristic29->setValue("Waterrower");
  char cRower[8];
  cRower[0] = 0x26;
  cRower[1] = SEND_EQUIVALENT_POWER ? 0x50 : 0x10;
  cRower[2] = cRower[3] = cRower[4] = 0;
  cRower[5] = cRower[6] = cRower[7] = 0;
  pFmfCharacteristic->setValue((uint8_t *)cRower, 8);
  NimBLEAdvertising *pAdvertising = pServer->getAdvertising();
  // Preserve the original Garmin FTMS advertisement prefix (Flags 0x06,
  // advertised 16-bit services 0x1826 + 0x180A). During the migration to
  // NimBLE, the name was only in a scan response and addServiceUUID() was
  // called AFTER custom setAdvertisementData(), which NimBLE ignores for
  // custom packets. Some Garmin clients filter the PRIMARY ad by name.
  // Include WR S4BL3 in the primary packet as well as scan response.
  // 9-byte original prefix + 10-byte name AD field = 19 bytes (<31).
  uint8_t advData[] = {0x02, 0x01, 0x06, 0x05, 0x02, 0x26, 0x18, 0x0a, 0x18};
  NimBLEAdvertisementData oAdvertisementData;
  oAdvertisementData.addData(std::string((const char*)advData,sizeof(advData)));
  oAdvertisementData.setName(BLE_SERVICE_NAME);
  pAdvertising->setScanResponse(true);
  pAdvertising->setAdvertisementData(oAdvertisementData);
  NimBLEAdvertisementData scanResponse;
  scanResponse.setName(BLE_SERVICE_NAME);
  pAdvertising->setScanResponseData(scanResponse);
  const bool advertisingStarted=pAdvertising->start();
  Serial.printf("\nFTMS: primary advertising started=%s active=%s name=%s service=0x1826 adBytes=%u\n",
                advertisingStarted?"yes":"no",
                pAdvertising->isAdvertising()?"yes":"no",
                BLE_SERVICE_NAME,
                (unsigned)oAdvertisementData.getPayload().size());
}

uint16_t bounded16(float value) {
  if (!isfinite(value) || value <= 0) return 0;
  return value >= 65535 ? 65535 : (uint16_t)roundf(value);
}
uint8_t encodedStrokeRate(float rate) {
  return (uint8_t)min((uint16_t)255, bounded16(rate * 2.0f));
}
void put16(uint8_t *data, uint8_t &offset, uint16_t value) {
  data[offset++] = value & 0xff;
  data[offset++] = value >> 8;
}
void setCxRowerData() {
  // Byte format and flag semantics copied from the working ArduRower v0.07.
  uint8_t data[19];
  uint8_t offset = 0;
  const uint16_t flags = SEND_EQUIVALENT_POWER ? 0x087e : 0x081e;
  put16(data, offset, flags);
  data[offset++] = encodedStrokeRate(metrics.strokeRate);
  put16(data, offset, (uint16_t)min(metrics.strokes, (uint32_t)65535));
  data[offset++] = encodedStrokeRate(metrics.averageStrokeRate());
  const uint32_t distance = (uint32_t)min(metrics.distance(), 16777215.0f);
  data[offset++] = distance & 0xff;
  data[offset++] = (distance >> 8) & 0xff;
  data[offset++] = (distance >> 16) & 0xff;
  put16(data, offset, metrics.pace() > 0 ? bounded16(metrics.pace()) : 0xffff);
  put16(data, offset, metrics.averagePace() > 0 ? bounded16(metrics.averagePace()) : 0xffff);
  if (SEND_EQUIVALENT_POWER) {
    put16(data, offset, min(bounded16(metrics.equivalentPower()), (uint16_t)32767));
    put16(data, offset, min(bounded16(metrics.averageEquivalentPower()), (uint16_t)32767));
  }
  put16(data, offset, bounded16(metrics.seconds()));
  pDtCharacteristic->setValue(data, offset);
  pDtCharacteristic->notify();
  // GPIO33 on CYD is the touch CS, NOT the Heltec battery ADC. The board is
  // USB-powered; keep the existing Battery Service present but report full
  // external power availability instead of reading bogus touch-pin voltages.
  uint8_t battery = 100;
  pBatCharacteristic->setValue(&battery, 1);
  pBatCharacteristic->notify();
}

void IRAM_ATTR rowerdebounceinterrupt() {
  const uint32_t now = micros();
  portENTER_CRITICAL_ISR(&pulseMux);
  if (!haveEdge || (uint32_t)(now - lastEdgeUs) >= SENSOR_DEBOUNCE_US) {
    haveEdge = true;
    lastEdgeUs = now;
    const uint16_t next = (queueHead + 1) % QUEUE_SIZE;
    if (next != queueTail) {
      pulseQueue[queueHead] = now;
      queueHead = next;
    } else ++droppedPulses;
  }
  portEXIT_CRITICAL_ISR(&pulseMux);
}

void resetWorkout() {
  portENTER_CRITICAL(&pulseMux);
  queueHead = queueTail = 0;
  haveEdge = false;
  droppedPulses = 0;
  metrics.reset(micros());
  portEXIT_CRITICAL(&pulseMux);
  motion.reset(micros());
  motion.setCalibration(speedCalibration);
  trendHead=trendCount=0;
  memset(trends,0,sizeof(trends));
  trendRevision++;
  trendDrawn=UINT32_MAX;
  historyHead = 0;
  historyCount = 0;
  memset(history, 0, sizeof(history));
  lastHistoryMs = millis();
  ++historyRevision;
}

// Persistent reference runs (not rewritten each second). The NVS blob is
// versioned/checksummed and accepted only for the same distance/calibration.
void resetRenderCache();
void renderRaceUI();
const uint16_t raceDistances[3] = {500, 1000, 2000};
const char *raceKeys[3] = {"g500", "g1000", "g2000"};
bool loadBest(uint8_t index, ghostrace::Record &out) {
  Preferences p;
  if (!p.begin("rower-races", true)) return false;
  const size_t n = p.getBytesLength(raceKeys[index]);
  const size_t read = (n == sizeof(out)) ?
      p.getBytes(raceKeys[index], &out, sizeof(out)) : 0;
  p.end();
  return read == sizeof(out) && ghostrace::valid(out, raceDistances[index],motion.calibrationMilli());
}
bool saveBest(uint8_t index, const ghostrace::Record &record) {
  if (!ghostrace::valid(record, raceDistances[index],motion.calibrationMilli())) return false;
  Preferences p;
  if (!p.begin("rower-races", false)) return false;
  const bool ok = p.putBytes(raceKeys[index], &record, sizeof(record)) == sizeof(record);
  p.end();
  return ok;
}
void loadBestTimes() {
  for (int i=0;i<3;++i)
    personalBestMs[i] = loadBest(i, storedGhost) ? storedGhost.finishMs : 0;
}
void startRace(uint8_t index) {
  if (index >= 3) return;
  selectedDistance = index;
  const bool found = loadBest(index, storedGhost);
  ghost.arm(raceDistances[index], found ? &storedGhost : nullptr,
            motion.calibrationMilli());
  // Start a fresh interval, so race distance is not polluted by earlier
  // dashboard rowing. Do not change the BLE/FTMS encoder in any way.
  resetWorkout();
  activeMode = Mode::Race;
  detailPage=false;pageRendered=false;
  Serial.println("GHOST: DASHBOARD enabled after distance choice");
  Serial.printf("GHOST: armed target=%u, best=%s, calibration=%lu\n",
                (unsigned)ghost.targetMeters,found?"yes":"no",
                (unsigned long)motion.calibrationMilli());
  raceLayoutReady = false;
  lastRacePaintMs = 0;
  recordSaveFailed=false;
}
void openRaceMenu() {
  if (ghost.phase == ghostrace::Phase::Running ||
      ghost.phase == ghostrace::Phase::Armed) ghost.cancel();
  ghost.phase = ghostrace::Phase::Idle;
  loadBestTimes();
  raceLayoutReady=false;
}
void toggleMode() {
  if (activeMode == Mode::Dashboard) {
    Serial.println("GHOST: ENTER");
    activeMode=Mode::Race;
    if (ghost.phase == ghostrace::Phase::Finished ||
        ghost.phase == ghostrace::Phase::Cancelled)
      openRaceMenu();
    else if (ghost.phase == ghostrace::Phase::Idle)
      loadBestTimes();
    raceLayoutReady=false;
    // Never launch a blocking touch-calibration wizard automatically when
    // entering the menu. BOOT can operate Ghost Race without touch setup.
  } else {
    Serial.println("GHOST: EXIT");
    if(ghost.phase==ghostrace::Phase::Running || ghost.phase==ghostrace::Phase::Armed)ghost.cancel();
    activeMode=Mode::Dashboard;
    pageRendered=false;
  }
  resetRenderCache();
}
void updateButton(uint32_t now) {
  const bool down = digitalRead(BUTTONSPIN) == LOW;
  if (down != rawButtonDown) { rawButtonDown=down; buttonChangedMs=now; }
  if (now - buttonChangedMs >= 30 && down != buttonDown) {
    buttonDown=down;
    if (down) { buttonPressedMs=now; longPressDone=false; }
    else if (!longPressDone) {
      if (activeMode == Mode::Dashboard) {
        if(dashboardBootClicks==1 && now-dashboardBootClickAt<=480) {
          dashboardBootClicks=0;
          toggleMode(); // explicit physical fallback for Ghost Race
          Serial.println("GHOST: entered menu using double BOOT click");
        } else {
          dashboardBootClicks=1;
          dashboardBootClickAt=now;
        }
      } else if (ghost.phase == ghostrace::Phase::Idle)
        selectedDistance=(selectedDistance+1)%3;
      else if (ghost.phase == ghostrace::Phase::Finished ||
               ghost.phase == ghostrace::Phase::Cancelled) openRaceMenu();
    }
  }
  if(dashboardBootClicks && now-dashboardBootClickAt>480) {
    dashboardBootClicks=0;
    if(activeMode==Mode::Dashboard) detailPage=!detailPage;
  }
  if (buttonDown && !longPressDone && now-buttonPressedMs>=1500) {
    longPressDone=true;
    if (activeMode == Mode::Dashboard) {
      if(ghost.phase==ghostrace::Phase::Running ||
         ghost.phase==ghostrace::Phase::Armed)ghost.cancel();
      resetWorkout();
    }
    else if (ghost.phase == ghostrace::Phase::Idle) {
      Serial.printf("GHOST: BOOT long start %u m\n",
                    (unsigned)raceDistances[selectedDistance]);
      startRace(selectedDistance);
    }
    else if (ghost.phase == ghostrace::Phase::Running ||
             ghost.phase == ghostrace::Phase::Armed) openRaceMenu();
    else openRaceMenu();
  }
}
bool mappedTouch(int16_t rawX,int16_t rawY,int16_t &x,int16_t &y) {
  if (!touchCalibrated) return false;
  float fx=touchA*rawX+touchB*rawY+touchC;
  float fy=touchD*rawX+touchE*rawY+touchF;
  if (touchRotation!=DEFAULT_ROTATION) {fx=319-fx;fy=239-fy;}
  if (!isfinite(fx)||!isfinite(fy)||fx < -18||fx >337||fy < -18||fy >257)
    return false;
  x=constrain((int)lroundf(fx),0,319);
  y=constrain((int)lroundf(fy),0,239);
  return true;
}
void readTouchCalibration() {
  Preferences p;
  if(p.begin("rower-touch",true)) {
    const bool forced=p.getBool("tc_force",false);
    p.end();
    if(forced) {touchCalibrated=false;Serial.println("TOUCH: calibration requested from Ghost menu");return;}
  }
  // Prefer the calibration from the previously working UBox CYD firmware.
  for (const char *area : {"rower-touch","ubox-cyd"}) {
    if (!p.begin(area,true)) continue;
    if (p.getBool("tc_ok",false)) {
      touchA=p.getFloat("tc_a",0);touchB=p.getFloat("tc_b",0);
      touchC=p.getFloat("tc_c",0);touchD=p.getFloat("tc_d",0);
      touchE=p.getFloat("tc_e",0);touchF=p.getFloat("tc_f",0);
      touchRotation=p.getUChar("tc_rot",DEFAULT_ROTATION);
      touchCalibrated=isfinite(touchA)&&isfinite(touchB)&&
        isfinite(touchC)&&isfinite(touchD)&&isfinite(touchE)&&isfinite(touchF) &&
        (fabsf(touchA)+fabsf(touchB)>.01f) &&
        (fabsf(touchD)+fabsf(touchE)>.01f);
    }
    p.end();
    if (touchCalibrated) {
      Serial.printf("TOUCH: calibrated from %s rotation=%u\n",area,(unsigned)touchRotation);
      break;
    }
  }
  if(!touchCalibrated)Serial.println("TOUCH: no saved calibration; using raw axis fallback for Ghost menu");
}
bool readCalPoint(int x,int y,int &rx,int &ry,int index) {
  tft.fillScreen(backgroundColor);
  tft.setTextDatum(TL_DATUM);tft.setTextColor(whiteColor,backgroundColor);
  tft.drawString("KALIBRACJA DOTYKU",10,10,2);
  tft.drawString(String(index)+"/3: dotknij zolty +",10,37,2);
  tft.drawFastHLine(x-13,y,27,powerColor);
  tft.drawFastVLine(x,y-13,27,powerColor);
  const uint32_t began=millis();
  while (millis()-began<15000UL) {
    if (touch.touched()) {
      long xx=0,yy=0;int n=0;
      const uint32_t hold=millis();
      while (touch.touched() && millis()-hold<650UL) {
        const TS_Point pt=touch.getPoint();
        if(pt.z>100){xx+=pt.x;yy+=pt.y;n++;}
        delay(25);
      }
      while (touch.touched()&&millis()-hold<1500UL)delay(25);
      if(n>=2){rx=xx/n;ry=yy/n;return true;}
    }
    delay(25);
  }
  return false;
}
bool calibrateRaceTouch() {
  const int tx[3]={25,295,25}, ty[3]={65,65,210};
  int rx[3],ry[3];
  for(int i=0;i<3;i++)
    if(!readCalPoint(tx[i],ty[i],rx[i],ry[i],i+1)) return false;
  const float ux=rx[1]-rx[0],uy=ry[1]-ry[0];
  const float vx=rx[2]-rx[0],vy=ry[2]-ry[0];
  const float det=ux*vy-uy*vx;
  if(fabsf(det)<80000.0f)return false;
  touchA=270.0f*vy/det;touchB=-270.0f*vx/det;
  touchD=-145.0f*uy/det;touchE=145.0f*ux/det;
  touchC=25.0f-touchA*rx[0]-touchB*ry[0];
  touchF=65.0f-touchD*rx[0]-touchE*ry[0];
  touchRotation=DEFAULT_ROTATION;touchCalibrated=true;
  Preferences p;
  if(p.begin("rower-touch",false)){
    p.putBool("tc_ok",true);p.putBool("tc_force",false);
    p.putUChar("tc_rot",DEFAULT_ROTATION);
    p.putFloat("tc_a",touchA);p.putFloat("tc_b",touchB);
    p.putFloat("tc_c",touchC);p.putFloat("tc_d",touchD);
    p.putFloat("tc_e",touchE);p.putFloat("tc_f",touchF);
    p.end();
  }
  return true;
}
void touchTap(int16_t rx,int16_t ry) {
  if(calibrationScreen) {
    int16_t x,y;
    if(!mappedTouch(rx,ry,x,y)) return;
    if(y>105 && y<160) {
      if(x<158) speedCalibration=fmaxf(0.3f,speedCalibration-0.05f);
      else speedCalibration=fminf(1.5f,speedCalibration+0.05f);
      motion.setCalibration(speedCalibration);
      saveMotionCalibration();
      // Calibration invalidates any in-progress Ghost Race measurement.
      // Never append samples under the new scale to an old-calibration race.
      if(ghost.phase==ghostrace::Phase::Armed ||
         ghost.phase==ghostrace::Phase::Running) ghost.cancel();
      ghost.phase=ghostrace::Phase::Idle;
      // New calibration changes virtual meters. Invalidate race replay
      // compatibility through GhostRace::Record.scaleMilli.
      resetWorkout();loadBestTimes();
      lastCalibrationPaint=255;
    } else if(y>=170) {
      calibrationScreen=false;
      pageRendered=false;
    }
    return;
  }
  if(activeMode==Mode::Dashboard) {
    int16_t x,y;
    // Short tap on upper-left header is a one-step Ghost Race entry.
    // Long header hold still opens virtual-distance calibration.
    if(mappedTouch(rx,ry,x,y) && x<104 && y<27) {
      toggleMode();
      Serial.println("GHOST: entered menu using dashboard title tap");
    } else detailPage=!detailPage;
    return;
  }
  if(ghost.phase==ghostrace::Phase::Finished ||
     ghost.phase==ghostrace::Phase::Cancelled){openRaceMenu();return;}
  int16_t x,y;
  const bool calibrated=mappedTouch(rx,ry,x,y);
  if(!calibrated) {
    // CYD horizontal view: with XPT2046 rotation=0, physical screen Y is
    // the reversed raw X axis. Unlike v0.09.8, tapping a row must choose
    // THAT row, not cycle all distances. Do not start a calibration wizard.
    x=160;
    y=constrain((int)((4095L-(long)rx)*240L/4095L),0,239);
    Serial.printf("GHOST: uncalibrated tap raw=%d,%d screenY~%d\n",
                  (int)rx,(int)ry,(int)y);
  }
  if(ghost.phase==ghostrace::Phase::Running ||
     ghost.phase==ghostrace::Phase::Armed) {
    if(y>=216) {
      Serial.println("GHOST: touch MENU cancels active race");
      openRaceMenu();
    }
    return;
  }
  if(ghost.phase!=ghostrace::Phase::Idle)return;
  // Touch-only recalibration control in the Ghost menu. Restart safely so
  // the three-point wizard runs before starting BLE/FTMS, as in UBox.
  if(y>=25 && y<53 && (!calibrated || x>=208)) {
    Preferences p;
    if(p.begin("rower-touch",false)) {
      p.putBool("tc_force",true);
      p.end();
    }
    Serial.println("TOUCH: Ghost menu requested three-point calibration; reboot");
    delay(100);
    ESP.restart();
    return;
  }
  Serial.printf("GHOST: menu tap mapped=(%d,%d), selection=%u calibrated=%d raw=%d,%d\n",
                (int)x,(int)y,(unsigned)selectedDistance,calibrated?1:0,(int)rx,(int)ry);
  // Three entire rows are large touch targets with no small hitbox.
  if(y>=49 && y<108) {Serial.println("GHOST: SELECT 500");startRace(0);}
  else if(y>=108 && y<168) {Serial.println("GHOST: SELECT 1000");startRace(1);}
  else if(y>=168 && y<229) {Serial.println("GHOST: SELECT 2000");startRace(2);}
}
void updateTouch(uint32_t now) {
  if(!touchReady || (uint32_t)(now-touchLastPollMs)<20)return;
  touchLastPollMs=now;
  // Restore the same physical IRQ-based read sequence as HA_UBOX_bridge.
  // Earlier versions read getPoint() with IRQ unregistered, then inferred
  // physical release from stale high Z. That allowed the very same finger
  // motion to switch Dashboard -> Ghost -> Dashboard in succession.
  const bool pressed=touch.touched();
  if(!pressed) {
    if(!touchedLast)return;
    // The first missing poll may be SPI jitter; require a stable release.
    if(++touchReleasedSamples<2)return;
    touchReleasedSamples=0;
    touchedLast=false;
    const uint32_t duration=now-touchPressedAt;
    const int dx=(int)rawEndX-(int)rawStartX;
    const int dy=(int)rawEndY-(int)rawStartY;
    const float mappedDx=touchA*dx+touchB*dy;
    const float mappedDy=touchD*dx+touchE*dy;
    const bool swipe=cydtouch::pageSwipe(dx,dy,touchCalibrated,mappedDx,mappedDy);
    Serial.printf("TOUCH: UP raw=%d,%d delta=%d,%d duration=%lu swipe=%d cal=%d mode=%d\n",
          (int)rawEndX,(int)rawEndY,dx,dy,
          (unsigned long)duration,swipe?1:0,touchCalibrated?1:0,(int)activeMode);
    if(duration<55 || duration>TOUCH_PRESS_LIMIT_MS ||
       (uint32_t)(now-touchLastCompletedMs)<TOUCH_REARM_MS)return;
    // Exactly one command per complete physical contact and a release
    // rearm barrier. The next contact cannot be born from the swipe tail.
    touchLastCompletedMs=now;
    if(swipe && !calibrationScreen) {
      ++touchSwipeEvents;
      Serial.printf("GHOST: RELEASE SWIPE #%lu raw=%d,%d mode=%d\n",
                    (unsigned long)touchSwipeEvents,dx,dy,(int)activeMode);
      toggleMode();
    } else if(duration<=1100 && max(abs(dx),abs(dy))<490) {
      touchTap(rawEndX,rawEndY);
    } else if(duration>=1100 && max(abs(dx),abs(dy))<490 &&
              activeMode==Mode::Dashboard) {
      int16_t px,py;
      if(mappedTouch(rawEndX,rawEndY,px,py) && py<38) {
        calibrationScreen=true;lastCalibrationPaint=255;
      }
    }
    return;
  }
  // touch.touched() is the same authoritative contact condition used by
  // HA_UBOX_bridge, not Z>=400 sampled without a matching IRQ lifecycle.
  const TS_Point pt=touch.getPoint();
  if(!touchedLast) {
    if(touchLastCompletedMs &&
       (uint32_t)(now-touchLastCompletedMs)<TOUCH_REARM_MS)return;
    touchedLast=true;touchReleasedSamples=0;
    touchPressedAt=now;
    rawStartX=rawEndX=pt.x;rawStartY=rawEndY=pt.y;
    ++touchEvents;
    Serial.printf("TOUCH: DOWN #%lu raw=%d,%d z=%d irq=%d\n",
                  (unsigned long)touchEvents,pt.x,pt.y,pt.z,digitalRead(36));
    return;
  }
  touchReleasedSamples=0;
  rawEndX=pt.x;rawEndY=pt.y;
}
String formatTime(uint32_t seconds) {
  char buffer[16];
  if (seconds >= 3600) {
    snprintf(buffer, sizeof(buffer), "%lu:%02lu:%02lu",
             (unsigned long)(seconds / 3600), (unsigned long)(seconds / 60 % 60),
             (unsigned long)(seconds % 60));
  } else {
    snprintf(buffer, sizeof(buffer), "%02lu:%02lu",
             (unsigned long)(seconds / 60), (unsigned long)(seconds % 60));
  }
  return String(buffer);
}
String formatPace(float seconds) {
  return seconds > 0 ? formatTime((uint32_t)roundf(seconds)) : String("--:--");
}
void recordTrend(uint32_t now) {
  const float kmh=motion.speed*3.6f;
  const float tenth=fmaxf(0.0f,fminf(600.0f,kmh*10.0f));
  trends[trendHead]={static_cast<uint16_t>(lroundf(tenth)),
                     static_cast<uint8_t>(liveHr(now)),0};
  trendHead=(trendHead+1)%TREND_SAMPLES;
  if(trendCount<TREND_SAMPLES)++trendCount;
  ++trendRevision;
}
const TrendPoint &trendAt(uint16_t i) {
  return trends[(trendHead+TREND_SAMPLES-trendCount+i)%TREND_SAMPLES];
}
void recordHistory() {
  history[historyHead] = {(float)liveHr(millis()), metrics.speed * 3.6f};
  historyHead = (historyHead + 1) % HISTORY_LENGTH;
  if (historyCount < HISTORY_LENGTH) ++historyCount;
  ++historyRevision;
}
const HistorySample &getHistory(uint8_t chronologicalIndex) {
  const uint8_t oldest = (historyHead + HISTORY_LENGTH - historyCount) % HISTORY_LENGTH;
  return history[(oldest + chronologicalIndex) % HISTORY_LENGTH];
}
float sampleValue(uint8_t index, bool power) {
  const HistorySample &s = getHistory(index);
  return power ? s.power : s.speedKmh;
}
float historyMax(bool power) {
  float result = power ? 150.0f : 2.0f;
  for (uint8_t i = 0; i < historyCount; ++i)
    result = max(result, sampleValue(i, power));
  return result * 1.12f;
}

// ST7789 refresh: build changing regions off-screen and crop-push them to TFT.
// fillScreen() is used only when switching pages, NEVER on the 500-ms tick.
static constexpr int16_t BUFFER_W = 160, BUFFER_H = 55;
void resetRenderCache() {
  for (uint8_t i = 0; i < kTextSlotCount; i++) previousValid[i] = false;
  plottedRevision = UINT32_MAX;
}
void textPatch(uint8_t slot, int x, int y, int w, int h, String text,
               uint16_t fg, uint16_t bg, uint8_t font, bool right = false) {
  // Guard even if a future feature accidentally uses a new slot ID.
  if (slot >= kTextSlotCount) {
    Serial.printf("UI textPatch: invalid slot %u (max %u)\n",
                  (unsigned)slot, (unsigned)(kTextSlotCount-1));
    return;
  }
  if (previousValid[slot] && previousField[slot] == text) return;
  previousValid[slot] = true; previousField[slot] = text;
  if (scratchReady) {
    scratch.fillSprite(bg);
    scratch.setTextColor(fg, bg);
    scratch.setTextDatum(right ? TR_DATUM : TL_DATUM);
    if (right) scratch.drawRightString(text, w - 2, 0, font);
    else scratch.drawString(text, 0, 0, font);
    scratch.pushSprite(x, y, 0, 0, w, h);
  } else {
    // In the unlikely event of low heap, update only a small rectangle.
    tft.fillRect(x, y, w, h, bg);
    tft.setTextColor(fg, bg);
    tft.setTextDatum(right ? TR_DATUM : TL_DATUM);
    if (right) tft.drawRightString(text, x+w-2, y, font);
    else tft.drawString(text, x, y, font);
  }
}
void cardValue(uint8_t slot, int x, bool power) {
  if (slot >= kTextSlotCount) {
    Serial.printf("UI cardValue: invalid slot %u\n", (unsigned)slot);
    return;
  }
  const float val = power ? metrics.equivalentPower() : metrics.speed * 3.6f;
  const String txt = power ? "~" + String((int)roundf(val)) : String(val, 1);
  if (previousValid[slot] && previousField[slot] == txt) return;
  previousValid[slot] = true; previousField[slot] = txt;
  const int left = x+8, y=60, w=133, h=33;
  const uint16_t fg = power ? powerColor : speedColor;
  if (scratchReady) {
    scratch.fillSprite(panelColor);
    scratch.setTextDatum(TL_DATUM);
    scratch.setTextColor(fg, panelColor);
    scratch.drawString(txt, 0, 0, 4);
    scratch.setTextColor(labelColor, panelColor);
    scratch.drawRightString(power ? "W" : "km/h", w-1, 11, 2);
    scratch.pushSprite(left, y, 0, 0, w, h);
  } else {
    tft.fillRect(left, y, w, h, panelColor);
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(fg, panelColor);
    tft.drawString(txt, left, y, 4);
    tft.setTextColor(labelColor, panelColor);
    tft.drawRightString(power ? "W" : "km/h", left+w-1, y+11, 2);
  }
}
void plot(int x, int y, int w, int h, bool power) {
  const uint16_t bg = w > 200 ? backgroundColor : panelColor;
  const uint16_t fg = power ? powerColor : speedColor;
  const float peak = historyCount ? historyMax(power) : 1.0f;
  // A large 302x54 plot is split into two 151-pixel portions. One small
  // reusable sprite avoids PSRAM, frame tearing and heap fragmentation.
  for (int left = 0; left < w; left += BUFFER_W) {
    const int part = min(BUFFER_W, (int16_t)(w-left));
    if (scratchReady) scratch.fillSprite(bg);
    else tft.fillRect(x+left, y, part, h, bg);
    if (scratchReady) {
      scratch.drawFastHLine(0, h-1, part, gridColor);
      scratch.drawFastHLine(0, h/2, part, gridColor);
    } else {
      tft.drawFastHLine(x+left, y+h-1, part, gridColor);
      tft.drawFastHLine(x+left, y+h/2, part, gridColor);
    }
    int prevX=0, prevY=0;
    for (uint8_t i=0; i<historyCount; ++i) {
      const int px = w-1 - (historyCount-1-i)*(w-1)/(HISTORY_LENGTH-1);
      const int py = h-1 - (int)roundf(constrain(sampleValue(i,power), 0.0f, peak)/peak*(h-1));
      if (i == 0) {
        if (px>=left && px<left+part) {
          if (scratchReady) scratch.fillCircle(px-left,py,1,fg);
          else tft.fillCircle(x+px,y+py,1,fg);
        }
      } else if (max(prevX,px)>=left && min(prevX,px)<left+part) {
        if (scratchReady) scratch.drawLine(prevX-left,prevY,px-left,py,fg);
        else tft.drawLine(x+prevX,y+prevY,x+px,y+py,fg);
      }
      prevX=px; prevY=py;
    }
    if (scratchReady) scratch.pushSprite(x+left,y,0,0,part,h);
  }
}
// Two independent units on one long graph, with a fixed mapping to avoid
// rescaling mid-session. Heartbeat samples that are stale/absent break the HR
// line; they must never be drawn as zero or mistaken for a real measurement.
void renderSharedTrend(int x,int y,int width,int height,bool ghostTraining=false) {
  const int16_t speedMaxTenth=220; // 22.0 km/h right scale
  const int hrMin=60,hrMax=190; // 60..190 BPM left scale
  const uint16_t speedInk=ghostTraining?(speedColor>>1)&0x7BEF:speedColor;
  const uint16_t hrInk=ghostTraining?(heartColor>>1)&0x7BEF:heartColor;
  const uint16_t ghostInk=(powerColor>>1)&0x7BEF;
  const bool ghostCurve=ghostTraining && ghost.hasBest && ghost.phase==ghostrace::Phase::Running;
  const uint32_t ghostElapsed=ghostCurve?ghost.elapsed(millis()):0;
  for(int left=0;left<width;left+=BUFFER_W) {
    const int part=min((int)BUFFER_W,width-left);
    if(scratchReady) {
      scratch.fillSprite(panelColor);
      for(int i=0;i<3;i++)
        scratch.drawFastHLine(0,i*(height-1)/2,part,gridColor);
    }else{
      tft.fillRect(x+left,y,part,height,panelColor);
      for(int i=0;i<3;i++)
        tft.drawFastHLine(x+left,y+i*(height-1)/2,part,gridColor);
    }
    int previousX=0,previousHrY=0,previousSpeedY=0;
    bool prevHrValid=false,prevSpeedValid=false,prevGhostValid=false;
    int previousGhostY=0;
    for(uint16_t i=0;i<trendCount;++i) {
      const TrendPoint &sample=trendAt(i);
      const int xx=width-1-(int)((trendCount-1-i)*(width-1)/(TREND_SAMPLES-1));
      const int hrY=height-1-(int)((constrain((int)sample.heartBpm,hrMin,hrMax)-hrMin)*
                                  (height-1)/(hrMax-hrMin));
      const int spY=height-1-(int)((constrain((int)sample.tenthKmh,0,
                                (int)speedMaxTenth))*(height-1)/speedMaxTenth);
      bool hrValid=sample.heartBpm>=hrMin;
      const uint32_t age=(uint32_t)(trendCount-1-i)*TREND_SAMPLE_MS;
      const bool ghostValid=ghostCurve && ghostElapsed>=age;
      int ghostY=height-1;
      if(ghostValid) {
        const uint32_t t=ghostElapsed-age,start=t>2000?t-2000:0;
        const float dt=(t-start)*0.001f;
        const float meters=ghostrace::interpolate(ghost.best,t)-
                           ghostrace::interpolate(ghost.best,start);
        const float kmh=dt>0?constrain(meters*3.6f/dt,0.0f,22.0f):0;
        ghostY=height-1-(int)(kmh*10.0f*(height-1)/speedMaxTenth);
      }
      if(i>0 && xx>=left-2 && previousX<left+part) {
        if(prevSpeedValid) {
          if(scratchReady)scratch.drawLine(previousX-left,previousSpeedY,xx-left,spY,speedInk);
          else tft.drawLine(x+previousX,y+previousSpeedY,x+xx,y+spY,speedColor);
        }
        if(hrValid && prevHrValid) {
          if(scratchReady)scratch.drawLine(previousX-left,previousHrY,xx-left,hrY,hrInk);
          else tft.drawLine(x+previousX,y+previousHrY,x+xx,y+hrY,heartColor);
        }
      }
      if(i>0 && ghostValid && prevGhostValid && (i%2)==0 &&
         xx>=left-2 && previousX<left+part) {
        if(scratchReady)scratch.drawLine(previousX-left,previousGhostY,xx-left,ghostY,ghostInk);
        else tft.drawLine(x+previousX,y+previousGhostY,x+xx,y+ghostY,ghostInk);
      }
      previousX=xx;previousHrY=hrY;previousSpeedY=spY;previousGhostY=ghostY;
      prevHrValid=hrValid;prevSpeedValid=true;prevGhostValid=ghostValid;
    }
    if(scratchReady) scratch.pushSprite(x+left,y,0,0,part,height);
  }
}
void calibrationPage() {
  if(lastCalibrationPaint!=255)return;
  tft.fillScreen(backgroundColor);
  tft.setTextColor(whiteColor,backgroundColor);
  tft.setTextDatum(TL_DATUM);
  tft.drawString("KALIBRACJA DYSTANSU",9,6,2);
  tft.setTextColor(labelColor,backgroundColor);
  tft.drawString("Skala wirtualna (nie S4)",9,36,2);
  tft.setTextColor(whiteColor,backgroundColor);
  tft.drawCentreString(String(speedCalibration,2)+" x",160,61,4);
  tft.fillRoundRect(10,105,144,52,7,panelColor);
  tft.fillRoundRect(166,105,144,52,7,panelColor);
  tft.setTextColor(heartColor,panelColor);
  tft.drawCentreString("-0.05",82,118,4);
  tft.setTextColor(speedColor,panelColor);
  tft.drawCentreString("+0.05",238,118,4);
  tft.fillRoundRect(10,180,300,51,7,panelColor);
  tft.setTextColor(whiteColor,panelColor);
  tft.drawCentreString("ZAPISZ I WROC",160,192,2);
  lastCalibrationPaint=0;
}
void pageLayout() {
  const bool ghostWorkout=activeMode==Mode::Race &&
   (ghost.phase==ghostrace::Phase::Armed || ghost.phase==ghostrace::Phase::Running);
  tft.fillScreen(backgroundColor); // only on view change
  tft.setTextDatum(TL_DATUM);
  tft.fillRect(0,0,320,24,panelColor);
  tft.setTextColor(whiteColor,panelColor);
  tft.drawString(ghostWorkout?("DUCH "+String(ghost.targetMeters)+"m"):String("ARDUROWER"),7,3,2);
  // Header title: tap to enter Ghost Race; horizontal swipes are primary.
  if(!detailPage && !ghostWorkout) {
    tft.setTextColor(labelColor,backgroundColor);
    tft.drawString("<  GHOST  >",77,29,1);
  }
  if(ghostWorkout){tft.setTextColor(labelColor,backgroundColor);tft.drawString(ghost.phase==ghostrace::Phase::Armed?"CZEKAM NA RUCH":(ghost.hasBest?"PB OK":"BRAK PB"),73,29,1);}
  tft.drawString(FIRMWARE_VERSION,108,8,1);
  if(!detailPage || ghostWorkout){
    tft.setTextColor(heartColor,backgroundColor);
    tft.drawString("PULS",8,27,2);
    tft.setTextColor(speedColor,backgroundColor);
    tft.drawString("PREDKOSC",165,27,2);
    tft.fillRoundRect(7,87,306,62,4,panelColor);
    tft.setTextColor(labelColor,backgroundColor);
    const int xx[3]={8,115,224};
    const char *names[6]={"CZAS","DYSTANS","SPM",ghostWorkout?"ROZNICA / DUCH":"TEMPO /500m","MOC ~W","RUCHY"};
    for(int i=0;i<6;++i)
      tft.drawString(names[i],xx[i%3],i<3?156:199,1);
  }else{
    tft.setTextColor(whiteColor,backgroundColor);
    tft.drawString("ANALIZA CYKLU",9,28,2);
    tft.setTextColor(labelColor,backgroundColor);
    tft.drawString("FAZA",9,60,2);
    tft.drawString("IMPULSY",178,60,2);
    tft.drawString("RUCHY",9,114,2);
    tft.drawString("ZMIANY KIERUNKU ~",178,114,2);
    tft.fillRoundRect(8,176,304,43,4,panelColor);
    tft.setTextColor(labelColor,backgroundColor);
    tft.drawString("DOTKNIJ, ABY WROCIC",12,224,1);
  }
  pageRendered=true;displayedDetails=detailPage;displayedGhostWorkout=ghostWorkout;
  resetRenderCache();
  trendDrawn=UINT32_MAX;
}
void updateUI() {
  const bool ghostWorkout=activeMode==Mode::Race &&
   (ghost.phase==ghostrace::Phase::Armed || ghost.phase==ghostrace::Phase::Running);
  if(activeMode==Mode::Race && !ghostWorkout) {renderRaceUI();return;}
  if(calibrationScreen && !ghostWorkout) {calibrationPage();return;}
  if(!pageRendered || detailPage!=displayedDetails || ghostWorkout!=displayedGhostWorkout)pageLayout();
  textPatch(0,160,2,85,19,deviceConnected?"BLE OK":"BLE ...",
            deviceConnected?speedColor:labelColor,panelColor,2);
  textPatch(1,250,2,70,19,String(hrStatus(displayedHrState(millis()))),
            liveHr(millis())?heartColor:labelColor,panelColor,2,true);
  if(!detailPage || ghostWorkout){
    const uint16_t bpm=liveHr(millis());
    textPatch(2,8,49,145,34,bpm?String(bpm)+" BPM":"-- BPM",
              heartColor,backgroundColor,4);
    textPatch(3,164,49,149,34,String(metrics.speed*3.6f,1)+" km/h",
              speedColor,backgroundColor,4);
    textPatch(4,8,166,102,26,formatTime((uint32_t)metrics.seconds()),
              whiteColor,backgroundColor,4);
    textPatch(5,115,166,104,26,String((uint32_t)roundf(metrics.distance()))+" m",
              whiteColor,backgroundColor,4);
    textPatch(6,224,166,95,26,String((uint32_t)roundf(metrics.strokeRate)),
              whiteColor,backgroundColor,4);
    const String paceOrDelta=ghostWorkout ? (ghost.hasBest ?
      String(ghost.meterLead(millis())>=0?"+":"")+String((int)lroundf(ghost.meterLead(millis())))+"m" : "-- m") :
      formatPace(metrics.pace());
    textPatch(7,8,207,102,26,paceOrDelta,
              whiteColor,backgroundColor,4);
    textPatch(8,115,207,104,26,
              "~"+String((int)roundf(metrics.equivalentPower())),
              whiteColor,backgroundColor,4);
    textPatch(9,224,207,95,26,String(metrics.strokes),
              whiteColor,backgroundColor,4);
    if(trendDrawn!=trendRevision) {
      renderSharedTrend(9,91,302,54,ghostWorkout);
      trendDrawn=trendRevision;
    }
  }else{
    String phaseName=motion.phase==cydmotion::Phase::Drive?"NAPED" :
      motion.phase==cydmotion::Phase::Return?"POWROT":"OCZEKIWANIE";
    textPatch(10,9,82,159,26,phaseName,heartColor,backgroundColor,4);
    textPatch(11,178,82,138,26,String(motion.pulses),
              whiteColor,backgroundColor,4);
    textPatch(21,9,138,155,26,String(motion.strokes),
              whiteColor,backgroundColor,4);
    textPatch(22,178,138,138,26,String(motion.transitions),
              whiteColor,backgroundColor,4);
    textPatch(23,13,183,150,30,
              "Skala "+String(speedCalibration,2)+"   Faza z impulsow",
              labelColor,panelColor,2);
  }
}
// Ghost UI: page changes perform one clear; every live field is replaced by
// the existing cropped offscreen sprite renderer. Never redraw the entire
// screen at 2 Hz: that previously caused severe flicker on ST7789/GPIO21.
String raceTime(uint32_t ms) {
  const uint32_t sec=(ms+500)/1000;
  char buf[17];
  snprintf(buf,sizeof(buf),"%lu:%02lu",(unsigned long)(sec/60),
           (unsigned long)(sec%60));
  return String(buf);
}
uint8_t desiredRaceScreen() {
  if(ghost.phase==ghostrace::Phase::Idle) return 0;
  if(ghost.phase==ghostrace::Phase::Finished ||
     ghost.phase==ghostrace::Phase::Cancelled) return 2;
  return 1; // Armed or Running
}
void paintRaceLayout(uint8_t screen) {
  tft.fillScreen(backgroundColor); // only once on view change
  tft.setTextDatum(TL_DATUM);
  tft.fillRect(0,0,320,25,panelColor);
  tft.setTextColor(whiteColor,panelColor);
  tft.drawString("GHOST RACE",9,4,2);
  tft.drawString(FIRMWARE_VERSION,109,8,1);
  tft.setTextColor(labelColor,panelColor);
  tft.drawRightString("SWIPE: WROC",313,6,1);
  if(screen==0) {
    tft.setTextColor(labelColor,backgroundColor);
    tft.drawString("WYBIERZ DYSTANS",11,31,2);
    tft.drawRightString("KALIBRUJ",309,33,2);
    for(uint8_t i=0;i<3;i++) {
      const int yy=53+i*59;
      tft.fillRoundRect(10,yy,300,52,8,panelColor);
      tft.drawRoundRect(10,yy,300,52,8,gridColor);
      tft.setTextColor(whiteColor,panelColor);
      tft.drawString(String(raceDistances[i])+" m",23,yy+8,4);
      tft.setTextColor(labelColor,panelColor);
      const String pb=personalBestMs[i] ? "REKORD "+raceTime(personalBestMs[i]) :
                             "PIERWSZY WYSCIG";
      tft.drawRightString(pb,297,yy+17,2);
    }
    tft.setTextColor(labelColor,backgroundColor);
    tft.drawString("DOTKNIJ 500/1000/2000, SWIPE: WROC",10,230,1);
  } else if(screen==1) {
    tft.setTextColor(whiteColor,backgroundColor);
    tft.drawString("TY",12,33,2);
    tft.setTextColor(speedColor,backgroundColor);
    tft.drawString("DUCH",12,84,2);
    tft.setTextColor(labelColor,backgroundColor);
    tft.drawString("PULS",15,174,1);
    tft.drawString("km/h",174,174,1);
    tft.drawString("DOTKNIJ: MENU / SWIPE: WROC",9,228,1);
    tft.drawRightString(String(ghost.targetMeters)+" m",313,228,1);
  } else {
    tft.setTextColor(ghost.personalBest && !recordSaveFailed ?
                     speedColor : whiteColor,backgroundColor);
    tft.drawString(recordSaveFailed?"BLAD ZAPISU!" :
                   ghost.overflow?"LIMIT 34 MIN" :
                   ghost.phase==ghostrace::Phase::Cancelled?"ANULOWANO" :
                   ghost.personalBest?"NOWY REKORD!":"META!",20,38,4);
    tft.setTextColor(labelColor,backgroundColor);
    tft.drawString("DYSTANS",20,90,2);
    tft.setTextColor(whiteColor,backgroundColor);
    tft.drawString(String(ghost.targetMeters)+" m",20,112,4);
    tft.setTextColor(labelColor,backgroundColor);
    tft.drawString("CZAS",176,90,2);
    tft.setTextColor(whiteColor,backgroundColor);
    tft.drawString(ghost.phase==ghostrace::Phase::Finished ?
      raceTime(ghost.endMs) : "--:--",176,112,4);
    if(ghost.hasBest) {
      tft.setTextColor(labelColor,backgroundColor);
      tft.drawString("POPRZEDNI REKORD",20,164,2);
      tft.setTextColor(powerColor,backgroundColor);
      tft.drawString(raceTime(ghost.best.finishMs),20,184,4);
    } else {
      tft.setTextColor(labelColor,backgroundColor);
      tft.drawString("BRAK POPRZEDNIEGO REKORDU",20,174,2);
    }
    tft.setTextColor(whiteColor,backgroundColor);
    tft.drawString("DOTKNIJ: WYBIERZ DYSTANS",20,226,2);
  }
  resetRenderCache();
  lastRaceSelection=255;
  raceLayoutReady=true;
  displayedRaceScreen=screen;
}
void drawRaceSelection() {
  // BOOT short press can select without a calibrated touch screen.
  if(lastRaceSelection==selectedDistance)return;
  for(int i=0;i<3;i++){
    const int yy=53+i*59;
    tft.drawRoundRect(10,yy,300,52,8,i==selectedDistance?speedColor:gridColor);
    tft.drawRoundRect(11,yy+1,298,50,7,i==selectedDistance?speedColor:panelColor);
  }
  lastRaceSelection=selectedDistance;
}
void paintProgress(int y, float value, uint16_t fillInk) {
  const int x=16,w=288,h=13;
  const float portion=ghost.targetMeters ? constrain(value/ghost.targetMeters,0.0f,1.0f):0;
  const int fillWidth=(int)lroundf(w*portion);
  for(int start=0;start<w;start+=BUFFER_W){
    const int n=min((int)BUFFER_W,w-start);
    if(scratchReady){
      scratch.fillRect(0,0,n,h,faintColor);
      const int local=constrain(fillWidth-start,0,n);
      if(local>0)scratch.fillRect(0,0,local,h,fillInk);
      scratch.pushSprite(x+start,y,0,0,n,h);
    } else {
      tft.fillRect(x+start,y,n,h,faintColor);
      const int local=constrain(fillWidth-start,0,n);
      if(local>0)tft.fillRect(x+start,y,local,h,fillInk);
    }
  }
}
void renderRaceUI() {
  const uint8_t screen=desiredRaceScreen();
  if(!raceLayoutReady||displayedRaceScreen!=screen)
    paintRaceLayout(screen);
  if(screen==0) {
    drawRaceSelection();
    return;
  }
  if(screen==2)return; // result is fully static and drawn once
  const uint32_t now=millis();
  const bool armed=ghost.phase==ghostrace::Phase::Armed;
  textPatch(12,153,29,158,22,
            String((uint32_t)roundf(ghost.meters))+" m",
            whiteColor,backgroundColor,4,true);
  textPatch(13,153,80,158,22,
            ghost.hasBest ?
              String((uint32_t)roundf(ghost.ghostMeters(now)))+" m" : "-- m",
            speedColor,backgroundColor,4,true);
  if(armed) {
    textPatch(14,12,122,157,26,"CZEKAM NA RUCH",
              powerColor,backgroundColor,2);
    textPatch(15,175,122,135,26,"START NA CZUJNIK",
              labelColor,backgroundColor,1);
  } else if(!ghost.hasBest) {
    textPatch(14,12,122,157,26,"PIERWSZY WYSCIG",
              speedColor,backgroundColor,2);
    textPatch(15,175,122,135,26,"ZAPIS REKORDU",
              labelColor,backgroundColor,1);
  } else {
    const float lead=ghost.meterLead(now);
    textPatch(14,12,122,157,26,lead>=0?"PROWADZISZ":"TRACISZ",
              lead>=0?speedColor:powerColor,backgroundColor,2);
    textPatch(15,175,121,135,18,
              String(lead>=0?"+":"")+String(lead,1)+" m",
              lead>=0?speedColor:powerColor,backgroundColor,2,true);
    const float leadSeconds=ghost.timeLeadMs(now)/1000.0f;
    textPatch(20,175,139,135,10,
              String(leadSeconds>=0?"+":"")+String(leadSeconds,1)+" s",
              leadSeconds>=0?speedColor:powerColor,backgroundColor,1,true);
  }
  textPatch(16,12,149,145,25,
            liveHr(millis())?String(liveHr(millis()))+" BPM":"-- BPM",
            heartColor,backgroundColor,4);
  textPatch(17,165,149,148,25,
            String(metrics.speed*3.6f,1)+" km/h",
            speedColor,backgroundColor,4);
  textPatch(18,180,2,83,20,deviceConnected?"BLE OK":"BLE ...",
            deviceConnected?speedColor:labelColor,panelColor,2);
  textPatch(19,267,3,50,20,armed?"--:--":raceTime(ghost.elapsed(now)),
            labelColor,panelColor,2,true);
  paintProgress(56,ghost.meters,powerColor);
  paintProgress(106,ghost.ghostMeters(now),speedColor);
  if(trendDrawn!=trendRevision) {
    renderSharedTrend(15,189,290,28);
    trendDrawn=trendRevision;
  }
}

void initDisplay() {
  // Critical fix from HA_UBOX_bridge: turn on TFT backlight BEFORE initializing
  // anything that can block, then re-enable AFTER TFT init (3.x LEDC API).
  pinMode(BACKLIGHT_PIN, OUTPUT);
  digitalWrite(BACKLIGHT_PIN, HIGH);
  tft.init();
  tft.setRotation(DEFAULT_ROTATION); // 320x240; no software x/y remap
  digitalWrite(BACKLIGHT_PIN, HIGH);
  // Full brightness is deliberate here. Avoid core-2.x vs core-3.x LEDC API
  // differences until manual brightness adjustment is explicitly added.
  digitalWrite(BACKLIGHT_PIN, HIGH);
  backgroundColor = tft.color565(7, 13, 23);
  panelColor = tft.color565(15, 26, 42);
  faintColor = tft.color565(42, 57, 72);
  gridColor = tft.color565(34, 48, 62);
  powerColor = tft.color565(255, 184, 64);
  speedColor = tft.color565(64, 215, 196);
  heartColor = tft.color565(250, 91, 108);
  labelColor = tft.color565(145, 160, 175);
  whiteColor = TFT_WHITE;
  tft.fillScreen(backgroundColor);
  tft.setTextColor(whiteColor, backgroundColor);
  tft.drawString("ArduRower CYD", 16, 20, 4);
  tft.drawString("TFT " + String(tft.width()) + "x" + String(tft.height()), 16, 70, 2);
  tft.drawString("BL GPIO " + String(BACKLIGHT_PIN), 16, 93, 2);
#if defined(ST7789_DRIVER)
  tft.drawString("LCD: ST7789", 16, 116, 2);
#else
  tft.drawString("LCD: ILI9341", 16, 116, 2);
#endif
  tft.drawString(String("FW ") + FIRMWARE_VERSION, 16, 146, 4);
  Serial.printf("CYD LCD: %ux%u, backlight GPIO%d\n",
                (unsigned)tft.width(), (unsigned)tft.height(), BACKLIGHT_PIN);
}

void setup() {
  Serial.begin(115200);
  Serial.printf("\nArduRower CYD v0.09.15 | reset reason=%d | heap=%u\n",
                (int)esp_reset_reason(), (unsigned)ESP.getFreeHeap());
  pinMode(ROWERINPUT, INPUT_PULLUP);
  pinMode(BUTTONSPIN, INPUT_PULLUP);
  initDisplay();
  // Follow the same separate touch SPI and pins as the working UBox CYD code.
  touchSPI.begin(25, 39, 32, 33);
  touchReady = touch.begin(touchSPI);
  touch.setRotation(0);
  if(touchReady)readTouchCalibration();
  // Like HA_UBOX_bridge: calibrate BEFORE BLE starts, never while sending FTMS.
  if(touchReady && !touchCalibrated) {
    const bool ok=calibrateRaceTouch();
    Serial.printf("TOUCH: first-run calibration %s\n",ok?"OK":"FAILED");
  }
  Serial.printf("Touch: %s, IRQ36 calibrated=%d, CS33 SPI=25/39/32/33\n",
                touchReady ? "OK" : "unavailable",touchCalibrated?1:0);
  // Arduino-ESP32 checks btInUse()/bleInUse() BEFORE setup() and must
  // retain BLE memory. Both hooks are strong overrides defined above.
  // btStarted() is diagnostic only; it never starts a second controller.
  const bool controllerAlreadyStarted=btStarted();
  const int controllerStatus=(int)esp_bt_controller_get_status();
  Serial.printf("BT: Arduino btStarted=%d controllerStatus=%d (IDLE=0); retaining BLE memory\n",
                controllerAlreadyStarted?1:0,controllerStatus);
  initBLE();
  loadMotionCalibration();
  // FTMS advertising remains on PUBLIC address. The HR worker uses a
  // distinct static-random local address for separate GATT discovery.
  // Allocate after BLE so the BLE stack keeps its working heap.
  scratch.setColorDepth(16);
  scratchReady = scratch.createSprite(BUFFER_W, BUFFER_H) != nullptr;
  if (!scratchReady) {
    scratch.setColorDepth(8);
    scratchReady = scratch.createSprite(BUFFER_W, BUFFER_H) != nullptr;
  }
  Serial.printf("UI sprite: %s, depth %d, heap %u\n", scratchReady?"OK":"fallback",
                scratch.getColorDepth(), (unsigned)ESP.getFreeHeap());
  resetWorkout();
  loadBestTimes();
  attachInterrupt(digitalPinToInterrupt(ROWERINPUT), rowerdebounceinterrupt, FALLING);
  // Experimental second BLE identity. Keep PUBLIC FTMS unaffected on low heap.
  const uint32_t beforeHr=ESP.getFreeHeap();
  if(beforeHr>40000)
    hrTaskCreated=xTaskCreatePinnedToCore(hrWorker,"hr-recv",4096,
                                         nullptr,1,nullptr,0)==pdPASS;
  if(!hrTaskCreated)
    hrState=(beforeHr<=40000)?HrState::LowMemory:HrState::InitFailed;
  Serial.printf("HR client %s; heap %u, state=%s\n",
                hrTaskCreated?"enabled":"not started",
                (unsigned)beforeHr,hrStatus(hrState));
  updateUI();
}
void loop() {
  uint32_t pulseTime;
  // Keep the original ISR queue semantics and calculation order.
  while (true) {
    portENTER_CRITICAL(&pulseMux);
    const bool available = queueTail != queueHead;
    if (available) {
      pulseTime = pulseQueue[queueTail];
      queueTail = (queueTail + 1) % QUEUE_SIZE;
    } else pulseTime = micros();
    portEXIT_CRITICAL(&pulseMux);
    if (!available) { metrics.tick(pulseTime); break; }
    metrics.pulse(pulseTime);
    motion.onPulse(pulseTime);
    bleDuty.onMotion(millis());
    if (LOG_SENSOR_PULSES) {
      Serial.print("P,"); Serial.print(pulseTime);
      Serial.print(','); Serial.print(metrics.pulses);
      Serial.print(','); Serial.println(metrics.strokes);
    }
  }
  const uint32_t now=millis();
  bleDuty.onFtms(deviceConnected,now);
  checkHrStall(now);
  motion.tick(micros());
  metrics.setExternalTelemetry(motion.meters,motion.speed,
                               motion.strokes,motion.spm);
  updateButton(now);
  updateTouch(now);
  // Calibration always runs before BLE on boot; never block FTMS here.
  if(pendingCalibration)pendingCalibration=false;
  if(activeMode==Mode::Race &&
     (uint32_t)(now-lastGhostMenuHeartbeatMs)>=6000) {
    lastGhostMenuHeartbeatMs=now;
    Serial.printf("GHOST: alive phase=%u selected=%u touch=%lu swipes=%lu FTMS=%d tx=%lu\n",
                  (unsigned)ghost.phase,
                  (unsigned)raceDistances[selectedDistance],
                  (unsigned long)touchEvents,
                  (unsigned long)touchSwipeEvents,
                  deviceConnected?1:0,(unsigned long)ftmsNotifications);
  }
  if (ghost.phase==ghostrace::Phase::Armed && metrics.started) {
    ghost.start(now);
    Serial.printf("GHOST: started %u m at %lu, pulses=%lu\n",
                  (unsigned)ghost.targetMeters,(unsigned long)now,
                  (unsigned long)metrics.pulses);
  }
  if (ghost.phase==ghostrace::Phase::Running) {
    if (ghost.advance(now,metrics.distance())) {
      if (!saveBest(selectedDistance,ghost.attempt))recordSaveFailed=true;
      else personalBestMs[selectedDistance]=ghost.endMs;
      Serial.printf("GHOST: finished %u m, time=%lu, saved=%s\n",
                    (unsigned)ghost.targetMeters,
                    (unsigned long)ghost.endMs,
                    recordSaveFailed?"no":"yes");
    }
  }
  if(!deviceConnected && oldDeviceConnected) {
    Serial.println("FTMS: disconnected; restart advertising");
    pServer->startAdvertising();
  }
  oldDeviceConnected=deviceConnected;
  // Host may interrupt peripheral advertising during dual-role HR scanning.
  // Keep WR S4BL3 discoverable without disconnecting the successful HR link.
  if(!deviceConnected && (uint32_t)(now-lastFtmsAdvertCheckMs)>=5000) {
    lastFtmsAdvertCheckMs=now;
    NimBLEAdvertising *adv=pServer->getAdvertising();
    if(!adv->isAdvertising()) {
      const bool ok=adv->start();
      Serial.printf("FTMS: advertising recovery %s, HR=%s\n",
                    ok?"started":"FAILED",hrStatus(hrState));
    } else if((uint32_t)(now-lastFtmsAdvertReportMs)>=30000) {
      lastFtmsAdvertReportMs=now;
      Serial.printf("FTMS: advertising active; waiting for Garmin, HR=%s tx=%lu\n",
                    hrStatus(hrState),(unsigned long)ftmsNotifications);
    }
  }
  if (deviceConnected && now - lastBleMs >= 1000) {
    lastBleMs = now;
    setCxRowerData();
    ++ftmsNotifications;
  }
  if(now-trendLastMs>=TREND_SAMPLE_MS) {
    trendLastMs=now;
    recordTrend(now);
  }
  if (now - lastHistoryMs >= HISTORY_INTERVAL_MS) {
    lastHistoryMs = now;
    recordHistory();
  }
  if (now - lastDisplayMs >= DISPLAY_INTERVAL_MS) {
    lastDisplayMs = now;
    updateUI();
  }
  delay(1);
}
