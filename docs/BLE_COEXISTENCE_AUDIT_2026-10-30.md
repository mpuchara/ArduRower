# BLE audit: Garmin fēnix 6X Pro Solar ↔ ArduRower CYD

Date: 2026-10-30. Firmware audited: 0.09.14, changes: 0.09.15.

## Non-negotiable contract

Keep **WR S4BL3** as the original FTMS/GATT peripheral: public Bluetooth address, Fitness Machine service `0x1826`, Rower Data `0x2AD1`, 1 Hz notification cadence, original Rower Data bytes (checked by `tests/cyd_ble_contract_test.py`). Do not change the watch's existing Connect IQ data field or BLE pairing identity. The display's `BLE OK` indicates the Garmin FTMS client is connected, **not** that the HR feed is receiving.

## Evidence and chronology

- 0.09.7–0.09.8: standalone NimBLE GATT HR client scanned `0x180D`, connected to a broadcaster and subscribed to `0x2A37`; device logs showed actual BPM notifications in the 64–85 range. However the Garmin FTMS connection was not reliably available simultaneously.
- 0.09.9–0.09.11: GATT client attempted to subscribe to Garmin's `0x2A37` **on the incoming FTMS ACL**. Previous physical logs showed service/characteristic/CCCD discovery successful and CCCD readback `01 00`, yet `rawNotify=0` and BPM remained zero. The watch did not send HR notifications on that particular connection. This says nothing definitive about the protocol's ability to use another central ACL.
- 0.09.13–0.09.14: code created a static-random BLE identity in NVS `rower-hr-id`, scanned `0x180D`, and initiated `ble_gap_connect(BLE_OWN_ADDR_RANDOM,nullptr,...)` through a one-peer controller whitelist. User confirmed working FTMS but **no BPM**. No raw USB logs from those specific sessions are available in this audit, therefore the failing stage is unknown.

## Findings

| Severity | Defect / limitation | Consequence | 0.09.15 |
| --- | --- | --- | --- |
| Critical | `scanAndConnectHr()` refused to run without an active FTMS link. | HR could not establish *before* Garmin stopped advertising `0x180D` in FTMS-active mode. | Removed prerequisite, log `HR_FIRST` / `FTMS_FIRST`. |
| Critical | `hrWorker()` forcibly terminated the HR ACL whenever FTMS was not connected. | The previously proven working **HR-first** sequence was impossible; it could not wait for Garmin to join FTMS. | HR ACL persists while FTMS tries to join. |
| High | Total-lifetime `hrNotifyPackets==0` treated as a current-session heartbeat. | An earlier successful HR session could prevent retry after a subsequent silent HR ACL. | Snapshot count on CCCD subscribe and compare counts per ACL. |
| High | A dual-role HR connection can prevent the Garmin from joining FTMS at all. | A seemingly good BPM stream could sacrifice the training data feed. | 30 s FTMS join grace, then terminate HR only; wait 120 s before retry. Existing FTMS collision guard remains. |
| Medium | HR discovery while rowing with FTMS linked can disrupt radio/GATT timing. | Risk of degrading existing 1 Hz FTMS stream. | Defer new HR discovery if rowing is not quiet. Existing links keep receiving data. |
| Medium | Previous preferred HR source MAC may differ from current rotating Garmin address. | Candidate selection can be wrong if another BLE HR source is nearby. | Existing candidate scoring prioritizes the current FTMS peer and the historically proven HR source; logs show candidate MAC. Still depends on advertising. |
| Unknown | The ESP32 controller + NimBLE + fēnix may reject a second ACL to one physical watch even with two different local addresses. | Firmware may never obtain simultaneous connections on one radio. | Cannot be resolved/verified by source audit or CI: **hardware test required**. |

## Current architecture

```text
Garmin [GAP central: Connect IQ] ──> ESP32 PUBLIC BLE (FTMS/GATT server)
Garmin [GAP peripheral: HR 180D] <── ESP32 RANDOM BLE (GATT client)
                                     same NimBLE host / ESP32 radio
```

In the normal HR-first startup, FTMS service continues advertising throughout HR discovery. The HR link is allowed to remain while Garmin attempts to join FTMS. The firmware never executes `ble_gap_terminate()` using the FTMS ACL handle. If both roles remain connected, FTMS 1 Hz notifications and HR GATT notifications should be serviced by the same controller scheduler. If not, **the program gives priority to FTMS**.

## Verified by CI / not verified by CI

- CI checks exact FTMS identifier/payload/advertising/cadence equivalence and builds confirmed ST7789/GPIO21 firmware.
- Unit tests in `tests/cyd_ble_coexist_test.cpp` exercise HR-first waiting, FTMS-after-HR, 30-second grace, 120-second cooldown, per-session BPM timeout and collision guard, plus 32-bit elapsed wrapping.
- CI **cannot** verify that a Garmin makes `0x180D` discoverable during a session, that the controller accepts two links to one physical watch, that CCCD notifications actually arrive, or that Connect IQ retains the connection during the experiment.

## Physical verification on the watch

1. Enable Garmin's heart-rate broadcast during the activity. Start with the CYD powered off; start broadcast/rower activity, then power CYD to let the HR-first window take place.
2. Keep the watch's existing FTMS/Connect IQ field unchanged.
3. Check 115200 serial logs for `HR: attempt order=HR_FIRST`, `HR: private scan seen=...`, `HR: RANDOM central CONNECTED`, `HR: dual-identity Garmin BPM packet`, `FTMS: CONNECTED`, and rising `FTMS tx`.
4. If HR connects but Garmin never joins FTMS, confirm `HR: HR-first without FTMS for 30s; release HR ONLY to restore FTMS`; report failure of actual coexistence, not a successful implementation.
5. If FTMS connects first, `HR: attempt order=FTMS_FIRST` may still be tried when rowing is idle; `HR=0` during scan means the watch is no longer advertising, not that the BPM decoder is broken.

A measured success requires **simultaneous non-zero BPM and increasing FTMS notifications**. Otherwise only the individual BLE paths have been demonstrated.
