#include "../cyd_rower/src/GhostRace.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
using namespace ghostrace;
static bool near(float a,float b,float eps=1.0f) {
  return fabsf(a-b) < eps;
}
int main() {
  assert(allowedDistance(500) && allowedDistance(1000) && allowedDistance(2000));
  assert(!allowedDistance(750));
  Race first;
  first.arm(500,nullptr);
  assert(!first.hasBest && first.phase==Phase::Armed);
  assert(!first.advance(999,3.0f));
  first.start(1000);
  bool improved=false;
  for(int n=1;n<=125;n++)
    if(first.advance(1000+n*1000, n*4.0f)) improved=true;
  assert(improved && first.personalBest);
  assert(first.phase==Phase::Finished && first.endMs==125000);
  assert(valid(first.attempt,500));
  Record best=first.attempt;
  assert(best.count>=60);
  assert(near(interpolate(best,50000),200.0f,1.0f));
  assert(near(elapsedAtDistance(best,200.0f),50000,1000));
  assert(interpolate(best,999999)==500.0f);
  assert(!valid(best,1000));
  best.decimeters[5]++;
  assert(!valid(best,500)); // corrupted data rejected
  best=first.attempt;
  Race second;
  second.arm(500,&best);
  second.start(0);
  assert(second.hasBest);
  assert(!second.advance(60000,200.0f));
  assert(near(second.ghostMeters(60000),240.0f));
  assert(near(second.meterLead(60000),-40.0f));
  assert(near(second.timeLeadMs(60000),-10000.0f,1000.0f));
  assert(!second.advance(150000,500.0f));
  assert(!second.personalBest && second.phase==Phase::Finished);
  Race winner;
  winner.arm(500,&best);
  winner.start(0);
  assert(winner.advance(99000,500));
  assert(winner.personalBest && valid(winner.attempt,500));
  // Idle intervals must be present in the ghost trajectory.
  Race pause;
  pause.arm(1000,nullptr);
  pause.start(0);
  pause.advance(2000,100);
  pause.advance(4000,100);
  pause.advance(6000,100);
  assert(pause.attempt.decimeters[1]==1000);
  assert(pause.attempt.decimeters[2]==1000);
  assert(pause.attempt.decimeters[3]==1000);
  // Sampling memory bound and long sessions must not overwrite a previous PB.
  Race overflow;
  overflow.arm(2000,nullptr);
  overflow.start(100);
  assert(!overflow.advance(2200000,150.0f));
  assert(overflow.overflow && overflow.phase==Phase::Cancelled);
  Race wrap;
  wrap.arm(500,nullptr);
  wrap.start(0xfffffff0UL);
  assert(!wrap.advance(0x7c0UL,100));
  assert(wrap.elapsed(0x7c0UL)<2500);
  // A canceled run never creates a record or a personal best.
  wrap.cancel();
  assert(wrap.phase==Phase::Cancelled && !wrap.personalBest);
  puts("PASS: ghost best-record integrity, replay, pauses, live lead, overflow, wrap");
  return 0;
}
