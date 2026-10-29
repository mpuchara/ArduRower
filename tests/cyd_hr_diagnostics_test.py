#!/usr/bin/env python3
"""Assert one NimBLE host for FTMS and Garmin HR, no deadlock-induced HR PAUZ."""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
src=(root/"cyd_rower/src/main.cpp").read_text()
hr=(root/"cyd_rower/src/NimbleHr.h").read_text()
ini=(root/"cyd_rower/platformio.ini").read_text()
page=(root/"index.html").read_text()
for v in ('NimBLEDevice::createServer()', 'NimBLEDevice::init(BLE_SERVICE_NAME)',
          'NIMBLE_PROPERTY::NOTIFY', 'pDtCharacteristic->notify()',
          '#include "NimbleHr.h"', 'FIRMWARE_VERSION = "0.09.6"',
          'ArduRower CYD v0.09.6', 'BLE_SERVICE_NAME "WR S4BL3"',
          'String(hrStatus(displayedHrState(millis())))'):
    assert v in src,v
for v in ('NimBLEDevice::getScan()', 'NimBLEDevice::createClient()',
          'scan->setActiveScan(true)', 'scan->start(2,false)',
          'NimBLEUUID((uint16_t)0x180D)', 'NimBLEUUID((uint16_t)0x2A37)',
          'chr->subscribe(true,hrNotify,true)', 'hrNotifyPackets',
          'no FTMS pause gate', 'NimBLE connect ENTER', 'HR: packet #',
          'client->setConnectTimeout(6)', 'HR_STALLED_AFTER_MS'):
    assert v in hr,v
assert 'h2zero/NimBLE-Arduino@1.4.3' in ini
assert 'const EXPECTED_PREFIX = \'0.09.6-nimble-ghost-\'' in page
assert 'HR PAUZ' not in hr, "Never block HR because Garmin is receiving FTMS"
assert 'BLEDevice::deinit(' not in src and 'esp_ble_gap_disconnect(' not in src
assert src.count('NimBLEDevice::init(')==1
print("PASS: one NimBLE BLE host, simultaneous FTMS/peripheral + Garmin HR/client")
