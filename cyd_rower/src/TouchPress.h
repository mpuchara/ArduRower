#pragma once
// Gesture events from the CYD XPT2046. On this board the unplugged/released
// panel can continue to report Z~=3600 and IRQ=0. A release-based-only
// tap recognizer therefore never fires. A stationary press may commit a tap
// without UP; completed gestures are then re-armed by a new spatial contact.
#include <stdint.h>
#include <stdlib.h>
namespace cydtouch {
enum class Event : uint8_t { None, Down, Move, Tap, Up, Recovery };
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
      // Do not turn motion from the preceding swipe into a second swipe.
      if((uint32_t)(now-suppressedAt_)<350 ||
         (abs((int)x-(int)suppressedX_)<185 &&
          abs((int)y-(int)suppressedY_)<185))return Event::None;
      suppressed_=false;
    }
    if(!active_) {
      active_=true;tapEmitted_=false;pressedAt_=lastMovementAt_=now;
      startX_=previousX_=x;startY_=previousY_=y;travel_=0;
      samples_=0;
      return Event::Down;
    }
    if(samples_<255)++samples_;
    const int mx=abs((int)x-(int)previousX_);
    const int my=abs((int)y-(int)previousY_);
    travel_+=(uint32_t)(mx+my);
    if(mx>16 || my>16) lastMovementAt_=now;
    previousX_=x;previousY_=y;
    const int dx=abs((int)x-(int)startX_);
    const int dy=abs((int)y-(int)startY_);
    const uint32_t elapsed=now-pressedAt_;
    // A deliberate stationary tap, not a swipe, is handled after 160ms
    // even if the touch controller never indicates pressure release.
    if(!tapEmitted_ && samples_>=4 && elapsed>=160 && elapsed<900 &&
       dx<=175 && dy<=175 && travel_<=650) {
      tapEmitted_=true;
      return Event::Tap;
    }
    if(elapsed>2200 && (uint32_t)(now-lastMovementAt_)>950) {
      consume(now,x,y);
      return Event::Recovery; // never synthesize a tap on a timeout
    }
    return Event::Move;
  }
  void consume(uint32_t now,int16_t x,int16_t y) {
    active_=false;suppressed_=true;
    suppressedAt_=now;suppressedX_=x;suppressedY_=y;
  }
 private:
  bool active_=false,suppressed_=false,tapEmitted_=false;
  uint8_t offSamples_=0,samples_=0;
  uint32_t pressedAt_=0,lastMovementAt_=0,suppressedAt_=0;
  uint32_t travel_=0;
  int16_t startX_=0,startY_=0,previousX_=0,previousY_=0;
  int16_t suppressedX_=0,suppressedY_=0;
};
} // namespace cydtouch
