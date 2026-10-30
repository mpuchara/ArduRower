#pragma once
// Dual-identity BLE experiment for the same Garmin watch.
// FTMS remains a public-address GAP peripheral. HR initiates as a separate
// random-static-address GAP central via a BLE controller whitelist.
// Using a whitelist bypasses NimBLE's peer-address-only duplicate ACL guard;
// radio/controller/watch may still reject a second link. Fail closed without
// modifying, disconnecting, or changing the public FTMS peripheral.
#include "nimble/nimble/host/include/host/ble_gatt.h"
#include "nimble/nimble/host/include/host/ble_gap.h"
#include "nimble/nimble/host/include/host/ble_uuid.h"
#include "nimble/nimble/host/include/host/ble_hs_id.h"
#include "nimble/nimble/host/include/host/ble_hs_adv.h"
#include "nimble/porting/nimble/include/os/os_mbuf.h"
#include <stdint.h>

static constexpr uint16_t HR_INVALID_HANDLE=0xffff;
static volatile uint16_t hrFtmsHandle=HR_INVALID_HANDLE;
static volatile uint16_t hrClientHandle=HR_INVALID_HANDLE;
static volatile bool hrScanDone=false,hrConnectDone=false,hrConnecting=false;
static volatile int hrConnectStatus=0xffff;
static ble_addr_t hrScanCandidate={};
static volatile bool hrCandidateValid=false;
static volatile int hrCandidateScore=-200;
static volatile uint32_t hrScanSeen=0,hrScanMatched=0,hrSecondAclAttempts=0;
static uint8_t hrStaticAddr[6]={};
static bool hrOwnRandomReady=false;
static volatile uint16_t hrServiceStart=0,hrServiceEnd=0;
static volatile uint16_t hrRemoteValueHandle=0,hrRemoteCccdHandle=0;
static volatile uint16_t hrProcedureStatus=0xffff;
static volatile bool hrProcedureDone=false,hrDualReady=false;
static volatile uint32_t hrDualPackets=0;
static volatile uint32_t hrGapNotifies=0,hrGapOtherAttributes=0,hrLinkEpoch=0;
static const ble_uuid16_t hrServiceUUID=BLE_UUID16_INIT(0x180D);
static const ble_uuid16_t hrMeasureUUID=BLE_UUID16_INIT(0x2A37);
static const ble_uuid16_t hrCccdUUID=BLE_UUID16_INIT(0x2902);

static void setHrState(HrState state,const char *msg) {
  hrState=state;hrStageSinceMs=millis();
  Serial.printf("HR: %s FTMS=%d ftmsACL=%u hrACL=%u tx=%lu\n",msg,deviceConnected?1:0,
      (unsigned)hrFtmsHandle,(unsigned)hrClientHandle,(unsigned long)ftmsNotifications);
}
static HrState displayedHrState(uint32_t now) { (void)now;return hrState; }
static void checkHrStall(uint32_t now) { (void)now; }

static void onFtmsHandleReady(uint16_t handle) {
  hrFtmsHandle=handle;
  Serial.printf("HR: FTMS stays on PUBLIC ACL=%u; HR central will use RANDOM identity\n",
                (unsigned)handle);
}
static void onFtmsHandleClosed() {
  hrFtmsHandle=HR_INVALID_HANDLE;
  // Only the HR worker is allowed to terminate its own central link.
  // Never call ble_gap_terminate() on the Garmin FTMS handle.
}

static int onHrGapEvent(struct ble_gap_event *event,void *) {
  if(event->type!=BLE_GAP_EVENT_NOTIFY_RX)return 0;
  ++hrGapNotifies;
  if(hrGapNotifies<=8)Serial.printf(
    "HR: raw NOTIFY #%lu acl=%u attr=%u expected=%u len=%u\n",
    (unsigned long)hrGapNotifies,(unsigned)event->notify_rx.conn_handle,
    (unsigned)event->notify_rx.attr_handle,(unsigned)hrRemoteValueHandle,
    (unsigned)OS_MBUF_PKTLEN(event->notify_rx.om));
  if(event->notify_rx.conn_handle!=hrClientHandle ||
     event->notify_rx.attr_handle!=hrRemoteValueHandle) {
    ++hrGapOtherAttributes;
    return 0;
  }
  uint8_t data[3]={0};
  const size_t n=OS_MBUF_PKTLEN(event->notify_rx.om);
  if(n<2 || os_mbuf_copydata(event->notify_rx.om,0,(int)((n<3)?n:3),data)!=0)
    return 0;
  const bool wide=(data[0]&1)!=0;
  if(wide && n<3)return 0;
  const uint16_t bpm=wide? (uint16_t)(data[1]|(uint16_t(data[2])<<8)):data[1];
  if(bpm>=35 && bpm<=240) {
    receivedHr=bpm;receivedHrAt=millis();
    ++hrNotifyPackets;++hrSameLinkPackets;
    hrConnected=true;hrState=HrState::Receiving;
    if(hrSameLinkPackets<=5)Serial.printf("HR: dual-identity Garmin BPM packet #%lu bpm=%u\n",
                   (unsigned long)hrSameLinkPackets,(unsigned)bpm);
  }
  return 0;
}

