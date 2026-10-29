#include "../cyd_rower/src/TouchPress.h"
#include <cassert>
#include <cstdio>
using cydtouch::PressTracker;
using cydtouch::Event;
int main() {
  PressTracker t;
  assert(t.sample(0,false,0,0)==Event::None);
  // 0.09.10 physical trace: pressure never becomes low after initial down.
  assert(t.sample(30,true,1961,965)==Event::Down);
  assert(t.sample(70,true,1980,800)==Event::Move);
  assert(t.sample(120,true,2150,400)==Event::Move);
  t.consume(120,2150,400); // first swipe enters Ghost
  assert(t.sample(220,true,2200,350)==Event::None); // no double-toggle
  assert(t.sample(350,true,2200,350)==Event::None);
  // Physical tap in the 1000m row, no UP signal at all.
  assert(t.sample(530,true,2050,1100)==Event::Down);
  assert(t.sample(566,true,2052,1102)==Event::Move);
  assert(t.sample(600,true,2050,1101)==Event::Move);
  assert(t.sample(640,true,2051,1102)==Event::Move);
  assert(t.sample(700,true,2051,1102)==Event::Tap);
  t.consume(700,2051,1102);
  assert(t.sample(800,true,2051,1102)==Event::None);
  assert(t.sample(980,true,2051,1102)==Event::None);
  // A new distinct gesture can leave Ghost even with sticky pressure.
  assert(t.sample(1080,true,2300,1600)==Event::Down);
  assert(t.sample(1200,true,2250,1150)==Event::Move);
  t.consume(1200,2250,1150);
  assert(t.sample(1210,true,2250,1150)==Event::None);
  // Normal pressure release still works.
  assert(t.sample(1400,false,0,0)==Event::None);
  assert(t.sample(1420,false,0,0)==Event::None);
  assert(t.sample(1500,true,1300,1600)==Event::Down);
  assert(t.sample(2750,true,1300,1600)==Event::Move);
  assert(t.sample(3800,true,1300,1600)==Event::Recovery);
  assert(t.sample(3900,true,1300,1600)==Event::None);
  std::puts("PASS: v0.09.10 sticky Z trace: swipe, tap without UP, subsequent swipe, recovery");
}
