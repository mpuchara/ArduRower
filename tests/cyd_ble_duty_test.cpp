#include "../cyd_rower/src/BleDutySchedule.h"
#include <stdio.h>
using namespace cydble;
#define CHECK(x) do {if(!(x)){printf("FAIL %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(){
  DutySchedule s;
  CHECK(s.canStart(20000));
  s.onFtms(true,20000);
  CHECK(!s.canStart(26000)); // protect first 8s of FTMS
  CHECK(s.canStart(28500));
  s.begin(28500);
  CHECK(!s.canStart(30000));
  s.end();
  CHECK(!s.canStart(120000)); // one setup per FTMS session even after cooldown
  s.onMotion(120000);
  CHECK(!s.canStart(130000));
  s.onFtms(false,130100);
  CHECK(!s.canStart(134000));
  // Disconnect during HR setup triggers an extended circuit breaker.
  s.onFtms(true,210000);
  s.begin(220000);
  s.onFtms(false,220500);
  CHECK(s.collisionGuard);
  CHECK(s.shouldYield(220501));
  s.end();
  CHECK(!s.canStart(350000));
  CHECK(s.canStart(401000)); // cooldown after FTMS disconnect and no movement
  s.begin(401000);
  s.end();
  CHECK(!s.canStart(401001));
  CHECK(s.canStart(491000));
  DutySchedule quiet;
  quiet.onMotion(0xfffff000UL);
  CHECK(!quiet.canStart(0x00001000UL));
  CHECK(quiet.canStart(0x00040000UL));
  puts("PASS: FTMS-first BLE maintenance windows, rowing quiet time, backoff and disconnect guard");
  return 0;
}
