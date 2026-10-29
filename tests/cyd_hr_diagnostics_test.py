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
    "hrState=HrState::LowMemory",
    "hrState=HrState::NotFound",
    "hrState=HrState::Receiving",
    "String(hrStatus(hrState))",
    "ArduRower CYD v0.09.2",
    'FIRMWARE_VERSION = "0.09.2"',
):
    assert marker in src, marker

assert src.count("BLEDevice::init(")==1, "Never reinitialize Bluetooth when starting HR receiver"
assert 'BLE_SERVICE_NAME "WR S4BL3"' in src, "Garmin must still see same FTMS peripheral"
assert 'pDtCharacteristic->notify()' in src
assert 'SEND_EQUIVALENT_POWER = false' in src
assert 'const EXPECTED_PREFIX = ' in page
assert 'const EXPECTED_PREFIX = \'0.09.2-hr-scan-\'' in page
assert 'HR RAM' in page and 'HR BRAK' in page and 'HR OK' in page
print("PASS: BT FTMS retained, active HR scan and distinguishable error states")
