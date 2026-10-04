#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLEAdvertising.h>
#include <BLE2902.h>
#include "RunningAverage.h"
#include "SSD1306.h"
#include "RowerMetrics.h"

#define SDA 4
#define SCL 15
#define _VERSION 0.06
#define BLE_SERVICE_NAME "WR S4BL3"
#define SerialDebug Serial
#define BATPIN 33
#define MinADC 1095
#define MaxADC 1437
#define FitnessMachineService 0x1826
#define FitnessMachineRowerData 0x2AD1
#define FitnessMachineFeature 0x2ACC
#define FitnessMachineControlPoint 0x2AD9
#define FitnessMachineStatus 0x2ADA
#define batteryLevel 0x2A19
#define DEVICE_INFORMATION 0x180A

const int ROWERINPUT = 2;
const int BUTTONSPIN = 0;
// Enable only if your app requires estimated Concept2-equivalent watts.
const bool SEND_EQUIVALENT_POWER = false;
// Set true to capture 20-30 complete pull/recovery cycles in Serial Monitor (115200).
const bool LOG_SENSOR_PULSES = false;
SSD1306 display(0x3c, SDA, SCL, 16, GEOMETRY_128_64);
RunningAverage battery_RA(40);
RowerMetrics metrics;
BLEServer *pServer = nullptr;
BLECharacteristic *pCtrCharacteristic, *pDtCharacteristic, *pFmfCharacteristic;
BLECharacteristic *pStCharacteristic, *pBatCharacteristic;
volatile bool deviceConnected = false;
bool oldDeviceConnected = false;

// ISR only timestamps edges. All floating-point, BLE and display work stays in loop().
portMUX_TYPE pulseMux = portMUX_INITIALIZER_UNLOCKED;
const uint16_t QUEUE_SIZE = 256;
volatile uint32_t pulseQueue[QUEUE_SIZE];
volatile uint16_t queueHead = 0, queueTail = 0;
volatile uint32_t lastEdgeUs = 0, droppedPulses = 0;
volatile bool haveEdge = false;
uint32_t lastDisplayMs = 0, lastBleMs = 0, lastBatteryMs = 0;
bool detailPage = false, buttonDown = false, longPressDone = false;
bool rawButtonDown = false;
uint32_t buttonChangedMs = 0, buttonPressedMs = 0;

class MyServerCallbacks: public BLEServerCallbacks {
  void onConnect(BLEServer*) { deviceConnected = true; }
  void onDisconnect(BLEServer*) { deviceConnected = false; }
};
// Kept for legacy clients. FTMS control procedures are not implemented.
class MyCallbacks: public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic*) {}
};

