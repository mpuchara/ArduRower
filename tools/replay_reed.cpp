// Build: g++ -std=c++11 -O2 tools/replay_reed.cpp -o replay_reed
// Usage: replay_reed raw|filtered|legacy < reedtrace.log
#ifndef REED_MOTION_HEADER
#define REED_MOTION_HEADER "../cyd_rower/src/CydMotion.h"
#endif
#include REED_MOTION_HEADER
#include "../cyd_rower/src/ReedSignalFilter.h"
#include <stdio.h>
#include <string.h>

int main(int argc,char **argv) {
  if(argc!=2) {
    fprintf(stderr,"Usage: replay_reed raw|filtered|legacy < trace.log\n");
    return 2;
  }
  const bool raw=strcmp(argv[1],"raw")==0;
  const bool filtered=strcmp(argv[1],"filtered")==0;
  const bool legacy=strcmp(argv[1],"legacy")==0;
  if(!raw && !filtered && !legacy)return 2;
  cydmotion::Engine motion;
  cydreed::Filter contact;
  uint32_t first=0,last=0,lastRaw=0,legacyStroke=0,legacyDistance=0;
  bool initialized=false,haveLegacy=false;
  unsigned rawEdges=0,falling=0,accepted=0;
  float peak=0;
  char line[512];
  auto pulse=[&](uint32_t time,bool stroke) {
    if(!initialized) {
      initialized=true;
      motion.reset(time);
      first=time;
    }
#ifdef REED_BASELINE
    motion.onPulse(time,stroke);
#else
    if(legacy)motion.onPulse(time,stroke);
    else motion.onVerifiedPulse(time);
#endif
    if(motion.speed>peak)peak=motion.speed;
    last=time;
    ++accepted;
  };
  while(fgets(line,sizeof(line),stdin)) {
    unsigned time=0,level=0;
    char edge=0;
    if(sscanf(line,"R,%u,%c,%u",&time,&edge,&level)==3) {
      ++rawEdges;
      if(rawEdges==1)contact.reset(time-1000,true);
      lastRaw=time;
      if(!level)++falling;
      if(raw) {
        cydreed::Pulse result;
        if(contact.edge(time,level!=0,result))pulse(result.timeUs,true);
      }
      if(legacy && !level) {
        const bool stroke=!haveLegacy || time-legacyStroke>=15000;
        const bool distance=!haveLegacy || time-legacyDistance>=10000;
        if(stroke)legacyStroke=time;
        if(distance)legacyDistance=time;
        if(stroke || distance)pulse(time,stroke);
        haveLegacy=true;
      }
    } else if(filtered && sscanf(line,"F,%u",&time)==1) {
      pulse(time,true);
    }
  }
  if(raw) {
    cydreed::Pulse result;
    if(contact.poll(lastRaw+500,result))pulse(result.timeUs,true);
  }
  printf("{\"raw_edges\":%u,\"falling\":%u,\"accepted\":%u,\"strokes\":%u,"
         "\"turns\":%u,\"spm\":%.2f,\"meters\":%.4f,\"peak_kmh\":%.3f,"
         "\"duration_s\":%.3f,\"short_low\":%u,\"short_high\":%u}\n",
         rawEdges,falling,accepted,motion.strokes,motion.transitions,motion.spm,
         motion.meters,peak*3.6f,(last-first)/1000000.0f,contact.shortLow,contact.shortHigh);
}
