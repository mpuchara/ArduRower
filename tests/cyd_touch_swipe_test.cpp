#include "../cyd_rower/src/TouchGestures.h"
#include <stdio.h>
#define CHECK(x) do {if(!(x)){printf("FAIL at %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main() {
  using cydtouch::pageSwipe;
  CHECK(pageSwipe(850,40,false,0,0));    // raw X, right
  CHECK(pageSwipe(-850,-40,false,0,0));  // raw X, left
  CHECK(pageSwipe(35,1000,false,0,0));   // raw Y due 90 degree rotation
  CHECK(pageSwipe(-35,-1000,false,0,0));
  CHECK(pageSwipe(60,20,true,-78,7));   // short calibrated horizontal move
  CHECK(pageSwipe(900,60,true,4,130));  // damaged/rotated calibration fallback
  CHECK(!pageSwipe(200,8,false,0,0));   // too short, avoid accidental page
  CHECK(!pageSwipe(120,120,false,0,0)); // diagonal
  CHECK(!pageSwipe(200,210,true,3,145)); // vertical logical move, diagonal raw
  puts("PASS: Ghost Race swipe in both directions, swapped axes, saved-calibration fallback");
  return 0;
}
