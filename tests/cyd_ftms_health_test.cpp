#include "../cyd_rower/src/FtmsHealth.h"
#include <cassert>
#include <cstdio>
using cydble::FtmsHealth;
int main() {
  assert(!FtmsHealth::streamReady(true,false,10));
  assert(!FtmsHealth::streamReady(false,true,10));
  assert(!FtmsHealth::streamReady(true,true,0));
  assert(FtmsHealth::streamReady(true,true,10));
  assert(!FtmsHealth::canTryHr(20000,true,true,1000,10,true));
  assert(!FtmsHealth::canTryHr(18999,true,true,1000,10,true));
  assert(FtmsHealth::canTryHr(19000,true,true,1000,10,true));
  assert(!FtmsHealth::canTryHr(19002,true,true,1000,9,true));
  assert(!FtmsHealth::canTryHr(19002,true,true,1000,10,false));
  assert(!FtmsHealth::canTryHr(19002,true,false,1000,10,true));
  assert(!FtmsHealth::canTryHr(19002,false,true,1000,10,true));
  assert(!FtmsHealth::canTryHr(19002,true,true,0,10,true));
  assert(!FtmsHealth::retryUnsubscribed(19999,true,false,1000,0));
  assert(FtmsHealth::retryUnsubscribed(21000,true,false,1000,0));
  assert(!FtmsHealth::retryUnsubscribed(21000,true,true,1000,0));
  assert(!FtmsHealth::retryUnsubscribed(21000,false,false,1000,0));
  assert(!FtmsHealth::retryUnsubscribed(21000,true,false,1000,2));
  assert(!FtmsHealth::mustYieldHr(true,true));
  assert(FtmsHealth::mustYieldHr(true,false));
  assert(FtmsHealth::mustYieldHr(false,true));
  assert(FtmsHealth::canTryHr(0x1000,true,true,0xffffb000,15,true));
  puts("PASS: BLE ACL != FTMS subscribed; HR starts only after stable FTMS TX");
  return 0;
}
