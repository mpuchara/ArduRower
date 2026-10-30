#!/usr/bin/env python3
"""Guard experimental dual-identity HR without touching Garmin Connect IQ FTMS.

The firmware MUST keep the public-address FTMS GATT server and create the
second HR link through a distinct static-random BLE identity. The host
NimBLE 1.4.3 refuses direct duplicate-peer initiations, so we intentionally
use a one-peer whitelist with a null peer_addr argument. Controller/watch may
still reject the second ACL; the code must leave FTMS untouched in that case.
"""
from pathlib import Path

p=Path(__file__).resolve().parents[1]
src=(p/"cyd_rower/src/main.cpp").read_text()
hr=(p/"cyd_rower/src/SharedLinkHr.h").read_text()
page=(p/"index.html").read_text()
ini=(p/"cyd_rower/platformio.ini").read_text()
for marker in (
    'FIRMWARE_VERSION = "0.09.15"',
    'ArduRower CYD v0.09.15',
    '#include "SharedLinkHr.h"',
    'NimBLEDevice::init(BLE_SERVICE_NAME)',
    'NimBLEDevice::createServer()',
    'NimBLEDevice::setCustomGapHandler(onHrGapEvent)',
    'onFtmsHandleReady(desc->conn_handle)',
    'pDtCharacteristic->notify()',
    'BLE_SERVICE_NAME "WR S4BL3"',
):
    assert marker in src,marker
for marker in (
    'ble_hs_id_gen_rnd(0,&generated)',
    'ble_hs_id_set_rnd(hrStaticAddr)',
    'BLE_OWN_ADDR_RANDOM',
    'BLE_GAP_EVENT_DISC',
    'BLE_GAP_EVENT_CONNECT',
    'BLE_GAP_EVENT_DISCONNECT',
    'ble_hs_adv_parse_fields(',
    'ble_gap_disc(',
    'ble_gap_wl_set(&peer,1)',
    'ble_gap_connect(BLE_OWN_ADDR_RANDOM,nullptr',
    'ble_gap_terminate(h,',
    'ble_gattc_disc_svc_by_uuid(',
    'ble_gattc_disc_chrs_by_uuid(',
    'ble_gattc_disc_all_dscs(',
    'ble_gattc_write_flat(',
    'ble_gattc_read(',
    'event->notify_rx.conn_handle!=hrClientHandle',
    'BLE_GAP_EVENT_NOTIFY_RX',
    'HR: dual-identity Garmin BPM packet',
    'dual-ACL ready=',
    'hrFtmsHandle=handle',
    'hrClientHandle=event->connect.conn_handle',
    'hrFtmsHandle=HR_INVALID_HANDLE',
    'return hrConnectDone && hrConnectStatus==0',
    '0x180d',
    'BLE_UUID16_INIT(0x2A37)',
    'BLE_UUID16_INIT(0x2902)',
):
    assert marker in hr,marker
assert 'NimBLEDevice::setOwnAddrType(' not in hr, "Do not mutate global FTMS address type"
assert 'while(!hrScanDone && (uint32_t)(millis()-began)<3400)' in hr
assert 'while(!hrConnectDone && (uint32_t)(millis()-connStarted)<8000)' in hr
assert '&& deviceConnected)' not in hr[hr.index('static bool scanAndConnectHr()'):hr.index('static void hrWorker(void *)')]
assert 'ble_gap_terminate(hrFtmsHandle,' not in hr, "Never terminate Garmin FTMS ACL"
assert 'ble_gap_connect(BLE_OWN_ADDR_RANDOM,&peer' not in hr
assert 'NimBLEDevice::createClient(' not in hr
assert 'h2zero/NimBLE-Arduino@1.4.3' in ini
assert "const EXPECTED_PREFIX = '0.09.15-ble-coexist-'" in page
assert 'extern "C" bool btInUse(void) { return true; }' in src
assert 'extern "C" bool bleInUse(void) { return true; }' in src
for required in (
  '#include "BleCoexistPolicy.h"',
  'HR_FIRST', 'FTMS_FIRST',
  'cydble::CoexistPolicy::ftmsCollisionPossible(',
  'cydble::CoexistPolicy::shouldYieldHrForFtms(',
  'cydble::CoexistPolicy::inRetryCooldown(',
  'cydble::CoexistPolicy::noCurrentSessionBpm(',
  'hrPacketsAtSubscription=hrNotifyPackets',
  'HR: HR-first without FTMS for 30s',
):
  assert required in hr,required
assert 'if(!deviceConnected || hrFtmsHandle==HR_INVALID_HANDLE)return false;' not in hr
assert 'if(!deviceConnected || hrClientHandle!=handle)return false;' not in hr
assert 'ble_gap_terminate(hrFtmsHandle,' not in hr
print("PASS: safe dual BLE identity, HR-first bootstrap, FTMS priority, per-session BPM timeout")
