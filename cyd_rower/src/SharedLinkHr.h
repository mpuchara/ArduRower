#pragma once
// HR over the EXISTING FTMS Bluetooth LE ACL link.
// Bluetooth Core 6.2 permits only one ACL connection per pair of LE addresses.
// The local GAP Peripheral may also be a GATT Client on that single link.
#include "host/ble_gatt.h"
#include "host/ble_gap.h"
#include "host/ble_uuid.h"
#include "os/os_mbuf.h"
#include <stdint.h>

static constexpr uint16_t HR_INVALID_HANDLE=0xffff;
static volatile uint16_t hrFtmsHandle=HR_INVALID_HANDLE;
static volatile uint16_t hrServiceStart=0,hrServiceEnd=0;
static volatile uint16_t hrRemoteValueHandle=0,hrRemoteCccdHandle=0;
static volatile uint16_t hrProcedureStatus=0xffff;
static volatile bool hrProcedureDone=false,hrSameLinkReady=false;
static volatile uint32_t hrSameLinkPackets=0;
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
  hrFtmsHandle=handle;
  hrSameLinkReady=false;
  hrRemoteValueHandle=0;hrRemoteCccdHandle=0;
  receivedHr=0;receivedHrAt=0;hrConnected=false;
  Serial.printf("HR: reuse incoming FTMS ACL handle=%u; no second BLE connect\n",
                (unsigned)handle);
}
static void onFtmsHandleClosed() {
  hrFtmsHandle=HR_INVALID_HANDLE;
  hrSameLinkReady=false;hrConnected=false;
  hrRemoteValueHandle=0;hrRemoteCccdHandle=0;
  receivedHr=0;receivedHrAt=0;
}

static int onHrGapEvent(struct ble_gap_event *event,void *) {
  if(event->type!=BLE_GAP_EVENT_NOTIFY_RX ||
     event->notify_rx.conn_handle!=hrFtmsHandle ||
     event->notify_rx.attr_handle!=hrRemoteValueHandle)return 0;
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
    if(chr->properties & BLE_GATT_CHR_F_NOTIFY)hrRemoteValueHandle=chr->val_handle;
  } else {
    hrProcedureStatus=e->status==BLE_HS_EDONE?0:e->status;
    hrProcedureDone=true;
  }
  return 0;
}
static int onHrDescriptor(uint16_t handle,const ble_gatt_error *e,
                          uint16_t chrValHandle,const ble_gatt_dsc *dsc,void *) {
  (void)chrValHandle;
  if(handle!=hrFtmsHandle)return 0;
  if(e->status==0 && dsc &&
      ble_uuid_cmp(&dsc->uuid.u,&hrCccdUUID.u)==0)
    hrRemoteCccdHandle=dsc->handle;
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
