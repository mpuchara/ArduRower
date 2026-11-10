#include "../cyd_rower/src/TrendAutoScale.h"
#include <stdio.h>
using cydplot::AutoScale;
#define REQUIRE(c) do { if(!(c)) {printf("FAIL line %d: %s\n",__LINE__,#c);return 1;} } while(0)
int main() {
  AutoScale empty;
  empty.finish();
  REQUIRE(empty.speedMaxTenth==10);
  REQUIRE(!empty.hasHeartRate);
  REQUIRE(empty.speedY(0,54)==53);
  REQUIRE(empty.speedY(1000,54)==0);
  REQUIRE(empty.heartY(0,54)==53);

  AutoScale window;
  window.includeSpeed(30);
  window.includeHeart(0); // stale HR must not force a zero minimum
  window.includeHeart(96);
  window.includeHeart(104);
  window.includeSpeed(113); // peak 11.3 km/h
  window.finish();
  REQUIRE(window.speedMaxTenth==120); // visible max -> 12 km/h top tick
  REQUIRE(window.heartMin==80 && window.heartMax==110);
  REQUIRE(window.hasHeartRate);
  REQUIRE(window.speedY(113,54)>0 && window.speedY(113,54)<53);
  REQUIRE(window.heartY(104,54)>0 && window.heartY(104,54)<53);

  AutoScale faster;
  faster.includeSpeed(221); // no fixed 22 km/h cap
  faster.includeHeart(51); // low but valid pulse
  faster.includeHeart(205); // no fixed 190 BPM cap
  faster.finish();
  REQUIRE(faster.speedMaxTenth==230);
  REQUIRE(faster.heartMin==40 && faster.heartMax==210);
  REQUIRE(faster.speedY(221,28)>=0 && faster.speedY(221,28)<27);
  REQUIRE(faster.heartY(205,28)>=0 && faster.heartY(205,28)<27);

  AutoScale ghost;
  ghost.includeSpeed(70);
  ghost.includeSpeed(145); // ghost faster than actual athlete
  ghost.includeHeart(120);
  ghost.finish();
  REQUIRE(ghost.speedMaxTenth==150);
  REQUIRE(ghost.heartMin==110 && ghost.heartMax==130);
  REQUIRE(ghost.speedY(145,54)<ghost.speedY(70,54));

  AutoScale rolling;
  rolling.includeSpeed(32); // old 22km/h sample no longer in visible buffer
  rolling.includeHeart(77);
  rolling.finish();
  REQUIRE(rolling.speedMaxTenth==40);
  REQUIRE(rolling.heartMin==60 && rolling.heartMax==80);

  AutoScale maxValues;
  maxValues.includeSpeed(600);
  maxValues.includeHeart(255);
  maxValues.finish();
  REQUIRE(maxValues.speedMaxTenth==610);
  REQUIRE(maxValues.heartMax==260);
  REQUIRE(maxValues.heartY(255,54)>=0);
  puts("PASS: speed/HR auto axes, missing HR, ghost peak, rolling window, empty chart, extremes");
  return 0;
}
