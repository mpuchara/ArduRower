#pragma once
#include <stdint.h>
#include <cassert>
#include <deque>
static uint32_t fakeMillis=0;
inline uint32_t millis(){return fakeMillis;}
enum {OUTPUT,INPUT,HIGH,LOW,MSBFIRST,SPI_MODE0};
inline void pinMode(uint8_t,int){}
inline void digitalWrite(uint8_t,int){}
inline int digitalRead(uint8_t){return 0;}
inline void delayMicroseconds(int){}
struct SPISettings {SPISettings(int,int,int){}};
struct SPIClass {
  std::deque<uint16_t> conversions;
  int phase=0;uint16_t value=0;
  void beginTransaction(SPISettings){assert(phase==0);}
  void endTransaction(){assert(phase==0);}
  void end(){}
  uint8_t transfer(uint8_t b) {
    if(phase==0){assert(b&0x80);assert(!conversions.empty());
      value=conversions.front()<<3;conversions.pop_front();phase=1;return 0;}
    assert(b==0);
    if(phase++==1)return value>>8;
    phase=0;return value&255;
  }
};
