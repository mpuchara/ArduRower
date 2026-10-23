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
SPIClass touchSPI(HSPI);
XPT2046_Touchscreen touch(33, 36);
bool touchReady = false;
bool touchedLast = false;
uint32_t touchPressedAt = 0;
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
}

void updateButton(uint32_t now) {
  const bool down = digitalRead(BUTTONSPIN) == LOW;
  if (down != rawButtonDown) { rawButtonDown = down; buttonChangedMs = now; }
  if (now - buttonChangedMs >= 30 && down != buttonDown) {
    buttonDown = down;
    if (down) { buttonPressedMs = now; longPressDone = false; }
    else if (!longPressDone) detailPage = !detailPage;
  }
  if (buttonDown && !longPressDone && now - buttonPressedMs >= 1500) {
    resetWorkout();
    longPressDone = true;
  }
}
void updateTouch(uint32_t now) {
  if (!touchReady) return;
  const bool down = touch.touched();
  if (down && !touchedLast) touchPressedAt = now;
  if (!down && touchedLast && now - touchPressedAt >= 50 && now - touchPressedAt < 850)
    detailPage = !detailPage; // tap anywhere; no coordinate/calibration required
  touchedLast = down;
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

void drawSeries(int x, int y, int w, int h, bool power) {
  const uint16_t ink = power ? powerColor : speedColor;
  tft.drawFastHLine(x, y + h - 1, w, gridColor);
  tft.drawFastHLine(x, y + h / 2, w, gridColor);
  if (historyCount == 0) return;
  const float top = historyMax(power);
  int priorX = 0, priorY = 0;
  // Horizontal scale is fixed at 120 s: incoming samples enter on the right,
  // and shorter sessions occupy the right edge (no artificial stretching).
  for (uint8_t i = 0; i < historyCount; ++i) {
    const int px = x + w - 1 - ((historyCount - 1 - i) * (w - 1) / (HISTORY_LENGTH - 1));
    const float value = constrain(sampleValue(i, power), 0.0f, top);
    const int py = y + h - 1 - (int)roundf(value / top * (h - 1));
    if (i > 0) tft.drawLine(priorX, priorY, px, py, ink);
    else tft.fillCircle(px, py, 1, ink);
    priorX = px;
    priorY = py;
  }
}
void drawHeader() {
  tft.fillRect(0, 0, 320, 25, panelColor);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(whiteColor, panelColor);
  tft.drawString("ARDUROWER", 8, 4, 2);
  tft.setTextColor(deviceConnected ? speedColor : labelColor, panelColor);
  tft.drawString(deviceConnected ? "BLE OK" : "BLE ...", 158, 4, 2);
  if (metrics.paused) {
    tft.setTextColor(powerColor, panelColor);
    tft.drawRightString("PAUZA", 313, 4, 2);
  } else {
    tft.setTextColor(labelColor, panelColor);
    tft.drawRightString(formatTime((uint32_t)metrics.seconds()), 313, 4, 2);
  }
}
void drawCard(int x, bool power) {
  const int y = 32, w = 149, h = 123;
  tft.fillRoundRect(x, y, w, h, 6, panelColor);
  tft.drawRoundRect(x, y, w, h, 6, gridColor);
  const uint16_t ink = power ? powerColor : speedColor;
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(labelColor, panelColor);
  tft.drawString(power ? "MOC SZACOWANA" : "PREDKOSC", x + 9, y + 6, 2);
  const float value = power ? metrics.equivalentPower() : metrics.speed * 3.6f;
  const String shown = power ? "~" + String((int)roundf(value)) : String(value, 1);
  tft.setTextColor(ink, panelColor);
  tft.drawString(shown, x + 8, y + 29, 4);
  tft.setTextColor(labelColor, panelColor);
  tft.drawRightString(power ? "W" : "km/h", x + w - 8, y + 38, 2);
  tft.drawString(power ? "W / 120 s" : "km/h / 120 s", x + 9, y + 62, 2);
  drawSeries(x + 10, y + 81, w - 20, 32, power);
}
void drawStat(int x, int y, const String &label, const String &value) {
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(labelColor, backgroundColor);
  tft.drawString(label, x, y, 2);
  tft.setTextColor(whiteColor, backgroundColor);
  tft.drawString(value, x, y + 15, 4);
}
void drawDashboard() {
  tft.fillScreen(backgroundColor);
  drawHeader();
  drawCard(8, true);
  drawCard(163, false);
  drawStat(8, 159, "CZAS", formatTime((uint32_t)metrics.seconds()));
  drawStat(115, 159, "DYSTANS", String((uint32_t)metrics.distance()) + " m");
  drawStat(225, 159, "SPM~", String((uint32_t)roundf(metrics.strokeRate)));
  drawStat(8, 198, "TEMPO / 500m", formatPace(metrics.pace()));
  drawStat(115, 198, "SR. ~W", String((uint32_t)roundf(metrics.averageEquivalentPower())));
  drawStat(225, 198, "RUCHY~", String(metrics.strokes));
  if (droppedPulses) {
    tft.fillRect(290, 225, 23, 14, TFT_RED);
    tft.setTextColor(TFT_WHITE, TFT_RED);
    tft.drawString("!", 298, 224, 2);
  }
}
void drawHistoryPage() {
  tft.fillScreen(backgroundColor);
  drawHeader();
  tft.setTextColor(powerColor, backgroundColor);
  tft.drawString("MOC ~W", 8, 29, 2);
  tft.setTextColor(labelColor, backgroundColor);
  tft.drawRightString("MAX " + String((int)roundf(historyMax(true))) + " W",
                      310, 29, 2);
  drawSeries(9, 53, 302, 54, true);
  tft.setTextColor(speedColor, backgroundColor);
  tft.drawString("PREDKOSC km/h", 8, 119, 2);
  tft.setTextColor(labelColor, backgroundColor);
  tft.drawRightString("MAX " + String(historyMax(false), 1) + " km/h",
                      310, 119, 2);
  drawSeries(9, 143, 302, 54, false);
  tft.setTextColor(labelColor, backgroundColor);
  tft.drawString("OSTATNIE 120 s", 8, 207, 2);
  tft.drawRightString("Dotknij, by wrocic", 311, 207, 2);
  tft.drawString("BOOT: krotko = widok; >1.5 s = reset", 8, 225, 1);
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
  Serial.printf("Touch: %s\n", touchReady ? "OK" : "unavailable");
  initBLE();
  resetWorkout();
  attachInterrupt(digitalPinToInterrupt(ROWERINPUT), rowerdebounceinterrupt, FALLING);
  drawDashboard();
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
    if (detailPage) drawHistoryPage();
    else drawDashboard();
  }
  delay(1);
}
