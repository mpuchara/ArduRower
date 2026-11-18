#!/usr/bin/env python3
"""Offline source-contract checks for the Garmin Connect IQ 0.1.6 extension.

Does not compile Monkey C, exercise BLE or prove on-watch/Connect behaviour.
Run: python3 tests/garmin_telemetry_016_test.py
"""
from pathlib import Path
import re

root = Path(__file__).resolve().parents[1]
field = (root / "connect_iq/source/ArduRowerApp.mc").read_text(encoding="utf-8")
packet = (root / "connect_iq/source/RowerPacket.mc").read_text(encoding="utf-8")
link = (root / "connect_iq/source/RowerLink.mc").read_text(encoding="utf-8")
cyd = (root / "cyd_rower/src/main.cpp").read_text(encoding="utf-8")
manifest = (root / "connect_iq/manifest.xml").read_text(encoding="utf-8")

assert 'version="0.1.6"' in manifest
assert '0.09.36' in cyd
assert 'SEND_EQUIVALENT_POWER = false' in cyd

# Keep the watch's validated, single-ACL FTMS/HR link protocol unchanged.
assert '00001826-0000-1000-8000-00805f9b34fb' in link
assert '00002ad1-0000-1000-8000-00805f9b34fb' in link
assert 'c9f10002-6f50-4fca-9b8d-3f8d90ae1b65' in link
assert 'WR S4BL3' in link
assert 'const uint16_t flags = SEND_EQUIVALENT_POWER ? 0x087e : 0x081e;' in cyd

# Existing 15-byte notification: flags, rate, stroke count, average rate,
# distance, pace, average pace, elapsed time.
sample = bytes([0x1e, 0x08, 60, 15, 0, 56, 0xf4, 1, 0,
                120, 0, 130, 0, 60, 0])
assert len(sample) == 15
assert sample[2] / 2 == 30
assert sample[5] / 2 == 28
assert int.from_bytes(sample[6:9], "little") == 500
assert int.from_bytes(sample[9:11], "little") == 120
assert int.from_bytes(sample[11:13], "little") == 130
assert int.from_bytes(sample[13:15], "little") == 60
for token in (":avgRate", ":avgPace", ":strokes", ":elapsed"):
    assert token in packet and token in field, token

# FIT developer fields are <= 32 bytes per record; IDs are stable for 0-4.
fields = re.findall(r'createField\("[^"]+",(\d+),Fit.DATA_TYPE_(FLOAT|UINT8|UINT16)', field)
assert [int(fid) for fid, _ in fields] == list(range(9)), fields
size = sum({"FLOAT": 4, "UINT8": 1, "UINT16": 2}[kind] for _, kind in fields)
assert size == 27 and size <= 32, size
for token in ("fitStrokes.setData", "fitElapsed.setData",
              "fitAvgPace.setData", "fitAvgCadence.setData",
              "fitLink.setData(valid?1:0)", "1800.0/d[:pace]"):
    assert token in field, token

print("PASS: Garmin 0.1.6 telemetry wire/source contract (27 FIT bytes)")
