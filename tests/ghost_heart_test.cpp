#include "../cyd_rower/src/GhostHeart.h"
#include <cassert>
#include <cstdio>
int main(){
  ghostrace::Race race;
  race.arm(1000,nullptr);race.start(100);
  race.advance(2100,50);race.advance(4100,100);
  assert(race.advance(20100,1000));
  ghostrace::HeartRecord heart={};
  heart.count=race.attempt.count;heart.raceChecksum=race.attempt.checksum;
  heart.reserved=race.attempt.intervalSeconds;
  heart.bpm[0]=75;heart.bpm[1]=0;heart.bpm[2]=120;
  heart.checksum=ghostrace::hashHeart(heart);
  assert(ghostrace::validHeart(heart,race.attempt));
  assert(ghostrace::replayHeart(heart,1999)==75);
  assert(ghostrace::replayHeart(heart,2000)==0);
  assert(ghostrace::replayHeart(heart,4000)==120);
  assert(ghostrace::replayHeart(heart,999999)==0);
  auto damaged=heart;damaged.bpm[2]^=1;
  assert(!ghostrace::validHeart(damaged,race.attempt));
  auto other=race.attempt;other.checksum^=1;
  assert(!ghostrace::validHeart(heart,other));
  heart.count=ghostrace::kSamples+1;heart.checksum=ghostrace::hashHeart(heart);
  assert(!ghostrace::validHeart(heart,race.attempt));
  // 10 km uses a longer interval and adaptive decimation of HR.
  ghostrace::Race longRace;longRace.arm(10000,nullptr);longRace.start(0);
  ghostrace::HeartRecord hr={};
  ghostrace::captureHeart(hr,longRace.attempt,75);
  for(uint32_t ms=10000;ms<=12000000;ms+=10000) {
    longRace.advance(ms,10000.0f*(ms/12000000.0f));
    ghostrace::captureHeart(hr,longRace.attempt,ms==20000?130:100);
  }
  hr.raceChecksum=longRace.attempt.checksum;hr.checksum=ghostrace::hashHeart(hr);
  assert(ghostrace::validHeart(hr,longRace.attempt));
  assert(hr.reserved==20&&ghostrace::replayHeart(hr,20000)==130);
  assert(ghostrace::replayHeart(hr,0)==75);
  std::puts("PASS: ghost HR replay, missing BPM, long race decimation and torn data");
}
