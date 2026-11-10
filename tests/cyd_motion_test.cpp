#include "../cyd_rower/src/CydMotion.h"
#include <stdio.h>
#include <math.h>
using namespace cydmotion;
#define REQUIRE(c) do { if(!(c)) {printf("FAIL line %d: %s\n",__LINE__,#c);return 1;} } while(0)
int main(){
  Engine e;
  e.reset(0);
  REQUIRE(e.strokes==0 && e.meters==0);
  uint32_t now=100000;
  for(int n=0;n<30;n++){
    for(int i=0;i<20;i++){now+=40000;e.onPulse(now);}
    now+=210000;e.onPulse(now); // turnaround to return
    for(int i=0;i<19;i++){now+=55000;e.onPulse(now);}
    now+=280000;e.onPulse(now); // turnaround to drive
  }
  REQUIRE(e.strokes>=28 && e.strokes<=31);
  REQUIRE(e.transitions>=56);
  REQUIRE(e.meters>40 && e.meters<270);
  REQUIRE(e.speed>=0 && e.speed<7.5);
  REQUIRE(e.reportedSpeed>=0 && e.reportedSpeed<7.5);
  printf("30 cycles detected %u; phase transitions %u; distance %.1fm; speed %.2fm/s\n",
        e.strokes,e.transitions,e.meters,e.speed);
  const float before=e.meters;
  now+=6000000;e.tick(now);
  REQUIRE(e.speed==0);
  REQUIRE(e.reportedSpeed==0);
  REQUIRE(e.meters>=before);
  REQUIRE(e.meters<before+3.0f);
  REQUIRE(e.spm==0);
  // A lone pulse after a rest never counts an extra stroke.
  const uint32_t strokeCount=e.strokes;
  e.onPulse(now+25000);
  REQUIRE(e.strokes==strokeCount);
  REQUIRE(e.calibrationMilli()==700);
  e.setCalibration(0.5f);
  REQUIRE(e.calibrationMilli()==500);
  e.setCalibration(-100);
  REQUIRE(e.calibrationMilli()==500);
  // A burst shorter than one phase must not count as multiple strokes.
  e.reset(0);
  for(int i=0;i<5;i++)e.onPulse(100000+i*40000);
  REQUIRE(e.strokes==0);
  // Synthetic pulses keep the algorithm working across 32-bit micros wrap.
  e.reset(0xfffffff0UL);
  e.onPulse(0x0000c350UL);
  REQUIRE(e.pulses==1 && isfinite(e.speed) && isfinite(e.reportedSpeed));
  // Alternating 11/3 km/h raw readings each second must yield a much less
  // jagged reported reading; this must not change the stroke detector.
  e.reset(0);
  e.onPulse(100000);
  uint32_t wobbleUs=100000;
  float filteredMin=100.0f,filteredMax=0.0f;
  for(int i=0;i<400;++i) {
    wobbleUs+=100000;
    e.lastPulseUs=wobbleUs; // keep this test focused on the speed filter
    e.speed=((i/10)%2==0?11.0f:3.0f)/3.6f;
    e.tick(wobbleUs);
    if(i>=200) {
      if(e.reportedSpeed<filteredMin)filteredMin=e.reportedSpeed;
      if(e.reportedSpeed>filteredMax)filteredMax=e.reportedSpeed;
    }
  }
  REQUIRE((filteredMax-filteredMin)*3.6f<2.0f);
  REQUIRE(e.strokes==0);
  e.tick(wobbleUs+6000000);
  REQUIRE(e.reportedSpeed==0);
  // Short 50-70ms reversals during fast strokes were previously rejected
  // by the fixed 105ms / 320ms gates; this caused ~half the strokes to vanish.
  e.reset(0);
  now=100000;
  for(int n=0;n<30;++n) {
    for(int i=0;i<19;++i) {now+=26000;e.onPulse(now);}
    now+=55000;e.onPulse(now);
    for(int i=0;i<17;++i) {now+=33000;e.onPulse(now);}
    now+=70000;e.onPulse(now);
  }
  REQUIRE(e.strokes>=28 && e.strokes<=31);
  REQUIRE(e.transitions>=56);
  REQUIRE(e.spm>=45 && e.spm<=60);
  REQUIRE(e.meters>0);
  printf("Fast 51 SPM: %u strokes, %u phase transitions, %.1f SPM, %.1fm\\n",
         e.strokes,e.transitions,e.spm,e.meters);
  // Sprint cadence above 65 SPM must no longer freeze cadence reporting.
  e.reset(0);
  now=100000;
  for(int n=0;n<30;++n) {
    for(int i=0;i<12;++i) {now+=25000;e.onPulse(now);}
    now+=50000;e.onPulse(now);
    for(int i=0;i<12;++i) {now+=30000;e.onPulse(now);}
    now+=65000;e.onPulse(now);
  }
  REQUIRE(e.strokes>=28 && e.strokes<=31);
  REQUIRE(e.spm>=65 && e.spm<=90);
  // Four unevenly spaced magnets must NOT create artificial strokes.
  e.reset(0);
  now=100000;
  for(int n=0;n<150;++n) {
    const uint32_t gap=(n%4==3)?46000:25000;
    now+=gap;e.onPulse(now);
  }
  REQUIRE(e.strokes==0 && e.transitions==0);
  puts("PASS: slow & fast turning, 65+ SPM sprint, uneven magnets, smooth speed, pause, wrap");
  return 0;
}
