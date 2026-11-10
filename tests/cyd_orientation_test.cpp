#include "../cyd_rower/src/CydOrientation.h"
#include <math.h>
#include <stdio.h>
#define REQUIRE(c) do { if(!(c)) {printf("FAIL line %d: %s\n",__LINE__,#c);return 1;} } while(0)
int main() {
  using namespace cydview;
  REQUIRE(kDefaultRotation==1);
  REQUIRE(checkedRotation(1)==1);
  REQUIRE(checkedRotation(3)==3);
  REQUIRE(checkedRotation(42)==1);
  REQUIRE(toggleRotation(1)==3 && toggleRotation(3)==1);
  float x=25,y=65;
  mapTouchToDisplay(x,y,3,1); // previous calibration for old rotation
  REQUIRE(fabsf(x-294)<0.01f && fabsf(y-174)<0.01f);
  mapTouchToDisplay(x,y,3,1);
  REQUIRE(fabsf(x-25)<0.01f && fabsf(y-65)<0.01f);
  mapTouchToDisplay(x,y,1,1);
  REQUIRE(fabsf(x-25)<0.01f && fabsf(y-65)<0.01f);
  mapTouchToDisplay(x,y,1,3);
  REQUIRE(fabsf(x-294)<0.01f && fabsf(y-174)<0.01f);
  puts("PASS: CYD landscape rotate 180, preserved touch coordinates and invalid NVS value");
  return 0;
}