static int onHrService(uint16_t handle,const ble_gatt_error *e,
                       const ble_gatt_svc *svc,void *) {
  if(handle!=hrClientHandle)return 0;
  if(e->status==0 && svc) {
    hrServiceStart=svc->start_handle;hrServiceEnd=svc->end_handle;
    Serial.printf("HR: 180D handles=%u..%u\n",(unsigned)hrServiceStart,(unsigned)hrServiceEnd);
  } else {
    hrProcedureStatus=e->status==BLE_HS_EDONE?0:e->status;
    hrProcedureDone=true;
  }
  return 0;
}
static int onHrCharacteristic(uint16_t handle,const ble_gatt_error *e,
                              const ble_gatt_chr *chr,void *) {
  if(handle!=hrClientHandle)return 0;
  if(e->status==0 && chr) {
    Serial.printf("HR: 2A37 value=%u props=%u notify=%d\n",(unsigned)chr->val_handle,(unsigned)chr->properties,(chr->properties & BLE_GATT_CHR_F_NOTIFY)?1:0);
    if(chr->properties & BLE_GATT_CHR_F_NOTIFY)hrRemoteValueHandle=chr->val_handle;
  } else {
    hrProcedureStatus=e->status==BLE_HS_EDONE?0:e->status;
    hrProcedureDone=true;
  }
  return 0;
}
static int onHrDescriptor(uint16_t handle,const ble_gatt_error *e,
                          uint16_t chrValHandle,const ble_gatt_dsc *dsc,void *) {
  if(handle!=hrClientHandle)return 0;
  if(e->status==0 && dsc) {
    if(chrValHandle==hrRemoteValueHandle &&
       ble_uuid_cmp(&dsc->uuid.u,&hrCccdUUID.u)==0) {
      hrRemoteCccdHandle=dsc->handle;
      Serial.printf("HR: CCCD 2A37 val=%u desc=%u\n",
                    (unsigned)chrValHandle,(unsigned)dsc->handle);
    }
  }
  else if(e->status!=0) {
    hrProcedureStatus=e->status==BLE_HS_EDONE?0:e->status;
    hrProcedureDone=true;
  }
  return 0;
}
static int onHrWrite(uint16_t handle,const ble_gatt_error *e,
                     ble_gatt_attr *,void *) {
  if(handle!=hrClientHandle)return 0;
  hrProcedureStatus=e->status;
  hrProcedureDone=true;
  return 0;
}
static int onHrReadCccd(uint16_t handle,const ble_gatt_error *e,
                        ble_gatt_attr *attr,void *) {
  if(handle!=hrClientHandle)return 0;
  if(e->status==0 && attr && OS_MBUF_PKTLEN(attr->om)>=2) {
    uint8_t data[2]={0};
    if(os_mbuf_copydata(attr->om,0,2,data)==0)
      Serial.printf("HR: CCCD readback=%02x %02x (01 00 enables notify)\n",
                    (unsigned)data[0],(unsigned)data[1]);
  } else Serial.printf("HR: optional CCCD readback status=%u\n",(unsigned)e->status);
  return 0;
}
static bool awaitHrProcedure(uint16_t handle,const char *phase) {
  const uint32_t started=millis();
  while(!hrProcedureDone && (uint32_t)(millis()-started)<6000) {
    if(!deviceConnected || hrClientHandle!=handle)return false;
    vTaskDelay(pdMS_TO_TICKS(20));
  }
  Serial.printf("HR: shared ACL %s done=%d status=%u elapsed=%lu ms\n",phase,
                hrProcedureDone?1:0,(unsigned)hrProcedureStatus,
                (unsigned long)(millis()-started));
  return hrProcedureDone && hrProcedureStatus==0 &&
         deviceConnected && hrClientHandle==handle;
}
static void prepareHrProcedure() {
  hrProcedureDone=false;hrProcedureStatus=0xffff;
}