void initBLE() {
  SerialDebug.print(F("Init BLE:"));
  BLEDevice::init(BLE_SERVICE_NAME);

  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  BLEService *pService = pServer->createService(BLEUUID((uint16_t)FitnessMachineService));

  pDtCharacteristic = pService->createCharacteristic(
    BLEUUID((uint16_t)FitnessMachineRowerData),
    BLECharacteristic::PROPERTY_NOTIFY
  );
  pDtCharacteristic->addDescriptor(new BLE2902());
  pDtCharacteristic->setCallbacks(new MyCallbacks());

  pFmfCharacteristic = pService->createCharacteristic(
    BLEUUID((uint16_t)FitnessMachineFeature),
    BLECharacteristic::PROPERTY_READ
  );
  pFmfCharacteristic->addDescriptor(new BLE2902());

  pCtrCharacteristic = pService->createCharacteristic(
    BLEUUID((uint16_t)FitnessMachineControlPoint),
    BLECharacteristic::PROPERTY_WRITE
  );
  pCtrCharacteristic->addDescriptor(new BLE2902());
  pCtrCharacteristic->setCallbacks(new MyCallbacks());

  BLEService *pBatteryService = pServer->createService(BLEUUID((uint16_t)0x180F));
  pBatCharacteristic = pBatteryService->createCharacteristic(
    BLEUUID((uint16_t)batteryLevel),
    BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
  );
  pBatCharacteristic->addDescriptor(new BLE2902());

  pStCharacteristic = pService->createCharacteristic(
    BLEUUID((uint16_t)FitnessMachineStatus),
    BLECharacteristic::PROPERTY_NOTIFY
  );
  pStCharacteristic->addDescriptor(new BLE2902());

  pService->start();
  pBatteryService->start();

  BLEService *pService2 = pServer->createService(BLEUUID((uint16_t)DEVICE_INFORMATION));
  BLECharacteristic *pCharacteristic24 = pService2->createCharacteristic((uint16_t)0x2A24, BLECharacteristic::PROPERTY_READ);
  BLECharacteristic *pCharacteristic25 = pService2->createCharacteristic((uint16_t)0x2A25, BLECharacteristic::PROPERTY_READ);
  BLECharacteristic *pCharacteristic26 = pService2->createCharacteristic((uint16_t)0x2A26, BLECharacteristic::PROPERTY_READ);    
  BLECharacteristic *pCharacteristic27 = pService2->createCharacteristic((uint16_t)0x2A27, BLECharacteristic::PROPERTY_READ);
  BLECharacteristic *pCharacteristic28 = pService2->createCharacteristic((uint16_t)0x2A28, BLECharacteristic::PROPERTY_READ);
  BLECharacteristic *pCharacteristic29 = pService2->createCharacteristic((uint16_t)0x2A29, BLECharacteristic::PROPERTY_READ);
  pService2->start();

  pCharacteristic24->setValue("4");
  pCharacteristic25->setValue("0000");
  pCharacteristic26->setValue("0.06");
  pCharacteristic27->setValue("2.2BLE");
  pCharacteristic28->setValue("4.3");
  pCharacteristic29->setValue("Waterrower");

  char cRower[8];
  cRower[0]=0x26;
  cRower[1]=SEND_EQUIVALENT_POWER ? 0x50 : 0x10;
  cRower[2]=0x00;
  cRower[3]=0x00;
  cRower[4]=0x00;
  cRower[5]=0x00;
  cRower[6]=0x00;
  cRower[7]=0x00;
  pFmfCharacteristic->setValue((uint8_t* )cRower, 8);

  BLEAdvertising *pAdvertising = pServer->getAdvertising();
  // Tworzymy String z danych reklamowych
  uint8_t advData[] = {0x02,0x01,0x06,0x05,0x02,0x26,0x18,0x0a,0x18};
  String advDataStr;
  for (unsigned int i = 0; i < sizeof(advData); i++) {
    advDataStr += (char)advData[i];
  }
  BLEAdvertisementData oAdvertisementData;
  oAdvertisementData.addData(advDataStr);
  pAdvertising->setScanResponse(true);
  pAdvertising->setAdvertisementData(oAdvertisementData);

  pAdvertising->addServiceUUID(BLEUUID((uint16_t)FitnessMachineService));
  BLEDevice::startAdvertising();

  SerialDebug.println();
}


void IRAM_ATTR rowerdebounceinterrupt() {
  const uint32_t now = micros();
  portENTER_CRITICAL_ISR(&pulseMux);
  if (!haveEdge || (uint32_t)(now - lastEdgeUs) >= SENSOR_DEBOUNCE_US) {
    haveEdge = true; lastEdgeUs = now;
    const uint16_t next = (queueHead + 1) % QUEUE_SIZE;
    if (next != queueTail) {
      pulseQueue[queueHead] = now; queueHead = next;
    } else ++droppedPulses;
  }
  portEXIT_CRITICAL_ISR(&pulseMux);
}

void resetWorkout() {
  portENTER_CRITICAL(&pulseMux);
  queueHead = queueTail = 0; haveEdge = false; droppedPulses = 0;
  metrics.reset(micros());
  portEXIT_CRITICAL(&pulseMux);
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
    resetWorkout(); longPressDone = true;
  }
}

uint16_t bounded16(float value) {
  if (!isfinite(value) || value <= 0) return 0;
  return value >= 65535 ? 65535 : (uint16_t)roundf(value);
}
uint8_t encodedStrokeRate(float rate) {
  return (uint8_t)min((uint16_t)255, bounded16(rate * 2.0f));
}
void put16(uint8_t *data, uint8_t &offset, uint16_t value) {
  data[offset++] = value & 0xff; data[offset++] = value >> 8;
}

void setCxRowerData() {
  // Single record <= 20 bytes, including elapsed time. No fictitious HR/energy.
  uint8_t data[19]; uint8_t offset = 0;
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
  pDtCharacteristic->setValue(data, offset); pDtCharacteristic->notify();
  uint8_t battery = (uint8_t)roundf(battery_RA.getAverage() * 100 / 127);
  pBatCharacteristic->setValue(&battery, 1); pBatCharacteristic->notify();
}

