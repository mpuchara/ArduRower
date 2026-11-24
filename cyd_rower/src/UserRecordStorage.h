#pragma once
#include <nvs.h>
#include "GhostHeart.h"
#include "StartupNavigation.h"

class UserRecordStorage {
 public:
  enum class Read : uint8_t { Found, Missing, OtherCalibration, Error };

  void select(uint8_t user) {
    name=startup::recordNamespace(user);
    memset(bytes,0,sizeof(bytes));
  }

  Read loadBest(uint8_t index,ghostrace::Record &out,uint32_t calibration) {
    memset(&out,0,sizeof(out));
    size_t n=sizeof(bytes);
    const Read status=readBlob(raceKeys[index],n);
    if(status!=Read::Found)return status;
    if(n==sizeof(out))memcpy(&out,bytes,n); // full v3/v4 blob
    else {
      const size_t header=offsetof(ghostrace::Record,decimeters);
      if(n<header+sizeof(out.intervalSeconds)+sizeof(out.checksum))return Read::Error;
      memcpy(&out,bytes,header);
      if(out.version!=ghostrace::kVersion || !out.count || out.count>ghostrace::kSamples)
        return Read::Error;
      const size_t samples=out.count*sizeof(uint16_t);
      if(n!=header+samples+sizeof(out.intervalSeconds)+sizeof(out.checksum))return Read::Error;
      memcpy(out.decimeters,bytes+header,samples);
      memcpy(&out.intervalSeconds,bytes+header+samples,sizeof(out.intervalSeconds));
      memcpy(&out.checksum,bytes+header+samples+sizeof(out.intervalSeconds),sizeof(out.checksum));
    }
    if(!ghostrace::valid(out,ghostrace::kDistances[index],out.scaleMilli))return Read::Error;
    return out.scaleMilli==calibration?Read::Found:Read::OtherCalibration;
  }

  bool saveBest(uint8_t index,const ghostrace::Record &record,uint32_t calibration) {
    if(record.version!=ghostrace::kVersion ||
       !ghostrace::valid(record,ghostrace::kDistances[index],calibration))return false;
    const size_t prefix=offsetof(ghostrace::Record,decimeters)+record.count*sizeof(uint16_t);
    memcpy(bytes,&record,prefix);
    memcpy(bytes+prefix,&record.intervalSeconds,sizeof(record.intervalSeconds));
    memcpy(bytes+prefix+sizeof(record.intervalSeconds),&record.checksum,sizeof(record.checksum));
    return writeBlob(raceKeys[index],prefix+sizeof(record.intervalSeconds)+sizeof(record.checksum));
  }

  Read loadHeart(uint8_t index,ghostrace::HeartRecord &out,const ghostrace::Record &record) {
    out={};
    size_t n=sizeof(bytes);
    const Read status=readBlob(heartKeys[index],n);
    if(status!=Read::Found)return status;
    if(n==sizeof(out))memcpy(reinterpret_cast<uint8_t*>(&out),bytes,n);
    else {
      const size_t header=offsetof(ghostrace::HeartRecord,bpm);
      if(n<header+sizeof(out.checksum))return Read::Error;
      memcpy(reinterpret_cast<uint8_t*>(&out),bytes,header);
      if(!out.count || out.count>ghostrace::kSamples || n!=header+out.count+sizeof(out.checksum))
        return Read::Error;
      memcpy(out.bpm,bytes+header,out.count);
      memcpy(&out.checksum,bytes+header+out.count,sizeof(out.checksum));
    }
    if(out.raceChecksum!=record.checksum)return Read::Missing; // older sidecar
    return ghostrace::validHeart(out,record)?Read::Found:Read::Error;
  }

  bool saveHeart(uint8_t index,const ghostrace::HeartRecord &heart,const ghostrace::Record &record) {
    if(!ghostrace::validHeart(heart,record))return false;
    size_t oldSize=sizeof(bytes);
    if(readBlob(heartKeys[index],oldSize)==Read::Error)return false;
    const size_t prefix=offsetof(ghostrace::HeartRecord,bpm)+heart.count;
    memcpy(bytes,&heart,prefix);
    memcpy(bytes+prefix,&heart.checksum,sizeof(heart.checksum));
    return writeBlob(heartKeys[index],prefix+sizeof(heart.checksum));
  }

  Read loadWins(uint8_t index,uint32_t &wins) const {
    wins=0;
    if(!name)return Read::Error;
    nvs_handle_t handle;
    esp_err_t rc=nvs_open(name,NVS_READONLY,&handle);
    if(rc==ESP_OK) {
      rc=nvs_get_u32(handle,winKeys[index],&wins);
      nvs_close(handle);
    }
    return result(rc);
  }

  bool awardWin(uint8_t index,uint32_t &wins) const {
    uint32_t previous;
    if(loadWins(index,previous)==Read::Error)return false;
    const uint32_t next=previous==UINT32_MAX?UINT32_MAX:previous+1;
    nvs_handle_t handle;
    if(nvs_open(name,NVS_READWRITE,&handle)!=ESP_OK)return false;
    esp_err_t rc=nvs_set_u32(handle,winKeys[index],next);
    if(rc==ESP_OK)rc=nvs_commit(handle);
    nvs_close(handle);
    if(rc==ESP_OK)wins=next;
    return rc==ESP_OK;
  }

 private:
  const char *name=nullptr;
  uint8_t bytes[sizeof(ghostrace::Record)]={};
  const char *const raceKeys[3]={"g7_1000","g7_3000","g7_10000"};
  const char *const heartKeys[3]={"h7_1000","h7_3000","h7_10000"};
  const char *const winKeys[3]={"w7_1000","w7_3000","w7_10000"};

  static Read result(esp_err_t rc) {
    if(rc==ESP_OK)return Read::Found;
    return rc==ESP_ERR_NVS_NOT_FOUND?Read::Missing:Read::Error;
  }

  Read readBlob(const char *key,size_t &size) {
    if(!name)return Read::Error;
    nvs_handle_t handle;
    esp_err_t rc=nvs_open(name,NVS_READONLY,&handle);
    if(rc==ESP_OK) {
      rc=nvs_get_blob(handle,key,bytes,&size);
      nvs_close(handle);
    }
    return result(rc);
  }

  bool writeBlob(const char *key,size_t size) const {
    if(!name)return false;
    nvs_handle_t handle;
    if(nvs_open(name,NVS_READWRITE,&handle)!=ESP_OK)return false;
    esp_err_t rc=nvs_set_blob(handle,key,bytes,size);
    if(rc==ESP_OK)rc=nvs_commit(handle);
    nvs_close(handle);
    return rc==ESP_OK;
  }
};
