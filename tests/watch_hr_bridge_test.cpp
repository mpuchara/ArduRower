#include <assert.h>
#include <stdio.h>
#include "../cyd_rower/src/WatchHrBridge.h"
int main(){
  watchhr::Receiver r;uint16_t bpm=999;
  uint8_t p[]={1,1,254,255,95,0};
  assert(r.accept(p,6,bpm) && bpm==95);
  assert(!r.accept(p,6,bpm)); // repeated ATT delivery cannot refresh HR
  p[2]=255;p[4]=96;assert(r.accept(p,6,bpm) && bpm==96);
  p[2]=0;p[3]=0;p[4]=97;assert(r.accept(p,6,bpm) && bpm==97);
  p[2]=255;p[3]=255;assert(!r.accept(p,6,bpm)); // old sequence across wrap
  p[2]=1;p[3]=0;p[4]=0;p[1]=0;assert(r.accept(p,6,bpm) && bpm==0);
  p[2]=2;p[1]=1;assert(!r.accept(p,6,bpm)); // invalid live value
  p[4]=241;assert(!r.accept(p,6,bpm));
  p[4]=35;p[0]=2;assert(!r.accept(p,6,bpm));
  p[0]=1;p[1]=2;assert(!r.accept(p,6,bpm));
  p[1]=1;assert(!r.accept(p,5,bpm));assert(!r.accept(nullptr,6,bpm));
  assert(r.accept(p,6,bpm) && bpm==35); // rejected inputs did not consume sequence
  r.reset();assert(r.accept(p,6,bpm));
  puts("PASS: watch HR fresh sequence, wrap, duplicates, unknown HR and malformed frames");
}