// We never create a second GAP connection to Garmin. Discovery is carried out
// with NimBLE's low-level GATT client using the connection handle delivered
// by our existing NimBLEServer (Garmin is already the central).

static bool configureHrRandomIdentity() {
  if(hrOwnRandomReady)return true;
  Preferences prefs;
  bool loaded=false;
  if(prefs.begin("rower-hr-id",true)) {
    if(prefs.getBytesLength("rnd")==6 &&
       prefs.getBytes("rnd",hrStaticAddr,6)==6 &&
       (hrStaticAddr[5]&0xc0)==0xc0)loaded=true;
    prefs.end();
  }
  if(!loaded) {
    ble_addr_t generated={};
    const int rc=ble_hs_id_gen_rnd(0,&generated);
    if(rc) {
      Serial.printf("HR: random identity generation failed rc=%d\n",rc);
      return false;
    }
    memcpy(hrStaticAddr,generated.val,6);
    if(prefs.begin("rower-hr-id",false)) {
      prefs.putBytes("rnd",hrStaticAddr,6);
      prefs.end();
    }
  }
  const int rc=ble_hs_id_set_rnd(hrStaticAddr);
  hrOwnRandomReady=(rc==0);
  Serial.printf("HR: PRIVATE central identity rc=%d %02x:%02x:..:%02x; PUBLIC FTMS unchanged\n",
                rc,hrStaticAddr[5],hrStaticAddr[4],hrStaticAddr[0]);
  return hrOwnRandomReady;
}
// Scan for the watch's advertised 180D, as in the working older NimbleHr.h.
// Match its FTMS peer address when possible, otherwise select strongest 180D.
// The selected address is the ONLY address added to the initiating whitelist.
static int onHrScanEvent(ble_gap_event *event,void *) {
  if(event->type==BLE_GAP_EVENT_DISC) {
    ++hrScanSeen;
    const ble_gap_disc_desc &adv=event->disc;
    ble_hs_adv_fields fields={};
    if(ble_hs_adv_parse_fields(&fields,adv.data,adv.length_data)!=0)return 0;
    bool hasHr=false;
    for(uint8_t i=0;i<fields.num_uuids16;++i)
      if(fields.uuids16[i].value==0x180d)hasHr=true;
    if(!hasHr)return 0;
    ++hrScanMatched;
    int score=adv.rssi;
    ble_gap_conn_desc ftms={};
    if(hrFtmsHandle!=HR_INVALID_HANDLE &&
       ble_gap_conn_find(hrFtmsHandle,&ftms)==0 &&
       (ble_addr_cmp(&adv.addr,&ftms.peer_ota_addr)==0 ||
        ble_addr_cmp(&adv.addr,&ftms.peer_id_addr)==0))score+=1000;
    if(!hrCandidateValid || score>hrCandidateScore) {
      hrScanCandidate=adv.addr;
      hrCandidateScore=score;
      hrCandidateValid=true;
    }
  } else if(event->type==BLE_GAP_EVENT_DISC_COMPLETE) {
    hrScanDone=true;
  }
  return 0;
}
static int onHrCentralGapEvent(ble_gap_event *event,void *) {
  if(event->type==BLE_GAP_EVENT_CONNECT) {
    hrConnecting=false;
    hrConnectStatus=event->connect.status;
    if(event->connect.status==0) {
      hrClientHandle=event->connect.conn_handle;
      ++hrLinkEpoch;
      Serial.printf("HR: RANDOM central CONNECTED ACL=%u; PUBLIC FTMS ACL=%u\n",
                    (unsigned)hrClientHandle,(unsigned)hrFtmsHandle);
    } else {
      Serial.printf("HR: RANDOM central rejected status=%d; FTMS unaffected\n",
                    event->connect.status);
    }
    hrConnectDone=true;
  } else if(event->type==BLE_GAP_EVENT_DISCONNECT) {
    if(event->disconnect.conn.conn_handle==hrClientHandle) {
      Serial.printf("HR: RANDOM central disconnected reason=%d; FTMS=%d\n",
                    event->disconnect.reason,deviceConnected?1:0);
      hrClientHandle=HR_INVALID_HANDLE;
      hrSameLinkReady=false;
      hrConnected=false;receivedHr=0;receivedHrAt=0;
      ++hrLinkEpoch;
    }
    hrConnecting=false;
    hrConnectDone=true;
  }
  return 0;
}
static bool scanAndConnectHr() {
  if(!deviceConnected || hrFtmsHandle==HR_INVALID_HANDLE)return false;
  if(!configureHrRandomIdentity())return false;
  hrScanDone=false;hrCandidateValid=false;hrCandidateScore=-200;
  hrScanSeen=hrScanMatched=0;
  ble_gap_disc_params params={};
  params.itvl=160;params.window=28;params.passive=0;
  params.filter_duplicates=0;
  setHrState(HrState::Scan,"PRIVATE HR scan 180D, FTMS running");
  int rc=ble_gap_disc(BLE_OWN_ADDR_RANDOM,2600,&params,onHrScanEvent,nullptr);
  if(rc) {
    Serial.printf("HR: private scan rc=%d; preserving FTMS\n",rc);
    return false;
  }
  const uint32_t began=millis();
  while(!hrScanDone && (uint32_t)(millis()-began)<3400 && deviceConnected)
    vTaskDelay(pdMS_TO_TICKS(30));
  if(!hrScanDone)ble_gap_disc_cancel();
  if(!deviceConnected)return false;
  Serial.printf("HR: private scan seen=%lu HR=%lu candidate=%d score=%d\n",
                (unsigned long)hrScanSeen,(unsigned long)hrScanMatched,
                hrCandidateValid?1:0,(int)hrCandidateScore);
  if(!hrCandidateValid) {
    setHrState(HrState::NotFound,"No BLE 180D advertiser with FTMS connected");
    return false;
  }
  // NimBLE 1.4.3 ble_gap_connect(peer_addr) rejects an address already used
  // by the FTMS peripheral even with a DIFFERENT local address. Whitelist
  // initiation uses peer_addr=nullptr and avoids that host-only duplicate
  // check, while the controller still sees a distinct random local address.
  const ble_addr_t peer=hrScanCandidate;
  rc=ble_gap_wl_set(&peer,1);
  if(rc) {
    Serial.printf("HR: whitelist set rc=%d\n",rc);
    return false;
  }
  ble_gap_conn_params cp={};
  cp.scan_itvl=160;cp.scan_window=28;
  cp.itvl_min=24;cp.itvl_max=48;
  cp.latency=0;cp.supervision_timeout=180;
  cp.min_ce_len=0;cp.max_ce_len=0;
  hrConnectDone=false;hrConnectStatus=0xffff;hrConnecting=true;
  ++hrSecondAclAttempts;
  setHrState(HrState::Connecting,"RANDOM central + whitelist connect; no FTMS changes");
  rc=ble_gap_connect(BLE_OWN_ADDR_RANDOM,nullptr,7000,&cp,
                     onHrCentralGapEvent,nullptr);
  Serial.printf("HR: second-identity connect attempt=%lu rc=%d FTMS=%d\n",
                (unsigned long)hrSecondAclAttempts,rc,deviceConnected?1:0);
  if(rc) {
    hrConnecting=false;
    return false;
  }
  const uint32_t connStarted=millis();
  while(!hrConnectDone && (uint32_t)(millis()-connStarted)<8000 && deviceConnected)
    vTaskDelay(pdMS_TO_TICKS(30));
  if(!hrConnectDone && hrConnecting)ble_gap_conn_cancel();
  return deviceConnected && hrConnectDone && hrConnectStatus==0 &&
         hrClientHandle!=HR_INVALID_HANDLE &&
         hrClientHandle!=hrFtmsHandle;
}

