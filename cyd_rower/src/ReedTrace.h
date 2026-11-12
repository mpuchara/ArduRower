#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>

// Bounded, nonblocking serial output. Acquisition never waits for USB/UART.
class ReedTrace {
 public:
  uint32_t dropped=0;
  void clear() { head=tail=0; dropped=0; }
  void line(const char *data, size_t size) {
    if(size>127) { ++dropped; return; }
    const size_t used=(head+sizeof(buffer)-tail)%sizeof(buffer);
    if(size>=sizeof(buffer)-used) { ++dropped; return; }
    for(size_t i=0;i<size;++i) {
      buffer[head]=data[i];
      head=(head+1)%sizeof(buffer);
    }
  }

  template<class SerialPort>
  void flush(SerialPort &port) {
    const int available=port.availableForWrite();
    if(available<=0 || head==tail)return;
    size_t size=0;
    size_t cursor=tail;
    while(cursor!=head && size<128) {
      const char c=buffer[cursor];
      cursor=(cursor+1)%sizeof(buffer);
      ++size;
      if(c=='\n')break;
    }
    if(!size || buffer[(tail+size-1)%sizeof(buffer)]!='\n' ||
       size>static_cast<size_t>(available))return;
    char line[128];
    for(size_t i=0;i<size;++i)line[i]=buffer[(tail+i)%sizeof(buffer)];
    const size_t written=port.write(reinterpret_cast<const uint8_t*>(line),size);
    tail=(tail+written)%sizeof(buffer);
  }

 private:
  char buffer[8192]={};
  size_t head=0, tail=0;
};
