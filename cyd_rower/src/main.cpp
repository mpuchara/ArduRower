// ArduRower for ESP32-2432S028 CYD.
// TFT configuration, rotation and backlight follow HA_UBOX_bridge/cyd_monitor.
// Workout calculation is shared directly with original ArduRower v0.07.
// BLE FTMS wire contract follows original ArduRower.ino and must stay unchanged.
#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLEAdvertising.h>
#include <BLE2902.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include <XPT2046_Touchscreen.h>
#include <math.h>
#include <Preferences.h>
#include "GhostRace.h"
#include "../../ArduRower/RowerMetrics.h"

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
XPT2046_Touchscreen touch(33, 36);
bool touchReady = false;
bool touchedLast = false;
uint32_t touchPressedAt = 0;
int16_t rawStartX = 0, rawStartY = 0, rawEndX = 0, rawEndY = 0;
float touchA=0, touchB=0, touchC=0, touchD=0, touchE=0, touchF=0;
bool touchCalibrated=false, pendingCalibration=false;
uint8_t touchRotation=DEFAULT_ROTATION;

RowerMetrics metrics;
BLEServer *pServer = nullptr;
BLECharacteristic *pCtrCharacteristic = nullptr, *pDtCharacteristic = nullptr;
BLECharacteristic *pFmfCharacteristic = nullptr, *pStCharacteristic = nullptr;
BLECharacteristic *pBatCharacteristic = nullptr;
volatile bool deviceConnected = false;
bool oldDeviceConnected = false;

portMUX_TYPE pulseMux = portMUX_INITIALIZER_UNLOCKED;
volatile uint32_t pulseQueue[QUEUE_SIZE];
volatile uint16_t queueHead = 0, queueTail = 0;
volatile uint32_t lastEdgeUs = 0, droppedPulses = 0;
volatile bool haveEdge = false;
uint32_t lastDisplayMs = 0, lastBleMs = 0, lastHistoryMs = 0;
bool detailPage = false, buttonDown = false, longPressDone = false;
bool rawButtonDown = false;
uint32_t buttonChangedMs = 0, buttonPressedMs = 0;

struct HistorySample { float power; float speedKmh; };
HistorySample history[HISTORY_LENGTH] = {};
uint8_t historyHead = 0;
uint8_t historyCount = 0;
uint16_t backgroundColor, panelColor, faintColor, gridColor;
uint16_t powerColor, speedColor, labelColor, whiteColor;
uint32_t historyRevision = 0, plottedRevision = UINT32_MAX;
bool pageRendered = false, displayedDetails = false;
String previousField[12];
bool previousValid[24] = {};
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

class MyServerCallbacks : public BLEServerCallbacks {
 public:
  void onConnect(BLEServer *) override { deviceConnected = true; }
  void onDisconnect(BLEServer *) override { deviceConnected = false; }
};
// Intentionally no-op: as in the existing firmware, FTMS Control Point procedures
// are NOT implemented. Preserving the existing Connect IQ integration is paramount.
class MyCallbacks : public BLECharacteristicCallbacks {
 public:
  void onWrite(BLECharacteristic *) override {}
};

