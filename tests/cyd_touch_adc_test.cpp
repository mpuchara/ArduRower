#include "../cyd_rower/src/CydTouch.h"
#include <cstdio>
int main() {
  SPIClass spi;CydTouch touch(33,36);assert(touch.begin(spi));
  // A release must be read again even if IRQ never changes. The SPI stub
  // deliberately has no transfer16 API: all conversions are byte explicit.
  spi.conversions={0,4095,0};
  assert(!touch.touched());assert(spi.conversions.empty());
  fakeMillis=20;
  spi.conversions={700,3500,1000,2000,1010,2010,4090,2005,0};
  assert(touch.touched());
  auto point=touch.getPoint();
  assert(point.x==4095-1010 && point.y==2005 && point.z==1295);
  assert(spi.conversions.empty()); // cached within 8 ms
  fakeMillis=40;spi.conversions={0,4095,0};
  assert(!touch.touched());assert(touch.getPoint().z==0);
  std::puts("PASS: byte-order-independent XPT2046 conversion, median, release without IRQ edge");
}
