#!/usr/bin/env python3
"""Guard single-ACL Garmin HR GATT client: BLE standard forbids second ACL to same peer."""
from pathlib import Path
p=Path(__file__).resolve().parents[1]
src=(p/"cyd_rower/src/main.cpp").read_text()
hr=(p/"cyd_rower/src/SharedLinkHr.h").read_text()
page=(p/"index.html").read_text()
ini=(p/"cyd_rower/platformio.ini").read_text()
assert 'h2zero/NimBLE-Arduino@1.4.3' in ini
for key in ('FIRMWARE_VERSION = "0.09.11"','ArduRower CYD v0.09.11',
            'NimBLEDevice::createServer()','NimBLEDevice::init(BLE_SERVICE_NAME)',
            '#include "SharedLinkHr.h"',
            'NimBLEDevice::setCustomGapHandler(onHrGapEvent)',
            'onFtmsHandleReady(desc->conn_handle)',
            'onFtmsHandleClosed()',
            'pDtCharacteristic->notify()'):
    assert key in src,key
for key in ('ble_gattc_disc_svc_by_uuid(', 'ble_gattc_disc_chrs_by_uuid(',
            'ble_gattc_disc_all_dscs(', 'ble_gattc_write_flat(',
            'BLE_GAP_EVENT_NOTIFY_RX',
            'chrValHandle==hrRemoteValueHandle',
            'HR: raw NOTIFY',
            'HR: CCCD readback=',
            'ble_gattc_read(',
            'event->notify_rx.conn_handle!=hrFtmsHandle',
            'ble_uuid_cmp(', 'BLE_UUID16_INIT(0x180D)',
            'BLE_UUID16_INIT(0x2A37)', 'BLE_UUID16_INIT(0x2902)',
            'HR: shared FTMS ACL BPM packet',
            'HR: reuse incoming FTMS ACL handle',
            'client' if False else 'NimBLEDevice::setCustomGapHandler' ):
    if key=='NimBLEDevice::setCustomGapHandler':continue
    assert key in hr,key
assert 'NimBLEDevice::createClient(' not in src and 'NimBLEDevice::createClient(' not in hr
assert 'client->connect(' not in hr
assert 'NimBLEDevice::getScan()' not in src and 'NimBLEDevice::getScan()' not in hr
assert "const EXPECTED_PREFIX = '0.09.11-ghost-tap-'" in page
assert 'extern "C" bool btInUse(void) { return true; }' in src
assert 'extern "C" bool bleInUse(void) { return true; }' in src
print("PASS: HR GATT on existing incoming FTMS ACL, zero second BLE connections")
