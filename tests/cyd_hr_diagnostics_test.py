#!/usr/bin/env python3
"""Keep dual-role FTMS intact and expose why HR is unavailable on CYD."""
from pathlib import Path
import re

root=Path(__file__).resolve().parents[1]
src=(root/"cyd_rower/src/main.cpp").read_text(encoding="utf-8")
page=(root/"index.html").read_text(encoding="utf-8")
for marker in (
    "scanner->setActiveScan(true)",
    "scanner->start(4,false)",
    "BLEDevice::createServer()",
    "BLEDevice::createClient()",
    "BLEUUID((uint16_t)0x180D)",
    "BLEUUID((uint16_t)0x2A37)",
    "hrAdvertisers",
    "hrMatches",
    "HR RAM",
    "HR BRAK",
    "HR SKAN",
    "HR POL",
    "HR DANE",
    "HR OK",
    "HR BLAD",
    "HR ZAW",
    "getService(0x180D) ENTER",
    "getCharacteristic(0x2A37) ENTER",
    "registerForNotify(0x2A37) ENTER",
    "connect() ENTER",
    "connect() RETURN",
    "HR_FIRST_NOTIFICATION_MS=15000",
    "HR_MAX_FAILED_CONNECTS=12",
    "hrPreferredAddress",
    "hrNotifyPackets",
    "checkHrStall(now)",
    "scanner->stop()",
    "client->setClientCallbacks(&hrClientCallbacks)",
    "setHrState(HrState::LowMemory",
    "setHrState(HrState::NotFound",
    "hrState=HrState::Receiving",
    "String(hrStatus(displayedHrState(millis())))",
    "ArduRower CYD v0.09.4",
    'FIRMWARE_VERSION = "0.09.4"',
):
    assert marker in src, marker

assert src.count("BLEDevice::init(")==1, "Never reinitialize Bluetooth when starting HR receiver"
assert 'BLE_SERVICE_NAME "WR S4BL3"' in src, "Garmin must still see same FTMS peripheral"
assert 'pDtCharacteristic->notify()' in src
assert 'SEND_EQUIVALENT_POWER = false' in src
assert 'const EXPECTED_PREFIX = ' in page
assert 'const EXPECTED_PREFIX = \'0.09.4-hr-cccd-ghost-\'' in page
assert 'HR RAM' in page and 'HR BRAK' in page and 'HR OK' in page
assert "BLEDevice::deinit(" not in src, "Never reset existing FTMS host for HR"
assert "esp_ble_gap_disconnect(" not in src, "Never disconnect Garmin physical FTMS link for HR retry"
assert "vTaskDelete(client" not in src, "Never kill a BLE task still referenced by callbacks"
print("PASS: FTMS retained; HR connect/GATT stage tracing, 16s stalled state, safe HR-only retry")
