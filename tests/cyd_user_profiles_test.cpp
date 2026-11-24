#include <assert.h>
#include <map>
#include <string>
#include <vector>
#include "../cyd_rower/src/UserRecordStorage.h"

using Blob=std::vector<uint8_t>;
struct Namespace {
  std::map<std::string,Blob> blobs;
  std::map<std::string,uint32_t> numbers;
};
static std::map<std::string,Namespace> flash;
static Namespace *opened=nullptr;
static bool failWrite=false,failRead=false;

esp_err_t nvs_open(const char *name,nvs_open_mode_t mode,nvs_handle_t *handle) {
  assert(!opened);
  if(mode==NVS_READONLY && !flash.count(name))return ESP_ERR_NVS_NOT_FOUND;
  opened=&flash[name];
  *handle=1;
  return ESP_OK;
}
void nvs_close(nvs_handle_t) {assert(opened);opened=nullptr;}
esp_err_t nvs_get_blob(nvs_handle_t,const char *key,void *output,size_t *length) {
  if(failRead)return ESP_ERR_NVS_INVALID_STATE;
  if(opened->numbers.count(key))return ESP_ERR_NVS_TYPE_MISMATCH;
  if(!opened->blobs.count(key))return ESP_ERR_NVS_NOT_FOUND;
  const Blob &blob=opened->blobs.at(key);
  if(*length<blob.size())return ESP_ERR_NVS_INVALID_LENGTH;
  *length=blob.size();
  memcpy(output,blob.data(),*length);
  return ESP_OK;
}
esp_err_t nvs_get_u32(nvs_handle_t,const char *key,uint32_t *value) {
  if(opened->blobs.count(key))return ESP_ERR_NVS_TYPE_MISMATCH;
  if(!opened->numbers.count(key))return ESP_ERR_NVS_NOT_FOUND;
  *value=opened->numbers.at(key);
  return ESP_OK;
}
esp_err_t nvs_set_blob(nvs_handle_t,const char *key,const void *bytes,size_t length) {
  if(failWrite)return ESP_ERR_NVS_NOT_ENOUGH_SPACE;
  const auto *p=static_cast<const uint8_t*>(bytes);
  opened->blobs[key]=Blob(p,p+length);
  return ESP_OK;
}
esp_err_t nvs_set_u32(nvs_handle_t,const char *key,uint32_t value) {
  if(failWrite)return ESP_ERR_NVS_NOT_ENOUGH_SPACE;
  opened->numbers[key]=value;
  return ESP_OK;
}
esp_err_t nvs_commit(nvs_handle_t) {assert(opened);return ESP_OK;}

static ghostrace::Record workout(uint8_t index,uint32_t duration) {
  ghostrace::Race race;
  const uint16_t distance=ghostrace::kDistances[index];
  race.arm(distance,nullptr);
  race.start(0);
  for(uint32_t time=100;time<duration;time+=100)
    race.advance(time,distance*(time/static_cast<float>(duration)));
  assert(race.advance(duration,distance));
  assert(race.attempt.finishMs==duration);
  return race.attempt;
}

static void navigationTest() {
  using startup::Page;
  using startup::Choice;
  startup::Navigation pages;
  assert(pages.page==Page::Welcome && !pages.measuring() && !pages.user);
  assert(!pages.selectUser(0));
  pages.tick(1999);
  assert(pages.page==Page::Welcome);
  pages.tick(2000);
  assert(pages.page==Page::Users && pages.options()==4 && !pages.user);
  for(uint8_t i=0;i<4;++i) {
    assert(pages.focus==i);
    const auto box=startup::tile(pages.page,i);
    assert(box.contains(box.x+box.w/2,box.y+box.h/2));
    assert(!box.contains(box.x+box.w,box.y));
    pages.next();
  }
  assert(pages.focus==0);
  assert(pages.selectUser(0) && pages.user==1 && pages.page==Page::Modes);
  assert(!pages.measuring() && !pages.selectUser(1) && !pages.begin(Choice::Back));
  assert(pages.begin(Choice::Training) && pages.measuring());
  assert(!pages.requestSwitch(true,false) && pages.user==1);
  assert(pages.requestSwitch(true,true) && pages.page==Page::ConfirmSwitch && pages.measuring());
  pages.finishSwitch(true,false); // motion resumed before confirmation
  assert(pages.page==Page::Workout && pages.user==1);
  assert(pages.requestSwitch(true,true));
  pages.finishSwitch(true,true);
  assert(pages.page==Page::Users && !pages.user && !pages.measuring());
  assert(pages.selectUser(1) && pages.begin(Choice::Ghost) && pages.user==2);
  assert(pages.requestSwitch(false,false) && !pages.user);
}

