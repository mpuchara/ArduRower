#include "../cyd_rower/src/BleCoexistPolicy.h"
#include <cassert>
#include <cstdio>
int main() {
  using cydble::CoexistPolicy;
  assert(!CoexistPolicy::shouldYieldHrForFtms(20000,1000,true,false));
  assert(!CoexistPolicy::shouldYieldHrForFtms(40000,1000,true,true));
  assert(CoexistPolicy::shouldYieldHrForFtms(31000,1000,true,false));
  assert(!CoexistPolicy::shouldYieldHrForFtms(31000,1000,false,false));
  assert(CoexistPolicy::noCurrentSessionBpm(50001,1000,12,12));
  assert(!CoexistPolicy::noCurrentSessionBpm(50001,1000,13,12));
  assert(CoexistPolicy::inRetryCooldown(30000,130000));
  assert(!CoexistPolicy::inRetryCooldown(130000,130000));
  assert(CoexistPolicy::ftmsCollisionPossible(2020,1000,true));
  assert(!CoexistPolicy::ftmsCollisionPossible(30000,1000,true));
  assert(!CoexistPolicy::ftmsCollisionPossible(2020,1000,false));
  assert(!CoexistPolicy::shouldYieldHrForFtms(25000,0xfffffff0,true,false));
  std::puts("PASS: HR-first join, FTMS grace, retries, per-ACL BPM and collision guard");
}
