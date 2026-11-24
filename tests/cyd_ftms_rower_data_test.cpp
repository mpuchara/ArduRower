#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "../cyd_rower/src/FtmsRowerData.h"

int main() {
  using namespace ftmsrower;
  uint8_t packet[kMaxPacketSize];
  Sample run={30.0f,15,28.0f,500.5f,120.2f,129.6f,145.2f,93.7f,60.2f};
  const uint8_t legacy[]={0x1e,0x08,60,15,0,56,0xf4,1,0,120,0,130,0,60,0};
  const uint8_t withPower[]={0x7e,0x08,60,15,0,56,0xf4,1,0,120,0,130,0,145,0,94,0,60,0};
  assert(kFeatureByte0==0x26 && featureByte1(false)==0x10 && featureByte1(true)==0x50);
  assert(encode(run,false,packet)==sizeof(legacy) && !memcmp(legacy,packet,sizeof(legacy)));
  assert(encode(run,true,packet)==sizeof(withPower) && !memcmp(withPower,packet,sizeof(withPower)));

  const Sample idle={0,0,0,0,0,0,0,NAN,0};
  const uint8_t idleExpected[]={0x3e,0x08,0,0,0,0,0,0,0,0xff,0xff,0xff,0xff,0,0,0,0};
  assert(encode(idle,true,packet)==sizeof(idleExpected));
  assert(!memcmp(idleExpected,packet,sizeof(idleExpected)));

  const Sample missing={NAN,0,NAN,NAN,NAN,-1,NAN,NAN,NAN};
  const uint8_t missingExpected[]={0x1e,0x08,0xff,0,0,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff};
  assert(encode(missing,true,packet)==sizeof(missingExpected));
  assert(!memcmp(missingExpected,packet,sizeof(missingExpected)));

  const Sample averageOnly={1.5f,0xfffe,2,0xfffffe,0.3f,72000,INFINITY,321,0xfffe};
  const uint8_t averageExpected[]={0x5e,0x08,3,0xfe,0xff,4,0xfe,0xff,0xff,1,0,
                                    0xfe,0xff,0x41,1,0xfe,0xff};
  assert(encode(averageOnly,true,packet)==sizeof(averageExpected));
  assert(!memcmp(averageExpected,packet,sizeof(averageExpected)));
  run.power=65534;
  assert(encode(run,true,packet)==19 && packet[13]==0xff && packet[14]==0x7f);
  puts("PASS: FTMS power/capabilities, legacy frame, zero, missing values and limits");
}
