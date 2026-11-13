#include "../cyd_rower/src/GhostRace.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
using namespace ghostrace;
static bool near(float a,float b,float eps=1.0f){return fabsf(a-b)<eps;}
int main(){
  assert(allowedDistance(1000)&&allowedDistance(3000)&&allowedDistance(10000));
  assert(!allowedDistance(500)&&!allowedDistance(2000));
  Race first;
  first.arm(1000,nullptr);
  assert(!first.hasBest&&first.phase==Phase::Armed);
  assert(!first.advance(999,3.0f));
  first.start(1000);
  bool improved=false;
  for(int n=1;n<=250;++n)if(first.advance(1000+n*1000,n*4.0f))improved=true;
  assert(improved&&first.personalBest&&first.endMs==250000);
  assert(!first.beatGhost()&&valid(first.attempt,1000));
  Record best=first.attempt;
  assert(near(interpolate(best,50000),200.0f));
  assert(near(elapsedAtDistance(best,200.0f),50000,1));
  assert(interpolate(best,999999)==1000);
  assert(!valid(best,3000)&&!valid(best,1000,701));
  Record damaged=best;damaged.decimeters[5]++;
  assert(!valid(damaged,1000));
  Race second;second.arm(1000,&best);second.start(0);
  assert(!second.advance(60000,200.0f));
  assert(near(second.ghostMeters(60000),240));
  assert(near(second.meterLead(60000),-40));
  assert(near(second.timeLeadMs(60000),-10000));
  assert(!second.advance(300000,1000)&&!second.personalBest);
  Race winner;winner.arm(1000,&best);winner.start(0);
  assert(winner.advance(199000,1000)&&winner.beatGhost());
  Race tie;tie.arm(1000,&best);tie.start(0);
  assert(!tie.advance(best.finishMs,1000)&&!tie.beatGhost());
  Record legacy=best;legacy.version=3;legacy.intervalSeconds=0;
  legacy.checksum=hashRecord(legacy);
  assert(valid(legacy,1000)&&near(interpolate(legacy,50000),200));
  legacy.version=2;legacy.checksum=hashRecord(legacy);
  assert(!valid(legacy,1000));
  Race pause;pause.arm(1000,nullptr);pause.start(0);
  pause.advance(2000,100);pause.advance(4000,100);pause.advance(6000,100);
  assert(pause.attempt.decimeters[1]==1000&&pause.attempt.decimeters[3]==1000);
  pause.advance(6000,100);assert(pause.attempt.count==4); // frozen active clock
  Race slow;slow.arm(10000,nullptr);slow.start(0);
  for(uint32_t ms=10000;ms<=12000000;ms+=10000)
    slow.advance(ms,10000.0f*(ms/12000000.0f));
  assert(slow.personalBest&&slow.endMs==12000000&&valid(slow.attempt,10000));
  assert(slow.attempt.intervalSeconds==20);
  assert(near(interpolate(slow.attempt,6000000),5000,0.3f));
  assert(slow.attempt.decimeters[slow.attempt.count-1]==50000);
  // Three simultaneous milestones: never depend on a selected view.
  Race training[kDistanceCount];
  for(unsigned i=0;i<kDistanceCount;++i){training[i].arm(kDistances[i],nullptr);training[i].start(0);}
  for(uint32_t ms=1000;ms<=2500000;ms+=1000)
    for(auto &race:training)race.advance(ms,ms*0.004f);
  assert(training[0].endMs==250000&&training[1].endMs==750000&&training[2].endMs==2500000);
  for(auto &race:training)assert(race.personalBest&&valid(race.attempt,race.targetMeters));
  // Interpolate the crossing instead of counting the full overshooting tick.
  Race crossing;crossing.arm(3000,nullptr);crossing.start(0);
  crossing.advance(700000,2990);
  assert(crossing.advance(702000,3010)&&crossing.endMs==701000);
  assert(valid(crossing.attempt,3000));
  Race wrap;wrap.arm(1000,nullptr);wrap.start(0xfffffff0UL);
  assert(!wrap.advance(0x7c0UL,100)&&wrap.elapsed(0x7c0UL)<2500);
  wrap.cancel();assert(wrap.phase==Phase::Cancelled&&!wrap.personalBest);
  assert(leadColor565(0,100000)==0xffe0);
  assert(leadColor565(5000,100000)==0x07e0);
  assert(leadColor565(-5000,100000)==0xf800);
  const uint16_t ahead=leadColor565(2500,100000),behind=leadColor565(-2500,100000);
  assert(ahead!=0xffe0&&ahead!=0x07e0&&behind!=0xffe0&&behind!=0xf800);
  assert(leadColor565(1000,1000)!=0x07e0);
  puts("PASS: all milestones, legacy 1 km, live lead gradient, long 10 km, active pause, crossing and wrap");
}
