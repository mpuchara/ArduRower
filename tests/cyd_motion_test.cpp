#include "../cyd_rower/src/CydMotion.h"
#include <stdio.h>
#include <math.h>
using namespace cydmotion;
#define REQUIRE(c) do { if(!(c)) {printf("FAIL line %d: %s\n",__LINE__,#c);return 1;} } while(0)
int main(){
  Engine e;
  e.reset(0);
  REQUIRE(e.strokes==0 && e.meters==0);
  uint32_t now=100000;
  for(int n=0;n<30;n++){
    for(int i=0;i<20;i++){now+=40000;e.onPulse(now);}
    now+=210000;e.onPulse(now); // turnaround to return
    for(int i=0;i<19;i++){now+=55000;e.onPulse(now);}
    now+=280000;e.onPulse(now); // turnaround to drive
  }
  REQUIRE(e.strokes==30);
  REQUIRE(e.transitions>=56);
  REQUIRE(e.meters>40 && e.meters<270);
  REQUIRE(e.speed>=0 && e.speed<7.5);
  printf("30 cycles detected %u; phase transitions %u; distance %.1fm; speed %.2fm/s\n",
        e.strokes,e.transitions,e.meters,e.speed);
  const float slowMeanSpeed=e.meters*1000000.0f/(now-100000);
  const float before=e.meters;
  now+=8000000;e.tick(now);
  REQUIRE(e.speed==0);
  REQUIRE(e.meters==before);
  REQUIRE(e.spm==0);
  // A lone pulse after a rest never counts an extra stroke.
  const uint32_t strokeCount=e.strokes;
  e.onPulse(now+25000);
  REQUIRE(e.strokes==strokeCount);
  REQUIRE(e.calibrationMilli()==700);
  e.setCalibration(0.5f);
  REQUIRE(e.calibrationMilli()==500);
  e.setCalibration(-100);
  REQUIRE(e.calibrationMilli()==500);
  // A burst shorter than one phase must not count as multiple strokes.
  e.reset(0);
  for(int i=0;i<5;i++)e.onPulse(100000+i*40000);
  REQUIRE(e.strokes==0);
  // Synthetic pulses keep the algorithm working across 32-bit micros wrap.
  e.reset(0xfffffff0UL);
  e.onPulse(0x0000c350UL);
  REQUIRE(e.pulses==1 && isfinite(e.speed));
  // Same reel travel with uneven magnet gaps. Compress the timeline
  // without changing pulse count. Faster travel must give faster pace
  // and exactly the same distance for the same physical movement.
  Engine steady, quicker;
  uint32_t steadyUs=100000, quickerUs=100000;
  steady.onPulse(steadyUs);
  quicker.onPulse(quickerUs);
  const uint32_t unevenGaps[] = {36000, 138000, 38000, 112000};
  for(int i=0;i<400;++i) {
    const uint32_t gap=unevenGaps[i%4];
    steadyUs+=gap;
    quickerUs+=gap*3/4;
    steady.onPulse(steadyUs);
    quicker.onPulse(quickerUs);
  }
  const float expectedDistance=400*Engine::kMetersPerPulse*Engine::kDefaultCalibration;
  REQUIRE(fabsf(steady.meters-expectedDistance)<0.001f);
  REQUIRE(steady.meters==quicker.meters);
  REQUIRE(quicker.speed>steady.speed*1.25f);
  REQUIRE(500.0f/quicker.speed<500.0f/steady.speed);
  Engine coast=quicker;
  Engine sparseTicks=quicker;
  const float movingSpeed=coast.speed;
  const float movingDistance=coast.meters;
  coast.tick(quickerUs+1000000);
  REQUIRE(coast.speed>movingSpeed*0.74f && coast.speed<movingSpeed*0.76f);
  coast.tick(quickerUs+2000000);
  REQUIRE(coast.speed>movingSpeed*0.56f && coast.speed<movingSpeed*0.57f);
  coast.tick(quickerUs+6000000);
  REQUIRE(coast.speed>0 && coast.speed<movingSpeed*0.06f);
  sparseTicks.tick(quickerUs+6000000);
  REQUIRE(fabsf(sparseTicks.speed-coast.speed)<0.0001f);
  coast.tick(quickerUs+8000000);
  REQUIRE(coast.speed==0 && coast.meters==movingDistance);
  quicker.onPulse(quickerUs+5000); // contact bounce
  REQUIRE(quicker.meters==steady.meters);
  quicker.tick(quickerUs+8000000);
  REQUIRE(quicker.speed==0 && quicker.meters==steady.meters);
  quicker.onPulse(quickerUs+9000000);
  REQUIRE(quicker.meters==steady.meters); // interval across a rest
  // Fast reel: an alternating 13/37 ms pair loses one edge with
  // 15 ms debounce. Include both for distance and keep stroke input intact.
  Engine fastReel, strokeInputOnly;
  uint32_t pairTime=100000;
  fastReel.onPulse(pairTime);
  strokeInputOnly.onPulse(pairTime);
  for(int i=0;i<100;++i) {
    fastReel.onPulse(pairTime+2500,false); // bounce remains ignored
    fastReel.onPulse(pairTime+13000,false);
    pairTime+=50000;
    fastReel.onPulse(pairTime);
    strokeInputOnly.onPulse(pairTime);
  }
  REQUIRE(fastReel.pulses==strokeInputOnly.pulses);
  REQUIRE(fastReel.strokes==strokeInputOnly.strokes);
  REQUIRE(fastReel.transitions==strokeInputOnly.transitions);
  REQUIRE(fabsf(fastReel.meters-2*strokeInputOnly.meters)<0.001f);
  REQUIRE(fastReel.speed>1.9f*strokeInputOnly.speed);
  // Short 50-70ms reversals during fast strokes were previously rejected
  // by the fixed 105ms / 320ms gates; this caused ~half the strokes to vanish.
  e.reset(0);
  now=100000;
  for(int n=0;n<30;++n) {
    for(int i=0;i<19;++i) {now+=26000;e.onPulse(now);}
    now+=55000;e.onPulse(now);
    for(int i=0;i<17;++i) {now+=33000;e.onPulse(now);}
    now+=70000;e.onPulse(now);
  }
  REQUIRE(e.strokes==30);
  REQUIRE(e.transitions>=56);
  REQUIRE(e.spm>=50 && e.spm<=55);
  REQUIRE(e.meters>0);
  // Fast rowing must improve the virtual 500m pace, not make it worse.
  const float fastMeanSpeed=e.meters*1000000.0f/(now-100000);
  REQUIRE(fastMeanSpeed>slowMeanSpeed*1.10f);
  printf("Fast 51 SPM: %u strokes, %u phase transitions, %.1f SPM, %.1fm\n",
         e.strokes,e.transitions,e.spm,e.meters);
  // Sprint cadence above 65 SPM must no longer freeze cadence reporting.
  e.reset(0);
  now=100000;
  for(int n=0;n<30;++n) {
    for(int i=0;i<12;++i) {now+=25000;e.onPulse(now);}
    now+=50000;e.onPulse(now);
    for(int i=0;i<12;++i) {now+=30000;e.onPulse(now);}
    now+=65000;e.onPulse(now);
  }
  REQUIRE(e.strokes==30);
  REQUIRE(e.spm>=70 && e.spm<=80);
  printf("Sprint: %u strokes, %.1f SPM\n",e.strokes,e.spm);
  // Four unevenly spaced magnets must NOT create artificial strokes.
  e.reset(0);
  now=100000;
  for(int n=0;n<150;++n) {
    const uint32_t gap=(n%4==3)?46000:25000;
    now+=gap;e.onPulse(now);
  }
  REQUIRE(e.strokes==0 && e.transitions==0);
  // A repeating wide gap can exceed the old conservative threshold too.
  e.reset(0);
  now=100000;
  for(int n=0;n<150;++n) {
    now+=(n%4==3)?145000:26000;
    e.onPulse(now);
  }
  REQUIRE(e.strokes==0 && e.transitions==0);
  // Captured on CYD during 10 fast strokes, with irregular magnet gaps.
  // The old detector counted 12 from these same accepted reed edges.
  const uint32_t fastReelGaps[] = {
    3899282, 56912, 164598, 37057, 155309, 29187, 130239, 29537, 119739, 27100,
    102749, 24507, 107271, 21606, 99771, 23043, 95763, 22365, 87681, 22801,
    145840, 35041, 56513, 20736, 51105, 18057, 51289, 20075, 53055, 20192,
    49783, 21616, 57904, 23775, 64224, 26149, 69718, 27365, 76575, 46298,
    152561, 106770, 25404, 100887, 18644, 84477, 18955, 79200, 18069, 70384,
    18024, 78110, 15431, 75052, 17567, 75226, 17507, 70282, 18335, 170945,
    30664, 53478, 19065, 45430, 16515, 41860, 16083, 43019, 16224, 37899,
    16506, 45338, 18511, 52472, 22511, 61663, 24788, 67082, 35097, 210062,
    25250, 81884, 19640, 82765, 15619, 72051, 16606, 69324, 15935, 60209,
    80717, 77485, 77858, 15034, 58201, 104709, 109834, 21870, 44776, 17288,
    43149, 16034, 41894, 16355, 43565, 16551, 39114, 17251, 47096, 19248,
    54284, 22177, 61380, 25100, 66091, 33341, 204396, 23650, 70789, 18097,
    75878, 83315, 15405, 64454, 71143, 76698, 73208, 74258, 69434, 92735,
    126294, 20815, 43511, 17225, 43301, 16421, 43540, 17177, 45721, 17330,
    40866, 17657, 48410, 19663, 54350, 22183, 59610, 23988, 63240, 31413,
    249828, 21744, 69148, 17266, 75134, 81863, 15563, 63311, 69983, 75181,
    71252, 72933, 68966, 87067, 27092, 55927, 79882, 21008, 43339, 17056,
    42508, 15814, 41382, 16304, 43876, 16952, 40199, 17669, 49612, 20101,
    55589, 23061, 61767, 24048, 61468, 29871, 259718, 20554, 66854, 17310,
    74937, 82207, 15070, 63101, 70211, 75367, 71693, 72339, 68571, 88027,
    41715, 19871, 94699, 22542, 46986, 18199, 45371, 16958, 43402, 16872,
    45503, 17341, 40825, 17654, 48597, 19601, 54149, 22577, 59458, 23229,
    62141, 31099, 268395, 20201, 67870, 17225, 73054, 79239, 76964, 68223,
    73658, 70434, 71972, 67119, 78594, 17115, 99906, 62093, 18954, 40170,
    15892, 40397, 15131, 38965, 15413, 41221, 15791, 38147, 17076, 48801,
    19944, 55485, 23015, 62729, 24840, 62707, 30074, 130205, 173411, 20498,
    68244, 16867, 73126, 80369, 15079, 62206, 68704, 73729, 70403, 71700,
    66800, 77625, 17014, 96741, 60807, 18199, 38989, 15339, 38960, 53024,
    15224, 41255, 15830, 38033, 16699, 47606, 19827, 54132, 22016, 60233,
    24451, 66413, 33743, 249194, 22033, 66870, 16978, 73660, 78823, 74927,
    66868, 71935, 67925, 69097, 65275, 73925, 143567, 27996, 55281, 17734,
    38333, 15149, 38575, 51549, 54402, 15175, 36497, 16336, 45707, 18748,
    51771, 21844, 60908, 24408, 63480, 32276, 157554, 141390, 20113, 65920,
    16656, 72302, 79424, 15062, 61467, 66079, 15469, 60721, 70118, 71742,
    67678, 88834, 39476, 26622, 97911, 23199, 47812, 18172, 44950, 16548,
    42838, 16739, 44644, 16877, 39249, 17004, 46861, 18924, 52332, 21884,
    63099, 26317, 72727, 39636,
  };
  e.reset(0);
  now=100000;
  e.onPulse(now);
  for(uint32_t gap : fastReelGaps) {
    now+=gap;
    e.onPulse(now);
  }
  REQUIRE(e.strokes==10 && e.transitions==19);
  puts("PASS: physical fast reel trace replay, 10/10 strokes");
  puts("PASS: slow & fast turning, 65+ SPM sprint, uneven magnets, smooth speed, pause, wrap");
  return 0;
}
