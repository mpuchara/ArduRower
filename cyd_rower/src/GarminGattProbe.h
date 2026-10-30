#pragma once
// Bounded diagnostics on the existing FTMS ACL. Never pair, unpair or
// terminate a link. Vendor CCCD writes require UUID discovery and readback;
// their previous value is retained for explicit rollback.
static volatile uint8_t garminProbeRequest=0; // 1=dump, 2=legacy init, 3=restore, 4=ML HR, 5=ML stop
static uint16_t gpHandle=HR_INVALID_HANDLE;
static uint32_t gpEpoch=0;
static ble_gatt_svc gpServices[16]={};
static uint8_t gpServiceCount=0;
static uint16_t gpLegacyValue=0,gpLegacyEnd=0,gpLegacyCccd=0;
static uint8_t gpOldCccd[2]={};
static bool gpChanged=false;
static uint32_t gpVendorPackets=0;
static uint16_t gpMlValue=0,gpMlSend=0,gpMlEnd=0,gpMlCccd=0;
static uint8_t gpMlOldCccd[2]={};
static bool gpMlChanged=false,gpMlRequested=false;
static uint8_t gpMlStream=0;
static const uint8_t gpMlClient[8]={0x41,0x72,0x64,0x75,0x52,0x6f,0x77,0x31};
static volatile bool gpDone=false;
static volatile bool gpTimedOut=false;
static volatile int gpStatus=0;
static NimBLEUUID gpLegacyService("6a4e8022-667b-11e3-949a-0800200c9a66");
static NimBLEUUID gpLegacyNotify("6a4ecd28-667b-11e3-949a-0800200c9a66");
static NimBLEUUID gpMlNotify("6a4e2810-667b-11e3-949a-0800200c9a66");
static NimBLEUUID gpMlWrite("6a4e2820-667b-11e3-949a-0800200c9a66");
static NimBLEUUID gpMlService("6a4e2800-667b-11e3-949a-0800200c9a66");
static bool gpCurrent(uint16_t h) {
  return h==gpHandle && h==hrFtmsHandle && deviceConnected && gpEpoch==ftmsConnectedAtMs;
}
static void logGarminVendorNotify(struct ble_gap_event *event) {
  if(gpCurrent(event->notify_rx.conn_handle) && gpMlRequested &&
     event->notify_rx.attr_handle==gpMlValue) {
    uint8_t b[64];const size_t n=OS_MBUF_PKTLEN(event->notify_rx.om);
    if(n>sizeof(b) || os_mbuf_copydata(event->notify_rx.om,0,n,b))return;
    Serial.printf("GARMIN ML RX: size=%u data=",(unsigned)n);
    for(size_t i=0;i<n;++i)Serial.printf("%02x",b[i]);Serial.println();
    // Only accept registration for our client and service 6. No auth,
    // close-all, GFDI handshake or unrelated stream is requested.
    if(n>=13 && b[0]==0 && b[1]==1 && !memcmp(b+2,gpMlClient,8) && b[10]==6 && b[11]==0) {
      Serial.printf("GARMIN ML: REAL_TIME_HR registration status=%u\n",b[12]);
      if(n>=15 && b[12]==0 && b[13]!=0 && b[14]==0)gpMlStream=b[13];
    } else if(n>=6 && gpMlStream && b[0]==gpMlStream && b[1]==3 && b[2]>=35 && b[2]<=240) {
      receivedHr=b[2];receivedHrAt=millis();++hrNotifyPackets;++hrDualPackets;
      hrConnected=true;hrState=HrState::Receiving;
      Serial.printf("HR: Garmin ML live packet #%lu bpm=%u\n",(unsigned long)hrNotifyPackets,b[2]);
    }
    return;
  }
  if(!gpCurrent(event->notify_rx.conn_handle) || !gpLegacyValue ||
     event->notify_rx.attr_handle!=gpLegacyValue)return;
  if(++gpVendorPackets>12)return;
  uint8_t bytes[48];const size_t size=OS_MBUF_PKTLEN(event->notify_rx.om);
  const size_t n=size<sizeof(bytes)?size:sizeof(bytes);
  if(os_mbuf_copydata(event->notify_rx.om,0,n,bytes))return;
  Serial.printf("GARMIN VENDOR RX: packet=%lu size=%u data=",(unsigned long)gpVendorPackets,(unsigned)size);
  for(size_t i=0;i<n;++i)Serial.printf("%02x",bytes[i]);
  Serial.println();
}
static void gpPrepare(){gpDone=false;gpStatus=0xffff;}
static bool gpAwait(const char *what) {
  const uint32_t start=millis();
  while(!gpDone && gpCurrent(gpHandle) && millis()-start<60000)
    vTaskDelay(pdMS_TO_TICKS(20));
  Serial.printf("GARMIN PROBE: %s done=%d status=%d elapsed=%lu FTMS sent=%lu\n",
    what,gpDone?1:0,(int)gpStatus,(unsigned long)(millis()-start),(unsigned long)ftmsTxComplete);
  gpTimedOut=!gpDone && gpCurrent(gpHandle);
  return gpDone && gpStatus==0 && gpCurrent(gpHandle);
}
static int gpOnServices(uint16_t h,const ble_gatt_error *e,const ble_gatt_svc *s,void*) {
  if(!gpCurrent(h))return 0;
  if(!e->status && s) {
    char uuid[BLE_UUID_STR_LEN];ble_uuid_to_str(&s->uuid.u,uuid);
    Serial.printf("GARMIN SVC: %u..%u uuid=%s\n",s->start_handle,s->end_handle,uuid);
    if(gpServiceCount<16)gpServices[gpServiceCount++]=*s;
  } else {gpStatus=e->status==BLE_HS_EDONE?0:e->status;gpDone=true;}
  return 0;
}
static int gpOnChars(uint16_t h,const ble_gatt_error *e,const ble_gatt_chr *c,void*) {
  if(!gpCurrent(h))return 0;
  if(!e->status && c) {
    char uuid[BLE_UUID_STR_LEN];ble_uuid_to_str(&c->uuid.u,uuid);
    Serial.printf("GARMIN CHR: value=%u props=%u uuid=%s\n",c->val_handle,c->properties,uuid);
    if(gpMlValue && !gpMlEnd && c->def_handle>gpMlValue)gpMlEnd=c->def_handle-1;
    if(ble_uuid_cmp(&c->uuid.u,&gpLegacyNotify.getNative()->u)==0 &&
       (c->properties & BLE_GATT_CHR_F_NOTIFY))gpLegacyValue=c->val_handle;
    if(ble_uuid_cmp(&c->uuid.u,&gpMlNotify.getNative()->u)==0 &&
       (c->properties & BLE_GATT_CHR_F_NOTIFY))gpMlValue=c->val_handle;
    if(ble_uuid_cmp(&c->uuid.u,&gpMlWrite.getNative()->u)==0 &&
       (c->properties & BLE_GATT_CHR_F_WRITE_NO_RSP))gpMlSend=c->val_handle;
  } else {gpStatus=e->status==BLE_HS_EDONE?0:e->status;gpDone=true;}
  return 0;
}
static int gpOnDescriptors(uint16_t h,const ble_gatt_error *e,uint16_t value,const ble_gatt_dsc *d,void*) {
  if(!gpCurrent(h))return 0;
  if(!e->status && d) {
    if(value==gpLegacyValue && ble_uuid_cmp(&d->uuid.u,&hrCccdUUID.u)==0) {
      gpLegacyCccd=d->handle;
      Serial.printf("GARMIN CCCD: legacy value=%u descriptor=%u\n",value,d->handle);
    }
    if(value==gpMlValue && ble_uuid_cmp(&d->uuid.u,&hrCccdUUID.u)==0)gpMlCccd=d->handle;
  } else if(e->status){gpStatus=e->status==BLE_HS_EDONE?0:e->status;gpDone=true;}
  return 0;
}
static int gpOnReadCccd(uint16_t h,const ble_gatt_error *e,ble_gatt_attr *a,void*) {
  if(!gpCurrent(h))return 0;
  gpStatus=e->status;
  if(!e->status && a && OS_MBUF_PKTLEN(a->om)==2)
    gpStatus=os_mbuf_copydata(a->om,0,2,gpOldCccd);
  else if(!e->status)gpStatus=BLE_HS_EINVAL;
  gpDone=true;return 0;
}
static int gpOnHrWrite(uint16_t h,const ble_gatt_error *e,ble_gatt_attr*,void*) {
  if(!gpCurrent(h))return 0;
  gpStatus=e->status;gpDone=true;return 0;
}
static int gpOnLegacyWrite(uint16_t h,const ble_gatt_error *e,ble_gatt_attr*,void*) {
  if(!gpCurrent(h))return 0;
  if(e->status){gpStatus=e->status;gpDone=true;return 0;}
  gpChanged=true;
  const uint8_t enabled[2]={1,0};
  // Queue HR CCCD immediately from the vendor CCCD completion callback.
  const int rc=ble_gattc_write_flat(h,hrRemoteCccdHandle,enabled,2,gpOnHrWrite,nullptr);
  if(rc){gpStatus=rc;gpDone=true;}
  return 0;
}
static void runGarminProbe(uint8_t mode) {
  const uint16_t current=hrFtmsHandle;
  if(current==HR_INVALID_HANDLE || !deviceConnected)return;
  const bool cached=gpHandle==current && gpEpoch==ftmsConnectedAtMs && (gpLegacyCccd || gpMlCccd);
  if(mode!=3 && mode!=5 && cached && (gpChanged || gpMlChanged)) {
    Serial.println("GARMIN PROBE: restore with garminoff/garminmloff before repeating probe");return;
  }
  if(mode==5) {
    if(cached && gpMlRequested) {
      if(gpMlStream) {
        uint8_t close[13]={0,2};memcpy(close+2,gpMlClient,8);close[10]=6;close[12]=gpMlStream;
        Serial.printf("GARMIN ML: close own HR stream rc=%d\n",ble_gattc_write_no_rsp_flat(current,gpMlSend,close,sizeof(close)));
      }
      gpMlRequested=false;gpMlStream=0;
      if(gpMlChanged) {
        gpPrepare();int rc=ble_gattc_write_flat(current,gpMlCccd,gpMlOldCccd,2,gpOnHrWrite,nullptr);
        if(!rc && gpAwait("restore ML CCCD"))gpMlChanged=false;
      }
    }
    return;
  }
  if(mode==3) {
    if(cached && gpChanged) {
      gpPrepare();
      const int rc=ble_gattc_write_flat(current,gpLegacyCccd,gpOldCccd,2,gpOnHrWrite,nullptr);
      if(!rc && gpAwait("restore original legacy CCCD"))gpChanged=false;
    }
    return;
  }
  if(!cached || mode==1) {
    gpHandle=current;gpEpoch=ftmsConnectedAtMs;gpServiceCount=0;
    gpLegacyValue=gpLegacyEnd=gpLegacyCccd=0;gpChanged=false;gpVendorPackets=0;
    gpMlValue=gpMlSend=gpMlEnd=gpMlCccd=0;gpMlChanged=gpMlRequested=false;gpMlStream=0;
    gpPrepare();
    int rc=ble_gattc_disc_all_svcs(current,gpOnServices,nullptr);
    if(rc || !gpAwait("all services"))return;
    for(uint8_t i=0;i<gpServiceCount;++i) {
      const auto &s=gpServices[i];
      if(mode==4 && ble_uuid_cmp(&s.uuid.u,&gpMlService.getNative()->u)!=0)continue;
      gpPrepare();
      rc=ble_gattc_disc_all_chrs(current,s.start_handle,s.end_handle,gpOnChars,nullptr);
      if(rc || !gpAwait("characteristics"))return;
      if(ble_uuid_cmp(&s.uuid.u,&gpLegacyService.getNative()->u)==0)gpLegacyEnd=s.end_handle;
      if(!gpMlEnd && gpMlValue>=s.start_handle && gpMlValue<=s.end_handle)gpMlEnd=s.end_handle;
    }
    if(gpMlValue && gpMlEnd) {
      gpPrepare();rc=ble_gattc_disc_all_dscs(current,gpMlValue,gpMlEnd,gpOnDescriptors,nullptr);
      if(rc || !gpAwait("ML descriptor discovery"))return;
    }
    if(gpLegacyValue && gpLegacyEnd) {
      gpPrepare();
      rc=ble_gattc_disc_all_dscs(current,gpLegacyValue,gpLegacyEnd,gpOnDescriptors,nullptr);
      if(rc || !gpAwait("legacy descriptor discovery"))return;
    }
    Serial.printf("GARMIN PROBE: dump complete services=%u legacyValue=%u legacyCCCD=%u\n",
      gpServiceCount,gpLegacyValue,gpLegacyCccd);
  }
  if(mode==4) {
    if(!gpMlCccd || !gpMlSend || gpMlRequested)return;
    gpPrepare();int rc=ble_gattc_read(current,gpMlCccd,gpOnReadCccd,nullptr);
    if(rc || !gpAwait("read previous ML CCCD"))return;
    memcpy(gpMlOldCccd,gpOldCccd,2);
    const uint8_t enabled[2]={1,0};gpPrepare();
    rc=ble_gattc_write_flat(current,gpMlCccd,enabled,2,gpOnHrWrite,nullptr);
    if(rc || !gpAwait("enable ML notifications"))return;
    gpMlChanged=true;gpMlRequested=true;gpMlStream=0;
    uint8_t registration[13]={0,0};memcpy(registration+2,gpMlClient,8);registration[10]=6;
    gpPrepare();rc=ble_gattc_write_flat(current,gpMlSend,registration,sizeof(registration),gpOnHrWrite,nullptr);
    Serial.printf("GARMIN ML: request own REAL_TIME_HR stream with response rc=%d\n",rc);
    if(!rc)gpAwait("ML registration ATT response");
    hrPacketsAtSubscription=hrNotifyPackets;hrConnectedAtMs=millis();
    return;
  }
  if(mode!=2)return;
  if(!gpLegacyCccd || hrClientHandle!=current || !hrSharedTransport || !hrRemoteCccdHandle) {
    Serial.println("GARMIN PROBE: vendor initialization refused: UUID/HR session not verified");return;
  }
  gpPrepare();
  int rc=ble_gattc_read(current,gpLegacyCccd,gpOnReadCccd,nullptr);
  if(rc || !gpAwait("read previous legacy CCCD"))return;
  Serial.printf("GARMIN PROBE: legacy prior=%02x %02x; enable legacy then HR CCCD\n",gpOldCccd[0],gpOldCccd[1]);
  const uint8_t enabled[2]={1,0};gpPrepare();
  rc=ble_gattc_write_flat(current,gpLegacyCccd,enabled,2,gpOnLegacyWrite,nullptr);
  if(rc || !gpAwait("vendor + HR CCCD writes"))return;
  // A new observation window; cached ATT reads never refresh measurement.
  hrPacketsAtSubscription=hrNotifyPackets;hrConnectedAtMs=millis();
  Serial.println("GARMIN PROBE: vendor initialization done; waiting for real 2A37 notifications");
}

