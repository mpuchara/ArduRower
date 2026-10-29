#include "../cyd_rower/src/BleDutySchedule.h"
#include <stdio.h>
using namespace cydble;
#define CHECK(x) do {if(!(x)){printf("FAIL %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(){
  DutySchedule s;
  CHECK(s.canStart(20000));
  s.onFtms(true,20000);
  CHECK(!s.canStart(26000)); // no BLE HR discovery under FTMS, even idle
  CHECK(!s.canStart(28500));
  s.onMotion(30000);
  CHECK(!s.canStart(100000)); // FTMS still connected after rowing
  s.onFtms(false,130100);
  CHECK(s.canStart(130200)); // no active link, motion idle, first attempt
  s.begin(130200);
  CHECK(!s.canStart(130201)); // no overlapping HR setups
  // If watch connects to FTMS during HR discovery, abort setup.
  s.onFtms(true,131000);
  CHECK(s.shouldYield(131001));
  s.end();
  s.onFtms(false,132000);
  CHECK(!s.canStart(200000)); // cooldown since last active HR attempt
  CHECK(s.canStart(221000));  // 90+ second retry period
  s.begin(221000);
  // A FTMS link drop during an *ongoing* HR setup triggers a circuit breaker.
  s.onFtms(true,222000);
  s.onFtms(false,223000);
  CHECK(s.collisionGuard);
  CHECK(s.shouldYield(223001));
  s.end();
  CHECK(!s.canStart(350000));
  CHECK(s.canStart(404000));
  s.begin(404000);
  s.end();
  CHECK(!s.canStart(404001));
  CHECK(s.canStart(494000));
  DutySchedule quiet;
  quiet.onMotion(0xfffff000UL);
  CHECK(!quiet.canStart(0x00001000UL));
  CHECK(quiet.canStart(0x00040000UL));
  puts("PASS: strict FTMS-first HR maintenance (offline only), rowing idle, retry and link collision guard");
  return 0;
}
