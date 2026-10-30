#include "../cyd_rower/src/GhostHeart.h"
#include <cassert>
#include <cstdio>
int main() {
  ghostrace::Race race;
  race.arm(500,nullptr);race.start(100);
  race.advance(2100,50);race.advance(4100,100);
  assert(race.advance(20100,500));
  ghostrace::HeartRecord heart={};
  heart.count=race.attempt.count;heart.raceChecksum=race.attempt.checksum;
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
  std::puts("PASS: ghost HR replay, missing BPM gaps, torn data and different best rejection");
}
