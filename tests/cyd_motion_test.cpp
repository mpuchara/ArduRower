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
  printf("30 cycles detected %u; phase transitions %u; distance %.1fm; speed %.2fm/s\n",
        e.strokes,e.transitions,e.meters,e.speed);
  const float before=e.meters;
  now+=6000000;e.tick(now);
  REQUIRE(e.speed==0);
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
  REQUIRE(e.pulses==1 && isfinite(e.speed));
  puts("PASS: two turnaround phases, 30 real strokes, flywheel decay, calibration, bounce, micros wrap");
  return 0;
}
