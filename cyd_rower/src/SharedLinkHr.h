#pragma once
// HR over the EXISTING FTMS Bluetooth LE ACL link.
// Bluetooth Core 6.2 permits only one ACL connection per pair of LE addresses.
// The local GAP Peripheral may also be a GATT Client on that single link.
#include "nimble/nimble/host/include/host/ble_gatt.h"
#include "nimble/nimble/host/include/host/ble_gap.h"
#include "nimble/nimble/host/include/host/ble_uuid.h"
#include "nimble/porting/nimble/include/os/os_mbuf.h"
#include <stdint.h>

static constexpr uint16_t HR_INVALID_HANDLE=0xffff;
static volatile uint16_t hrFtmsHandle=HR_INVALID_HANDLE;
static volatile uint16_t hrServiceStart=0,hrServiceEnd=0;
static volatile uint16_t hrRemoteValueHandle=0,hrRemoteCccdHandle=0;
static volatile uint16_t hrProcedureStatus=0xffff;
static volatile bool hrProcedureDone=false,hrSameLinkReady=false;
static volatile uint32_t hrSameLinkPackets=0;
static volatile uint32_t hrGapNotifies=0,hrGapOtherAttributes=0,hrLinkEpoch=0;
static const ble_uuid16_t hrServiceUUID=BLE_UUID16_INIT(0x180D);
static const ble_uuid16_t hrMeasureUUID=BLE_UUID16_INIT(0x2A37);
static const ble_uuid16_t hrCccdUUID=BLE_UUID16_INIT(0x2902);

static void setHrState(HrState state,const char *msg) {
  hrState=state;hrStageSinceMs=millis();
  Serial.printf("HR: %s FTMS=%d acl=%u tx=%lu\n",msg,deviceConnected?1:0,
      (unsigned)hrFtmsHandle,(unsigned long)ftmsNotifications);
}
static HrState displayedHrState(uint32_t now) { (void)now;return hrState; }
static void checkHrStall(uint32_t now) { (void)now; }

static void onFtmsHandleReady(uint16_t handle) {
  ++hrLinkEpoch;
  hrFtmsHandle=handle;
  hrSameLinkReady=false;
  hrRemoteValueHandle=0;hrRemoteCccdHandle=0;
  receivedHr=0;receivedHrAt=0;hrConnected=false;
  Serial.printf("HR: reuse incoming FTMS ACL handle=%u; no second BLE connect\n",
                (unsigned)handle);
}
static void onFtmsHandleClosed() {
  ++hrLinkEpoch;
  hrFtmsHandle=HR_INVALID_HANDLE;
  hrSameLinkReady=false;hrConnected=false;
  hrRemoteValueHandle=0;hrRemoteCccdHandle=0;
  receivedHr=0;receivedHrAt=0;
}

static int onHrGapEvent(struct ble_gap_event *event,void *) {
  if(event->type!=BLE_GAP_EVENT_NOTIFY_RX)return 0;
  ++hrGapNotifies;
  if(hrGapNotifies<=8)Serial.printf(
    "HR: raw NOTIFY #%lu acl=%u attr=%u expected=%u len=%u\n",
    (unsigned long)hrGapNotifies,(unsigned)event->notify_rx.conn_handle,
    (unsigned)event->notify_rx.attr_handle,(unsigned)hrRemoteValueHandle,
    (unsigned)OS_MBUF_PKTLEN(event->notify_rx.om));
  if(event->notify_rx.conn_handle!=hrFtmsHandle ||
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
    if(hrSameLinkPackets<=5)Serial.printf("HR: shared FTMS ACL BPM packet #%lu bpm=%u\n",
                   (unsigned long)hrSameLinkPackets,(unsigned)bpm);
  }
  return 0;
}

static int onHrService(uint16_t handle,const ble_gatt_error *e,
                       const ble_gatt_svc *svc,void *) {
  if(handle!=hrFtmsHandle)return 0;
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
  if(handle!=hrFtmsHandle)return 0;
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
  if(handle!=hrFtmsHandle)return 0;
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
  if(handle!=hrFtmsHandle)return 0;
  hrProcedureStatus=e->status;
  hrProcedureDone=true;
  return 0;
}
static int onHrReadCccd(uint16_t handle,const ble_gatt_error *e,
                        ble_gatt_attr *attr,void *) {
  if(handle!=hrFtmsHandle)return 0;
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
    if(!deviceConnected || hrFtmsHandle!=handle)return false;
    vTaskDelay(pdMS_TO_TICKS(20));
  }
  Serial.printf("HR: shared ACL %s done=%d status=%u elapsed=%lu ms\n",phase,
                hrProcedureDone?1:0,(unsigned)hrProcedureStatus,
                (unsigned long)(millis()-started));
  return hrProcedureDone && hrProcedureStatus==0 &&
         deviceConnected && hrFtmsHandle==handle;
}
static void prepareHrProcedure() {
  hrProcedureDone=false;hrProcedureStatus=0xffff;
}