int main() {
  navigationTest();
  using Read=UserRecordStorage::Read;
  auto legacy=workout(0,250000);
  legacy.version=3;
  legacy.intervalSeconds=0;
  legacy.checksum=ghostrace::hashRecord(legacy);
  const auto *legacyBytes=reinterpret_cast<const uint8_t*>(&legacy);
  const Blob original(legacyBytes,legacyBytes+sizeof(legacy));
  flash["rower-races"].blobs["g7_1000"]=original;
  flash["rower-races"].blobs["g7_500"]={1,2,3};

  UserRecordStorage store;
  ghostrace::Record best;
  uint32_t wins;
  assert(store.loadBest(0,best,700)==Read::Error);
  assert(!store.saveBest(0,legacy,700));
  store.select(1);
  assert(store.loadBest(0,best,700)==Read::Found && best.finishMs==250000);
  assert(store.loadBest(0,best,701)==Read::OtherCalibration);
  assert(store.awardWin(0,wins) && wins==1);

  for(uint8_t user=2;user<=4;++user) {
    store.select(user);
    for(uint8_t distance=0;distance<3;++distance) {
      assert(store.loadBest(distance,best,700)==Read::Missing && best.finishMs==0);
      const auto run=workout(distance,255000+10000*user+140000*distance);
      ghostrace::HeartRecord heart;
      heart.count=run.count;
      heart.reserved=run.intervalSeconds;
      heart.raceChecksum=run.checksum;
      heart.bpm[0]=115+user;
      heart.checksum=ghostrace::hashHeart(heart);
      assert(store.saveBest(distance,run,700));
      assert(store.saveHeart(distance,heart,run));
      for(uint8_t count=0;count<user;++count)
        assert(store.awardWin(distance,wins) && wins==count+1U);
    }
  }

  UserRecordStorage restarted;
  for(uint8_t user=2;user<=4;++user) {
    restarted.select(user);
    for(uint8_t distance=0;distance<3;++distance) {
      assert(restarted.loadBest(distance,best,700)==Read::Found);
      assert(best.finishMs==255000U+10000U*user+140000U*distance);
      ghostrace::Race replay;
      replay.arm(ghostrace::kDistances[distance],&best);
      assert(replay.hasBest && replay.best.finishMs==best.finishMs);
      ghostrace::HeartRecord heart;
      assert(restarted.loadHeart(distance,heart,best)==Read::Found && heart.bpm[0]==115+user);
      assert(restarted.loadWins(distance,wins)==Read::Found && wins==user);
      auto unrelated=best;
      ++unrelated.checksum;
      assert(restarted.loadHeart(distance,heart,unrelated)==Read::Missing);
    }
  }

  restarted.select(2);
  failWrite=true;
  assert(!restarted.saveBest(0,workout(0,230000),700));
  assert(!restarted.awardWin(0,wins));
  failWrite=false;
  assert(restarted.loadBest(0,best,700)==Read::Found && best.finishMs==275000);
  assert(restarted.loadWins(0,wins)==Read::Found && wins==2);
  failRead=true;
  assert(restarted.loadBest(0,best,700)==Read::Error);
  failRead=false;

  restarted.select(1);
  assert(restarted.loadBest(0,best,700)==Read::Found && best.finishMs==250000);
  assert(restarted.loadWins(0,wins)==Read::Found && wins==1);
  assert(flash.at("rower-races").blobs.at("g7_1000")==original);
  assert((flash.at("rower-races").blobs.at("g7_500")==Blob{1,2,3}));
}