void initBLE() {
  Serial.print(F("Init BLE:"));
  BLEDevice::init(BLE_SERVICE_NAME);
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  BLEService *pService =
      pServer->createService(BLEUUID((uint16_t)FitnessMachineService));
  pDtCharacteristic = pService->createCharacteristic(
      BLEUUID((uint16_t)FitnessMachineRowerData),
      BLECharacteristic::PROPERTY_NOTIFY);
  pDtCharacteristic->addDescriptor(new BLE2902());
  pDtCharacteristic->setCallbacks(new MyCallbacks());
  pFmfCharacteristic = pService->createCharacteristic(
      BLEUUID((uint16_t)FitnessMachineFeature),
      BLECharacteristic::PROPERTY_READ);
  pFmfCharacteristic->addDescriptor(new BLE2902());
  pCtrCharacteristic = pService->createCharacteristic(
      BLEUUID((uint16_t)FitnessMachineControlPoint),
      BLECharacteristic::PROPERTY_WRITE);
  pCtrCharacteristic->addDescriptor(new BLE2902());
  pCtrCharacteristic->setCallbacks(new MyCallbacks());
  BLEService *pBatteryService = pServer->createService(BLEUUID((uint16_t)0x180F));
  pBatCharacteristic = pBatteryService->createCharacteristic(
      BLEUUID((uint16_t)batteryLevel),
      BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
  pBatCharacteristic->addDescriptor(new BLE2902());
  pStCharacteristic = pService->createCharacteristic(
      BLEUUID((uint16_t)FitnessMachineStatus),
      BLECharacteristic::PROPERTY_NOTIFY);
  pStCharacteristic->addDescriptor(new BLE2902());
  pService->start();
  pBatteryService->start();
  BLEService *pService2 = pServer->createService(BLEUUID((uint16_t)DEVICE_INFORMATION));
  BLECharacteristic *pCharacteristic24 = pService2->createCharacteristic(
      (uint16_t)0x2A24, BLECharacteristic::PROPERTY_READ);
  BLECharacteristic *pCharacteristic25 = pService2->createCharacteristic(
      (uint16_t)0x2A25, BLECharacteristic::PROPERTY_READ);
  BLECharacteristic *pCharacteristic26 = pService2->createCharacteristic(
      (uint16_t)0x2A26, BLECharacteristic::PROPERTY_READ);
  BLECharacteristic *pCharacteristic27 = pService2->createCharacteristic(
      (uint16_t)0x2A27, BLECharacteristic::PROPERTY_READ);
  BLECharacteristic *pCharacteristic28 = pService2->createCharacteristic(
      (uint16_t)0x2A28, BLECharacteristic::PROPERTY_READ);
  BLECharacteristic *pCharacteristic29 = pService2->createCharacteristic(
      (uint16_t)0x2A29, BLECharacteristic::PROPERTY_READ);
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
  BLEAdvertising *pAdvertising = pServer->getAdvertising();
  uint8_t advData[] = {0x02, 0x01, 0x06, 0x05, 0x02, 0x26, 0x18, 0x0a, 0x18};
  String advDataStr;
  for (unsigned int i = 0; i < sizeof(advData); i++)
    advDataStr += (char)advData[i];
  BLEAdvertisementData oAdvertisementData;
  oAdvertisementData.addData(advDataStr);
  pAdvertising->setScanResponse(true);
  pAdvertising->setAdvertisementData(oAdvertisementData);
  pAdvertising->addServiceUUID(BLEUUID((uint16_t)FitnessMachineService));
  BLEDevice::startAdvertising();
  Serial.println();
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
  return read == sizeof(out) && ghostrace::valid(out, raceDistances[index]);
}
bool saveBest(uint8_t index, const ghostrace::Record &record) {
  if (!ghostrace::valid(record, raceDistances[index])) return false;
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
  ghost.arm(raceDistances[index], found ? &storedGhost : nullptr);
  // Start a fresh interval, so race distance is not polluted by earlier
  // dashboard rowing. Do not change the BLE/FTMS encoder in any way.
  resetWorkout();
  activeMode = Mode::Race;
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
    activeMode=Mode::Race;
    if (ghost.phase == ghostrace::Phase::Finished ||
        ghost.phase == ghostrace::Phase::Cancelled)
      openRaceMenu();
    else if (ghost.phase == ghostrace::Phase::Idle)
      loadBestTimes();
    raceLayoutReady=false;
    if (touchReady && !touchCalibrated) pendingCalibration=true;
  } else {
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
      if (activeMode == Mode::Dashboard) detailPage=!detailPage;
      else if (ghost.phase == ghostrace::Phase::Idle)
        selectedDistance=(selectedDistance+1)%3;
      else if (ghost.phase == ghostrace::Phase::Finished ||
               ghost.phase == ghostrace::Phase::Cancelled) openRaceMenu();
    }
  }
  if (buttonDown && !longPressDone && now-buttonPressedMs>=1500) {
    longPressDone=true;
    if (activeMode == Mode::Dashboard) {
      if(ghost.phase==ghostrace::Phase::Running ||
         ghost.phase==ghostrace::Phase::Armed)ghost.cancel();
      resetWorkout();
    }
    else if (ghost.phase == ghostrace::Phase::Idle) startRace(selectedDistance);
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
    if (touchCalibrated) break;
  }
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
    p.putBool("tc_ok",true);p.putUChar("tc_rot",DEFAULT_ROTATION);
    p.putFloat("tc_a",touchA);p.putFloat("tc_b",touchB);
    p.putFloat("tc_c",touchC);p.putFloat("tc_d",touchD);
    p.putFloat("tc_e",touchE);p.putFloat("tc_f",touchF);
    p.end();
  }
  return true;
}
void touchTap(int16_t rx,int16_t ry) {
  if(activeMode==Mode::Dashboard){detailPage=!detailPage;return;}
  if(ghost.phase==ghostrace::Phase::Finished ||
     ghost.phase==ghostrace::Phase::Cancelled){openRaceMenu();return;}
  if(ghost.phase!=ghostrace::Phase::Idle)return;
  int16_t x,y;
  if(!mappedTouch(rx,ry,x,y)) {pendingCalibration=true;return;}
  // Three entire rows are large touch targets with no small hitbox.
  if(y>=53 && y<105) startRace(0);
  else if(y>=112 && y<164) startRace(1);
  else if(y>=171 && y<223) startRace(2);
}
void updateTouch(uint32_t now) {
  if(!touchReady)return;
  const bool down=touch.touched();
  if(down) {
    const TS_Point pt=touch.getPoint();
    if(!touchedLast){
      touchedLast=true;touchPressedAt=now;
      rawStartX=rawEndX=pt.x;
      rawStartY=rawEndY=pt.y;
    } else {
      rawEndX=pt.x;rawEndY=pt.y;
    }
  } else if(touchedLast) {
    touchedLast=false;
    const uint32_t held=now-touchPressedAt;
    const int dx=abs(rawEndX-rawStartX),dy=abs(rawEndY-rawStartY);
    // Touch is on a separate SPI and may swap raw axes with screen rotation.
    // Both physical horizontal directions enter the same other main mode,
    // hence axis polarity is irrelevant. Short taps never trigger navigation.
    if(held>75 && held<1750 && max(dx,dy)>700 &&
       max(dx,dy)>min(dx,dy)*1.6f) {
      toggleMode();
    } else if(held>=60 && held<750 && max(dx,dy)<500) {
      touchTap(rawEndX,rawEndY);
    } else if(held>=1200 && max(dx,dy)<500 &&
              activeMode==Mode::Race &&
              (ghost.phase==ghostrace::Phase::Running ||
               ghost.phase==ghostrace::Phase::Armed)) {
      int16_t x,y;
      if(mappedTouch(rawEndX,rawEndY,x,y) && y>=218)openRaceMenu();
    }
  }
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
void recordHistory() {
  history[historyHead] = {metrics.equivalentPower(), metrics.speed * 3.6f};
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
  float result = power ? 10.0f : 2.0f;
  for (uint8_t i = 0; i < historyCount; ++i)
    result = max(result, sampleValue(i, power));
  return result * 1.12f;
}

// ST7789 refresh: build changing regions off-screen and crop-push them to TFT.
// fillScreen() is used only when switching pages, NEVER on the 500-ms tick.
static constexpr int16_t BUFFER_W = 160, BUFFER_H = 55;
void resetRenderCache() {
  for (uint8_t i = 0; i < 24; i++) previousValid[i] = false;
  plottedRevision = UINT32_MAX;
}
void textPatch(uint8_t slot, int x, int y, int w, int h, String text,
               uint16_t fg, uint16_t bg, uint8_t font, bool right = false) {
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
void pageLayout() {
  tft.fillScreen(backgroundColor);  // only at startup or on explicit view change
  tft.fillRect(0,0,320,25,panelColor);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(whiteColor,panelColor);
  tft.drawString("ARDUROWER",8,4,2);
  if (!detailPage) {
    for (int i=0;i<2;i++) {
      const int x=i?163:8, y=32, w=149, h=123;
      tft.fillRoundRect(x,y,w,h,6,panelColor);
      tft.drawRoundRect(x,y,w,h,6,gridColor);
      tft.setTextColor(labelColor,panelColor);
      tft.drawString(i?"PREDKOSC":"MOC SZACOWANA",x+9,y+6,2);
      tft.drawString(i?"km/h / 120 s":"W / 120 s",x+9,y+62,2);
    }
    const int pos[3]={8,115,225};
    const char* names[6]={"CZAS","DYSTANS","SPM~","TEMPO / 500m","SR. ~W","RUCHY~"};
    tft.setTextColor(labelColor,backgroundColor);
    for (int i=0;i<6;i++) tft.drawString(names[i],pos[i%3],i<3?156:198,2);
  } else {
    tft.setTextColor(powerColor,backgroundColor);
    tft.drawString("MOC ~W",8,29,2);
    tft.setTextColor(speedColor,backgroundColor);
    tft.drawString("PREDKOSC km/h",8,119,2);
    tft.setTextColor(labelColor,backgroundColor);
    tft.drawString("OSTATNIE 120 s",8,207,2);
    tft.drawRightString("Dotknij, by wrocic",311,207,2);
    tft.drawString("BOOT: krotko = widok; >1.5 s = reset",8,225,1);
  }
  pageRendered=true; displayedDetails=detailPage;
  resetRenderCache();
}
void updateUI() {
  if (activeMode == Mode::Race) { renderRaceUI(); return; }
  if (!pageRendered || detailPage != displayedDetails) pageLayout();
  textPatch(0,155,3,85,20,deviceConnected?"BLE OK":"BLE ...",
            deviceConnected?speedColor:labelColor,panelColor,2);
  textPatch(1,244,3,76,20,metrics.paused?"PAUZA":formatTime((uint32_t)metrics.seconds()),
            metrics.paused?powerColor:labelColor,panelColor,2,true);
  if (!detailPage) {
    cardValue(2,8,true);
    cardValue(3,163,false);
    textPatch(4,8,172,99,26,formatTime((uint32_t)metrics.seconds()),
              whiteColor,backgroundColor,4);
    textPatch(5,115,172,105,26,String((uint32_t)metrics.distance())+" m",
              whiteColor,backgroundColor,4);
    textPatch(6,225,172,92,26,String((uint32_t)roundf(metrics.strokeRate)),
              whiteColor,backgroundColor,4);
    textPatch(7,8,214,99,26,formatPace(metrics.pace()),
              whiteColor,backgroundColor,4);
    textPatch(8,115,214,105,26,String((uint32_t)roundf(metrics.averageEquivalentPower())),
              whiteColor,backgroundColor,4);
    textPatch(9,225,214,92,26,String(metrics.strokes),
              whiteColor,backgroundColor,4);
  } else {
    textPatch(10,176,29,143,18,"MAX "+String((int)roundf(historyMax(true)))+" W",
              labelColor,backgroundColor,2,true);
    textPatch(11,165,119,154,18,"MAX "+String(historyMax(false),1)+" km/h",
              labelColor,backgroundColor,2,true);
  }
  if (plottedRevision != historyRevision) {
    if (detailPage) {
      plot(9,53,302,54,true);
      plot(9,143,302,54,false);
    } else {
      plot(18,113,129,32,true);
      plot(173,113,129,32,false);
    }
    plottedRevision=historyRevision;
  }
  if (!detailPage && droppedPulses) {
    tft.fillRect(290,225,23,14,TFT_RED);
    tft.setTextColor(TFT_WHITE,TFT_RED);
    tft.drawString("!",298,224,2);
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
  tft.setTextColor(labelColor,panelColor);
  tft.drawRightString("SWIPE: DASHBOARD",313,6,1);
  if(screen==0) {
    tft.setTextColor(labelColor,backgroundColor);
    tft.drawString("WYBIERZ DYSTANS",11,31,2);
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
    tft.drawString("DOTKNIJ DYSTANS, BY ZACZAC",10,230,1);
  } else if(screen==1) {
    tft.setTextColor(whiteColor,backgroundColor);
    tft.drawString("TY",12,33,2);
    tft.setTextColor(speedColor,backgroundColor);
    tft.drawString("DUCH",12,84,2);
    tft.setTextColor(labelColor,backgroundColor);
    tft.drawString("MOC ~W",15,174,1);
    tft.drawString("km/h",174,174,1);
    tft.drawString("BOOT 1.5s: anuluj",9,228,1);
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
            "~"+String((int)roundf(metrics.equivalentPower()))+" W",
            powerColor,backgroundColor,4);
  textPatch(17,165,149,148,25,
            String(metrics.speed*3.6f,1)+" km/h",
            speedColor,backgroundColor,4);
  textPatch(18,180,2,83,20,deviceConnected?"BLE OK":"BLE ...",
            deviceConnected?speedColor:labelColor,panelColor,2);
  textPatch(19,267,3,50,20,armed?"--:--":raceTime(ghost.elapsed(now)),
            labelColor,panelColor,2,true);
  paintProgress(56,ghost.meters,powerColor);
  paintProgress(106,ghost.ghostMeters(now),speedColor);
  if(plottedRevision!=historyRevision) {
    plot(15,189,132,28,true);
    plot(173,189,132,28,false);
    plottedRevision=historyRevision;
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
  Serial.printf("CYD LCD: %ux%u, backlight GPIO%d\n",
                (unsigned)tft.width(), (unsigned)tft.height(), BACKLIGHT_PIN);
}

void setup() {
  Serial.begin(115200);
  pinMode(ROWERINPUT, INPUT_PULLUP);
  pinMode(BUTTONSPIN, INPUT_PULLUP);
  initDisplay();
  // Follow the same separate touch SPI and pins as the working UBox CYD code.
  touchSPI.begin(25, 39, 32, 33);
  touchReady = touch.begin(touchSPI);
  touch.setRotation(0);
  if(touchReady)readTouchCalibration();
  Serial.printf("Touch: %s\n", touchReady ? "OK" : "unavailable");
  initBLE();
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
    if (LOG_SENSOR_PULSES) {
      Serial.print("P,"); Serial.print(pulseTime);
      Serial.print(','); Serial.print(metrics.pulses);
      Serial.print(','); Serial.println(metrics.strokes);
    }
  }
  const uint32_t now = millis();
  updateButton(now);
  updateTouch(now);
  if (pendingCalibration && activeMode==Mode::Race &&
      touchReady && !touchCalibrated) {
    pendingCalibration=false;
    if(!calibrateRaceTouch())Serial.println("Touch calibration unavailable: BOOT menu works");
    raceLayoutReady=false;
    pageRendered=false;
    touchedLast=false;
  }
  if (ghost.phase==ghostrace::Phase::Armed && metrics.started)
    ghost.start(now);
  if (ghost.phase==ghostrace::Phase::Running) {
    if (ghost.advance(now,metrics.distance())) {
      if (!saveBest(selectedDistance,ghost.attempt))recordSaveFailed=true;
      else personalBestMs[selectedDistance]=ghost.endMs;
    }
  }
  if (!deviceConnected && oldDeviceConnected) pServer->startAdvertising();
  oldDeviceConnected = deviceConnected;
  if (deviceConnected && now - lastBleMs >= 1000) {
    lastBleMs = now;
    setCxRowerData();
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