// We never create a second GAP connection to Garmin. Discovery is carried out
// with NimBLE's low-level GATT client using the connection handle delivered
// by our existing NimBLEServer (Garmin is already the central).
static void hrWorker(void *) {
  Serial.println("HR: shared FTMS ACL GATT client enabled (NO second BLE connection)");
  uint32_t processedEpoch=UINT32_MAX;
  uint32_t lastReport=0;
  while(true) {
    const uint16_t h=hrFtmsHandle;
    if(h==HR_INVALID_HANDLE || !deviceConnected) {
      processedEpoch=UINT32_MAX;
      if(hrState!=HrState::Paused)
        setHrState(HrState::Paused,"waiting for Garmin FTMS link (GATT duplex)");
      vTaskDelay(pdMS_TO_TICKS(500));continue;
    }
    const uint32_t epoch=hrLinkEpoch;
    if(processedEpoch==epoch) {
      if((uint32_t)(millis()-lastReport)>8000) {
        lastReport=millis();
        Serial.printf("HR: single-ACL ready=%d bpm=%u packets=%lu rawNotify=%lu otherAttr=%lu FTMS tx=%lu\n",
                      hrSameLinkReady?1:0,(unsigned)liveHr(millis()),
                      (unsigned long)hrNotifyPackets,
                      (unsigned long)hrGapNotifies,
                      (unsigned long)hrGapOtherAttributes,
                      (unsigned long)ftmsNotifications);
      }
      vTaskDelay(pdMS_TO_TICKS(500));continue;
    }
    processedEpoch=epoch;
    hrServiceStart=hrServiceEnd=0;
    hrRemoteValueHandle=hrRemoteCccdHandle=0;
    hrSameLinkReady=false;
    setHrState(HrState::Service,"discover Garmin HR 180D on *existing* FTMS ACL");
    prepareHrProcedure();
    int rc=ble_gattc_disc_svc_by_uuid(h,&hrServiceUUID.u,onHrService,nullptr);
    if(rc) {
      Serial.printf("HR: shared ACL service discovery call error=%d\n",rc);
      setHrState(HrState::InitFailed,"Garmin FTMS ACL service discovery rejected");
      continue;
    }
    if(!awaitHrProcedure(h,"service 180D") || !hrServiceStart) {
      setHrState(HrState::NotFound,"Garmin does not expose 180D on FTMS link");
      continue;
    }
    setHrState(HrState::Characteristic,"discover 2A37 on same FTMS ACL");
    prepareHrProcedure();
    rc=ble_gattc_disc_chrs_by_uuid(h,hrServiceStart,hrServiceEnd,
                                   &hrMeasureUUID.u,onHrCharacteristic,nullptr);
    if(rc || !awaitHrProcedure(h,"characteristic 2A37") || !hrRemoteValueHandle) {
      Serial.printf("HR: no remote measurement (rc=%d)\n",rc);
      setHrState(HrState::NotFound,"no Garmin 2A37 on shared ACL");
      continue;
    }
    setHrState(HrState::Descriptor,"discover Garmin CCCD on FTMS ACL");
    prepareHrProcedure();
    rc=ble_gattc_disc_all_dscs(h,hrRemoteValueHandle,hrServiceEnd,
                               onHrDescriptor,nullptr);
    if(rc || !awaitHrProcedure(h,"descriptor 2902") || !hrRemoteCccdHandle) {
      Serial.printf("HR: no remote CCCD (rc=%d)\n",rc);
      setHrState(HrState::InitFailed,"no 0x2902 CCCD on FTMS ACL");
      continue;
    }
    setHrState(HrState::Subscribing,"enable Garmin HR notifications on FTMS ACL");
    prepareHrProcedure();
    const uint8_t notify[2]={0x01,0x00};
    rc=ble_gattc_write_flat(h,hrRemoteCccdHandle,notify,sizeof(notify),
                            onHrWrite,nullptr);
    if(rc || !awaitHrProcedure(h,"subscribe CCCD")) {
      Serial.printf("HR: subscribe 2902 error=%d\n",rc);
      setHrState(HrState::InitFailed,"Garmin refused HR subscribe on FTMS ACL");
      continue;
    }
    hrSameLinkReady=true;
    const int readRc=ble_gattc_read(h,hrRemoteCccdHandle,onHrReadCccd,nullptr);
    if(readRc)Serial.printf("HR: CCCD readback call error=%d\n",readRc);
    setHrState(HrState::NoData,"shared ACL HR subscribed; waiting for Garmin BPM");
  }
}
