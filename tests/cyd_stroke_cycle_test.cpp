#include "../cyd_rower/src/CydMotion.h"
#include "../cyd_rower/src/ReedSignalFilter.h"
#include <algorithm>
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <vector>

// Generate crossings of four unequal magnets from a smooth, reversible
// reel trajectory. The endpoint is deliberately at different magnet angles.
static void phase(std::vector<uint64_t> &times,double start,double travel,
                  uint64_t at,double duration,bool reverse) {
  const double magnets[]={0,0.13,0.5,0.63};
  const double end=reverse?start-travel:start+travel;
  const double lo=std::min(start,end),hi=std::max(start,end);
  for(int rev=int(floor(lo))-1;rev<=int(ceil(hi))+1;++rev)
    for(double magnet:magnets) {
      const double position=rev+magnet;
      if(position<=lo || position>=hi)continue;
      const double fraction=(reverse?start-position:position-start)/travel;
      const double seconds=acos(1-2*fraction)/3.14159265358979323846;
      times.push_back(at+uint64_t(seconds*duration));
    }
}

static std::vector<uint64_t> strokes(const std::vector<int> &cadences,double offset,
                                     uint64_t start=100000) {
  std::vector<uint64_t> result;
  for(int spm:cadences) {
    const double duration=60000000.0/spm;
    phase(result,offset,5.01,start,duration*0.45,false);
    start+=uint64_t(duration*0.45);
    phase(result,offset+5.01,5.01,start,duration*0.55,true);
    start+=uint64_t(duration*0.55);
  }
  std::sort(result.begin(),result.end());
  return result;
}

int main() {
  for(int spm:{20,30,50,75}) {
    for(double offset:{0.01,0.10,0.26,0.49}) {
      const auto input=strokes(std::vector<int>(30,spm),offset);
      cydmotion::Engine engine;
      for(uint64_t time:input) {
        engine.onVerifiedPulse(uint32_t(time));
        assert(isfinite(engine.reelSpeed) && isfinite(engine.filteredReelSpeed));
        assert(isfinite(engine.acceleration) && isfinite(engine.confidence));
      }
      assert(engine.strokes==30);
      assert(fabsf(engine.spm-spm)<1.0f);
      assert(engine.transitions==59);
    }
  }
  std::vector<int> changes;
  for(int spm:{20,30,45,25})
    for(int i=0;i<15;++i)changes.push_back(spm);
  const auto input=strokes(changes,0.26,0xffff0000UL);
  cydmotion::Engine direct,noisy;
  cydreed::Filter filter;
  cydreed::Pulse pulse;
  filter.reset(uint32_t(input.front()-1000),true);
  for(uint64_t time:input) {
    direct.onVerifiedPulse(uint32_t(time));
    // Additional 60/80 us contact chatter belongs to the same magnet.
    for(auto edge:std::vector<std::pair<uint32_t,bool>>{
        {0,false},{60,true},{140,false},{1200,true},{1280,false},{1360,true}}) {
      if(filter.edge(uint32_t(time+edge.first),edge.second,pulse))
        noisy.onVerifiedPulse(pulse.timeUs);
    }
    if(filter.poll(uint32_t(time+1700),pulse))
      noisy.onVerifiedPulse(pulse.timeUs);
  }
  assert(direct.strokes==60 && noisy.strokes==60);
  assert(fabsf(direct.meters-noisy.meters)<0.001f);
  assert(fabsf(direct.speed-noisy.speed)<0.01f);

  // A missing crossing must not invent an extra cycle.
  cydmotion::Engine missing;
  for(size_t i=0;i<input.size();++i)
    if(i!=input.size()/3)missing.onVerifiedPulse(uint32_t(input[i]));
  assert(missing.strokes==60);

  const float retained=missing.meters;
  const uint32_t completed=missing.strokes;
  missing.signalLost(uint32_t(input.back()+1000));
  assert(missing.meters==retained && missing.strokes==completed && missing.speed==0);
  missing.onVerifiedPulse(uint32_t(input.back()+2000));
  assert(missing.meters==retained && missing.strokes==completed);
  // A partial pull has no completed slowdown/recovery.
  std::vector<uint64_t> partial;
  phase(partial,0.26,5.01,100000,1000000,false);
  std::sort(partial.begin(),partial.end());
  cydmotion::Engine unfinished;
  for(size_t i=0;i<partial.size()/2;++i)
    unfinished.onVerifiedPulse(uint32_t(partial[i]));
  assert(unfinished.strokes==0);
  unfinished.tick(uint32_t(partial[partial.size()/2]+9000000));
  assert(unfinished.strokes==0 && unfinished.speed==0);
  puts("PASS: 30 smooth cycles at 20/30/50/75 SPM, unequal magnets, near-magnet turns, cadence changes, chatter, missing edge, partial pull and wrap");
}