String formatTime(uint32_t seconds) {
  char text[16];
  if (seconds >= 3600) snprintf(text, sizeof(text), "%lu:%02lu:%02lu",
    (unsigned long)(seconds / 3600), (unsigned long)(seconds / 60 % 60), (unsigned long)(seconds % 60));
  else snprintf(text, sizeof(text), "%02lu:%02lu", (unsigned long)(seconds / 60), (unsigned long)(seconds % 60));
  return String(text);
}
void fittedText(int x, int y, int maxWidth, String text, const uint8_t *font) {
  display.setFont(font);
  if (display.getStringWidth(text) > maxWidth) display.setFont(ArialMT_Plain_16);
  if (display.getStringWidth(text) > maxWidth) display.setFont(ArialMT_Plain_10);
  display.drawString(x, y, text);
}
void drawDashboard() {
  display.clear(); display.setColor(WHITE); display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_10);
  // Show only an established client connection, never mere advertising.
  // A single FTMS link does not identify the client's watch model.
  if (deviceConnected) display.drawString(0, 0, "BLE");
  if (metrics.paused) display.drawString(28, 0, "PAUZA");
  display.setTextAlignment(TEXT_ALIGN_RIGHT);
  display.drawString(128, 0, "~" + String((int)roundf(metrics.equivalentPower())) + " W");
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.drawLine(0, 12, 127, 12);

  if (!detailPage) {
    fittedText(0, 13, 96, String(metrics.speed * 3.6f, 1), ArialMT_Plain_24);
    display.setFont(ArialMT_Plain_10); display.drawString(100, 23, "km/h");
    display.drawLine(0, 40, 127, 40);
    display.drawString(0, 41, "CZAS"); display.drawString(53, 41, "METRY");
    display.drawString(100, 41, "SPM~");
    fittedText(0, 51, 50, formatTime((uint32_t)metrics.seconds()), ArialMT_Plain_10);
    fittedText(53, 48, 44, String((uint32_t)metrics.distance()), ArialMT_Plain_16);
    fittedText(100, 48, 28, String((int)roundf(metrics.strokeRate)), ArialMT_Plain_16);
  } else {
    display.setFont(ArialMT_Plain_10);
    display.drawString(0, 15, "PREDKOSC"); display.drawString(75, 15, String(metrics.speed * 3.6f, 1) + " km/h");
    display.drawString(0, 27, "MOC SZAC."); display.drawString(75, 27, String((int)roundf(metrics.equivalentPower())) + " W");
    display.drawString(0, 39, "SR. /500m");
    display.drawString(75, 39, metrics.averagePace() > 0 ? formatTime((uint32_t)roundf(metrics.averagePace())) : "--:--");
    display.drawString(0, 51, "RUCHY~"); display.drawString(75, 51, String(metrics.strokes));
  }
  uint32_t lost;
  portENTER_CRITICAL(&pulseMux); lost = droppedPulses; portEXIT_CRITICAL(&pulseMux);
  if (lost) { display.setFont(ArialMT_Plain_10); display.drawString(23, 0, "!"); }
  display.display();
}

void setup() {
  SerialDebug.begin(115200);
  pinMode(ROWERINPUT, INPUT_PULLUP); pinMode(BUTTONSPIN, INPUT_PULLUP);
  display.init(); display.flipScreenVertically();
  initBLE();
  const int battery = constrain(map(analogRead(BATPIN), MinADC, MaxADC, 0, 127), 0, 127);
  battery_RA.fillValue(battery, battery_RA.getSize());
  resetWorkout();
  attachInterrupt(digitalPinToInterrupt(ROWERINPUT), rowerdebounceinterrupt, FALLING);
  drawDashboard();
}

void loop() {
  uint32_t pulseTime;
  // Read queue and current time under the same lock: never tick beyond a queued edge.
  while (true) {
    portENTER_CRITICAL(&pulseMux);
    const bool available = queueTail != queueHead;
    if (available) { pulseTime = pulseQueue[queueTail]; queueTail = (queueTail + 1) % QUEUE_SIZE; }
    else pulseTime = micros();
    portEXIT_CRITICAL(&pulseMux);
    if (!available) { metrics.tick(pulseTime); break; }
    metrics.pulse(pulseTime);
    if (LOG_SENSOR_PULSES) {
      SerialDebug.print("P,"); SerialDebug.print(pulseTime);
      SerialDebug.print(','); SerialDebug.print(metrics.pulses);
      SerialDebug.print(','); SerialDebug.println(metrics.strokes);
    }
  }
  const uint32_t now = millis();
  updateButton(now);
  if (now - lastBatteryMs >= 1000) {
    lastBatteryMs = now;
    battery_RA.addValue(constrain(map(analogRead(BATPIN), MinADC, MaxADC, 0, 127), 0, 127));
  }
  if (!deviceConnected && oldDeviceConnected) pServer->startAdvertising();
  oldDeviceConnected = deviceConnected;
  if (deviceConnected && now - lastBleMs >= 1000) { lastBleMs = now; setCxRowerData(); }
  if (now - lastDisplayMs >= 100) { lastDisplayMs = now; drawDashboard(); }
  delay(1);
}
