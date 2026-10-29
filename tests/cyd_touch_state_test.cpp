#include "../cyd_rower/src/TouchPress.h"
#include <cassert>
#include <cstdio>
using cydtouch::PressTracker;
using cydtouch::Event;
int main() {
  PressTracker t;
  assert(t.sample(0,false,0,0)==Event::None);
  assert(t.sample(30,true,1000,2000)==Event::Down);
  assert(t.sample(80,true,1600,2100)==Event::Move);
  assert(t.sample(125,false,1600,2100)==Event::None);
  assert(t.sample(140,false,1600,2100)==Event::Up);
  assert(t.sample(160,true,1200,2300)==Event::Down);
  assert(t.sample(190,true,1200,2300)==Event::Move);
  assert(t.sample(250,false,0,0)==Event::None);
  assert(t.sample(265,false,0,0)==Event::Up);
  assert(t.sample(400,true,1300,1600)==Event::Down);
  assert(t.sample(2700,true,1300,1600)==Event::Recovery);
  assert(t.sample(2750,true,1300,1600)==Event::None);
  assert(t.sample(2800,true,2000,2000)==Event::Down);
  assert(t.sample(2860,false,0,0)==Event::None);
  assert(t.sample(2880,false,0,0)==Event::Up);
  assert(t.sample(2900,true,2000,2000)==Event::Down);
  assert(t.sample(2980,false,0,0)==Event::None);
  assert(t.sample(3000,false,0,0)==Event::Up);
  std::puts("PASS: touch lifecycle, consecutive taps, stuck contact recovery");
}
