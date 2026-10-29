#pragma once
// Pressure-based XPT2046 gesture tracker, independent of Arduino and IRQ pins.
#include <stdint.h>
#include <stdlib.h>
namespace cydtouch {
enum class Event : uint8_t { None, Down, Move, Up, Recovery };
class PressTracker {
 public:
  Event sample(uint32_t now,bool pressure,int16_t x,int16_t y) {
    if(!pressure) {
      if(offSamples_<3)++offSamples_;
      if(offSamples_<2)return Event::None;
      suppressed_=false;
      if(active_) {active_=false;return Event::Up;}
      return Event::None;
    }
    offSamples_=0;
    if(suppressed_) {
      // Ignore stale coordinates after a stuck contact; accept a new position.
      if(abs((int)x-(int)suppressedX_)<160 &&
         abs((int)y-(int)suppressedY_)<160)return Event::None;
      suppressed_=false;
    }
    if(!active_) {
      active_=true;pressedAt_=lastMovementAt_=now;
      previousX_=x;previousY_=y;
      return Event::Down;
    }
    if(abs((int)x-(int)previousX_)>16 ||
       abs((int)y-(int)previousY_)>16) {
      lastMovementAt_=now;previousX_=x;previousY_=y;
    }
    if((uint32_t)(now-pressedAt_)>2200 &&
       (uint32_t)(now-lastMovementAt_)>950) {
      active_=false;suppressed_=true;
      suppressedX_=x;suppressedY_=y;
      return Event::Recovery; // NEVER synthesize a tap or page transition
    }
    return Event::Move;
  }
 private:
  bool active_=false,suppressed_=false;
  uint8_t offSamples_=0;
  uint32_t pressedAt_=0,lastMovementAt_=0;
  int16_t previousX_=0,previousY_=0,suppressedX_=0,suppressedY_=0;
};
} // namespace cydtouch
