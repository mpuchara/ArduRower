# CYD 0.09.16-0.09.17: touch and Garmin coexistence investigation

Base: `b574725`, ST7789/GPIO21, ESP32-D0WD-V3 revision 3.1, COM6.

## Measured before changes

- Serial boot identified 0.09.15. No usable NVS touch calibration was loaded.
- Calibration failed; a contact `raw=2261,274 z=3657 irq=0` did not produce a release in the captured boot window.
- Setting the HR random address after FTMS advertising returned `524` (HCI Command Disallowed). This prevents that attempt from reaching discovery.
- In another live window FTMS was connected with its counter increasing. HR advertisement from `b4:c2:6a:95:7c:23` was detected, but creating the second ACL returned `523` (HCI ACL Connection Already Exists). A missing advertisement is therefore not the only failure.
- A complete 4 MiB flash backup was read before writing firmware. NVS is never erased as part of the update.

## Changes

- One tap anywhere on the ordinary dashboard opens the distance menu. Whole distance rows select 500/1000/2000 m. WROC is a touch exit. The user has no access to BOOT: physical button operation is not part of acceptance.
- Startup defaults to the ordinary dashboard and TEMPO /500m. Only selecting a distance arms the race and replaces pace with ROZNICA / DUCH. Opening the menu alone does not arm a race.
- The XPT2046 keeps the known UBox pins and coordinate convention. Both explicit-byte hardware SPI and independent GPIO-clocked SPI were tested. Both report the same persistent pressure. The active diagnostic build uses GPIO-clocked SPI; no claim that an SPI byte-order defect was proven.
- Calibration requires stable release between targets, logs each raw target, and saves the affine transform in the existing NVS namespace.
- Best race distance records remain unchanged. Optional HR traces have their own checksum and are bound to the exact best race checksum; missing or mismatched traces are ignored. Existing best races can still replay speed.
- Live graph traces stay bright. Dim best speed and, when recorded, best HR are drawn underneath. The pace field becomes the distance lead at the same elapsed race time.
- Configure the random HR identity before advertising. Run one bounded startup HR discovery/connection attempt before the initial FTMS advertisement. USB `hrfirst` requests a one-shot extended startup scan and a 180 s HR-first join window; ordinary startup retains the 30 s fallback. No running FTMS session is stopped for discovery.
- Preserve FTMS UUIDs, primary advertisement, name, byte encoder and 1 Hz cadence. Retain the 30 s HR-only yield and 120 s retry cooldown if FTMS cannot join.
- STATUS separately reports FTMS subscription, queued notifications, successful controller transmit events, fresh BPM and packet count. `simultaneous=1` means both streams progressed in the last status window on the ESP32; it does not establish the watch application's display or recording behavior.

## Validation status

Host C++ tests cover metrics, race timing/replay, corruption, optional HR gaps, byte-oriented ADC pressure/release and BLE coexistence policy. Python guards cover the unchanged FTMS wire contract, navigation, display cache bounds and installer version matching.

Firmware build, flash verification and physical touch/Garmin results are recorded below when performed. Host tests alone do not establish hardware success or impossibility.

## COM6 results

- Versions 0.09.16 and diagnostic 0.09.17 compiled and were application-only flashed at 0x10000. esptool verified the written hash each time. Strong `btInUse` and `bleInUse` hooks were verified in the ELF.
- All six Python checks and nine native C++ tests passed, including the extended diagnostic join-window test.
- With no physical release, the touchscreen reports approximately raw 2245,272, Z=3665, Z1=650, Z2=1080, IRQ=0. This persisted with software SPI. A deliberate touch changed coordinates temporarily, but the untouched baseline returned to permanent strong pressure. User reported that the calibration cross never advanced. Mechanical pressure at the panel edge is a hypothesis requiring a physical check, not a confirmed diagnosis.
- A standalone HR-first connection really received 38 HR notifications at 90 BPM from the watch. The existing 30 s FTMS join guard then deliberately released HR; FTMS connected. That automatic release is not evidence that simultaneous operation is impossible.
- On the incoming FTMS ACL, service 180D (48..51), characteristic 2A37 (value handle 50, properties 16 / NOTIFY), and CCCD 51 were discovered. CCCD readback was 01 00, but no notifications arrived on this ACL.
- Despite declaring NOTIFY-only, Garmin accepted explicit ATT reads of 2A37 and returned 90 BPM. Repeated reads progressed while FTMS subscriptions and successful transmit counters also progressed, producing simultaneous=1 transport windows. However, all observed read values were 90. **Freshness against changing pulse on the watch is not yet accepted.**
- Shared-ACL teardown clears only HR state; it never terminates the FTMS connection. HR polling is serialized, and an ATT read error disables polling rather than fabricating a measurement.

## Open physical acceptance

1. Check whether removing pressure from the screen bezel/film restores IRQ high / Z near zero. Complete calibration and confirm tap-only menu and all three distance rows.
2. Compare explicit HR reads with changing HR shown on Garmin. Constant cached data must not be called a live HR fix.
3. If reads are stale, use the explicit 180 s diagnostic HR-first test and attempt FTMS while HR remains connected. Do not infer impossibility from the 30 s fallback.
4. Exercise Ghost selection, equal-elapsed distance lead, bright live / dim best curves, persistence and restart on the physical screen after touch input works.

## Reference code

- UBox touch calibration/pins: `mpuchara/HA_UBOX_bridge` at `7f1f165`.
- Standard 180D/2A37 subscription and 8/16-bit BPM decoding: `cagnulein/qdomyos-zwift`, `src/devices/heartratebelt/heartratebelt.cpp` at `5a55180`. The current HR parser already follows that format; changing the parser cannot fix an HCI connection rejection.
- NimBLE-Arduino 1.4.3: custom GAP handlers are registered as global event listeners, including NOTIFY_RX. The existing raw HR central callback does not need a second notification forwarder.

ESP32 supports simultaneous BLE central/peripheral roles in general: <https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-guides/ble/get-started/ble-introduction.html>. That general capability does not prove two opposite-role ACLs to this particular Garmin with the same peer address.
