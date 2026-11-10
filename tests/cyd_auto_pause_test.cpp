#include "../cyd_rower/src/CydMotion.h"
#include "../cyd_rower/src/GhostRace.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
int main(){
  cydmotion::PauseResumeGate resume;
  uint32_t first=0;
  assert(!resume.confirm(1000000,first));
  assert(!resume.confirm(2600000,first)); // isolated edge after a stop
  assert(resume.confirm(2680000,first) && first==2600000);
  assert(!resume.confirm(2690000,first)); // requires a fresh pair
  resume.reset();
  assert(!resume.confirm(0xfffffff0UL,first));
  assert(resume.confirm(100000,first) && first==0xfffffff0UL);
  ghostrace::Race reference;reference.arm(500,nullptr);reference.start(0);
  for(int i=1;i<=125;++i)reference.advance(i*1000,i*4.0f);
  RowerMetrics m(cydmotion::Engine::kPauseAfterUs);cydmotion::Engine movement;ghostrace::Race race;
  m.reset(0);movement.reset(0);race.arm(500,&reference.attempt);
  uint32_t t=100000;
  auto sync=[&](){m.tick(t);movement.tick(t);m.setExternalTelemetry(movement.meters,movement.speed,movement.strokes,movement.spm);
    if(race.phase==ghostrace::Phase::Armed && m.started)race.start((uint32_t)(m.activeUs/1000));
    race.advance((uint32_t)(m.activeUs/1000),m.distance());};
  for(int i=0;i<100;++i){m.pulse(t);movement.onPulse(t);sync();t+=40000;}
  assert(m.started && !m.paused && m.distance()>0);
  // The eight-second coast belongs to training, without adding distance.
  const float beforeCoast=m.distance();
  const uint32_t lastPulse=t-40000;
  t=lastPulse+7999999;sync();
  assert(!m.paused && movement.speed>0 && m.distance()==beforeCoast);
  ++t;sync();
  assert(m.paused && m.speed==0 && m.strokeRate==0 && m.pace()==0);
  const uint64_t active=m.activeUs;const float meters=m.distance();
  const float opponent=race.ghostMeters((uint32_t)(active/1000));
  const uint16_t samples=race.attempt.count;
  for(int i=0;i<300;++i){t+=100000;sync();}
  assert(m.activeUs==active && m.distance()==meters);
  assert(race.attempt.count==samples);
  assert(race.ghostMeters((uint32_t)(m.activeUs/1000))==opponent);
  // First valid resumed pulse wakes training without counting the rest.
  m.pulse(t);movement.onPulse(t);sync();
  assert(!m.paused && m.activeUs==active);
  t+=100000;sync();assert(m.activeUs==active+100000);
  // Delayed timestamps must not produce phantom elapsed time.
  const uint64_t before=m.activeUs;m.tick(t-1000);assert(m.activeUs==before);
  m.pulse(t-500);m.tick(t+1000);assert(m.activeUs<=before+1000);
  // External movement cannot overwrite an already-paused speed/cadence.
  t+=9000000;sync();assert(m.paused);
  m.setExternalTelemetry(m.distance(),5.0f,4,30);assert(m.speed==0 && m.strokeRate==0);
  puts("PASS: CYD coast, automatic pause, frozen ghost/distance, resume and delayed samples");
}