static void hrWorker(void *) {
  Serial.println("HR: EXPERIMENT two identities: PUBLIC FTMS, RANDOM central HR");
  uint32_t processedEpoch=UINT32_MAX;
  uint32_t lastReport=0,lastAttempt=0,connectedAt=0;
  uint8_t failures=0;
  while(true) {
    const uint32_t now=millis();
    if(!deviceConnected || hrFtmsHandle==HR_INVALID_HANDLE) {
      if(hrClientHandle!=HR_INVALID_HANDLE)
        ble_gap_terminate(hrClientHandle,BLE_ERR_REM_USER_CONN_TERM);
      processedEpoch=UINT32_MAX;
      if(hrState!=HrState::Paused)
        setHrState(HrState::Paused,"FTMS disconnected; HR central disabled");
      vTaskDelay(pdMS_TO_TICKS(600));continue;
    }
    const uint16_t h=hrClientHandle;
    if(h==HR_INVALID_HANDLE) {
      const uint32_t backoff=failures>=3?120000UL:20000UL;
      if(lastAttempt && (uint32_t)(now-lastAttempt)<backoff) {
        vTaskDelay(pdMS_TO_TICKS(600));continue;
      }
      lastAttempt=now;
      if(scanAndConnectHr()) {
        failures=0;connectedAt=millis();processedEpoch=UINT32_MAX;
      } else {
        if(failures<5)++failures;
        setHrState(HrState::Disconnected,"RANDOM HR ACL not available (FTMS intact)");
      }
      vTaskDelay(pdMS_TO_TICKS(500));continue;
    }
    const uint32_t epoch=hrLinkEpoch;
    if(processedEpoch==epoch) {
      if((uint32_t)(now-lastReport)>8000) {
        lastReport=now;
        Serial.printf("HR: dual-ACL ready=%d bpm=%u packets=%lu rawNotify=%lu FTMS tx=%lu FTMS=%u HR=%u\n",
                      hrSameLinkReady?1:0,(unsigned)liveHr(now),
                      (unsigned long)hrNotifyPackets,
                      (unsigned long)hrGapNotifies,
                      (unsigned long)ftmsNotifications,
                      (unsigned)hrFtmsHandle,(unsigned)hrClientHandle);
      }
      if(hrSameLinkReady && hrNotifyPackets==0 &&
         (uint32_t)(now-connectedAt)>45000UL) {
        Serial.println("HR: dual ACL subscribed but no BPM after 45s; recycle ONLY HR central");
        ble_gap_terminate(h,BLE_ERR_REM_USER_CONN_TERM);
      }
      vTaskDelay(pdMS_TO_TICKS(500));continue;
    }
    processedEpoch=epoch;
    hrServiceStart=hrServiceEnd=0;
    hrRemoteValueHandle=hrRemoteCccdHandle=0;
    hrSameLinkReady=false;
    setHrState(HrState::Service,"discover Garmin HR 180D on RANDOM central ACL");
    prepareHrProcedure();
    int rc=ble_gattc_disc_svc_by_uuid(h,&hrServiceUUID.u,onHrService,nullptr);
    if(rc || !awaitHrProcedure(h,"service 180D") || !hrServiceStart) {
      setHrState(HrState::NotFound,"180D unavailable on HR central ACL");
      ble_gap_terminate(h,BLE_ERR_REM_USER_CONN_TERM);continue;
    }
    setHrState(HrState::Characteristic,"discover 2A37 on HR central ACL");
    prepareHrProcedure();
    rc=ble_gattc_disc_chrs_by_uuid(h,hrServiceStart,hrServiceEnd,
                                   &hrMeasureUUID.u,onHrCharacteristic,nullptr);
    if(rc || !awaitHrProcedure(h,"characteristic 2A37") || !hrRemoteValueHandle) {
      setHrState(HrState::NotFound,"2A37 unavailable");
      ble_gap_terminate(h,BLE_ERR_REM_USER_CONN_TERM);continue;
    }
    setHrState(HrState::Descriptor,"discover 2A37 CCCD");
    prepareHrProcedure();
    rc=ble_gattc_disc_all_dscs(h,hrRemoteValueHandle,hrServiceEnd,
                               onHrDescriptor,nullptr);
    if(rc || !awaitHrProcedure(h,"descriptor 2902") || !hrRemoteCccdHandle) {
      setHrState(HrState::InitFailed,"2A37 CCCD unavailable");
      ble_gap_terminate(h,BLE_ERR_REM_USER_CONN_TERM);continue;
    }
    setHrState(HrState::Subscribing,"subscribe to Garmin BPM via independent HR ACL");
    prepareHrProcedure();
    const uint8_t notify[2]={0x01,0x00};
    rc=ble_gattc_write_flat(h,hrRemoteCccdHandle,notify,sizeof(notify),
                            onHrWrite,nullptr);
    if(rc || !awaitHrProcedure(h,"subscribe CCCD")) {
      setHrState(HrState::InitFailed,"remote HR CCCD write failed");
      ble_gap_terminate(h,BLE_ERR_REM_USER_CONN_TERM);continue;
    }
    hrSameLinkReady=true;
    const int readRc=ble_gattc_read(h,hrRemoteCccdHandle,onHrReadCccd,nullptr);
    if(readRc)Serial.printf("HR: CCCD readback call error=%d\n",readRc);
    setHrState(HrState::NoData,"dual-identity HR subscribed; waiting for BPM");
  }
}
