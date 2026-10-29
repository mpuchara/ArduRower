#!/usr/bin/env python3
"""Guard the previously working Garmin Connect IQ / FTMS wire contract.

This is a source-level regression guard, NOT a substitute for testing with the
actual fēnix 6X and field already installed on that watch.
"""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
OLD = (ROOT / "ArduRower" / "ArduRower.ino").read_text(encoding="utf-8")
CYD = (ROOT / "cyd_rower" / "src" / "main.cpp").read_text(encoding="utf-8")


def method(text: str, name: str) -> str:
    match = re.search(r"\b" + re.escape(name) + r"\(\)\s*\{", text)
    if not match:
        raise AssertionError(f"Missing method: {name}")
    start = match.start()
    depth = 0
    for idx in range(match.end() - 1, len(text)):
        if text[idx] == "{":
            depth += 1
        elif text[idx] == "}":
            depth -= 1
            if depth == 0:
                return text[start : idx + 1]
    raise AssertionError(f"Unbalanced braces in: {name}")


def normalize(text: str) -> str:
    text = re.sub(r"//[^\n]*", "", text)
    return re.sub(r"\s+", "", text)


for symbol in (
    "BLE_SERVICE_NAME",
    "FitnessMachineService",
    "FitnessMachineRowerData",
    "FitnessMachineFeature",
    "FitnessMachineControlPoint",
    "FitnessMachineStatus",
    "batteryLevel",
    "DEVICE_INFORMATION",
):
    regex = r"^#define\s+" + symbol + r"\s+(.+)$"
    lhs = re.search(regex, OLD, flags=re.M)
    rhs = re.search(regex, CYD, flags=re.M)
    assert lhs and rhs and lhs.group(1).strip() == rhs.group(1).strip(), symbol

assert "const bool SEND_EQUIVALENT_POWER = false" in OLD
assert "SEND_EQUIVALENT_POWER = false" in CYD
assert "../../ArduRower/RowerMetrics.h" in CYD

# Every FTMS data payload field remains a byte-for-byte equivalent expression.
# Differences after setValue() are limited to the Battery Service (CYD has no
# Heltec battery ADC; GPIO33 is touch CS).
end_marker = "pDtCharacteristic->setValue(data, offset)"
old_encoder = method(OLD, "setCxRowerData").split(end_marker)[0]
new_encoder = method(CYD, "setCxRowerData").split(end_marker)[0]
assert normalize(old_encoder) == normalize(new_encoder), "Rowing Data payload changed"

# Preserve the BLE advertising bytes and the set of FTMS characteristics.
for exact in (
    "0x02,0x01,0x06,0x05,0x02,0x26,0x18,0x0a,0x18",
    'setValue("Waterrower")',
    'setValue("2.2BLE")',
):
    assert exact in normalize(OLD) and exact in normalize(CYD), exact

old_ble = normalize(method(OLD, "initBLE"))
new_ble = normalize(method(CYD, "initBLE"))
for characteristic in ("pDtCharacteristic", "pFmfCharacteristic", "pCtrCharacteristic",
                       "pStCharacteristic", "pBatCharacteristic"):
    assert characteristic in old_ble and characteristic in new_ble

# Garmin discovers FTMS by the advertised primary packet. NimBLE's
# custom advertising setAdvertisementData() ignores addServiceUUID() added
# separately; the device name must be in that same primary packet.
assert 'oAdvertisementData.setName(BLE_SERVICE_NAME)' in CYD
assert 'pAdvertising->setAdvertisementData(oAdvertisementData)' in CYD
assert 'FTMS: primary advertising started=' in CYD
assert 'FTMS: advertising recovery' in CYD
assert 'pAdvertising->start()' in CYD
assert "now - lastBleMs >= 1000" in OLD
assert "now - lastBleMs >= 1000" in CYD
print("PASS: preserved FTMS/Connect IQ payload, identifiers, advertising and cadence")
