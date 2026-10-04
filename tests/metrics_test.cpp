#include "RowerMetrics.h"
#include <assert.h>
#include <stdio.h>
#undef assert
#define assert(x) do { if (!(x)) { printf("FAIL line %d: %s\n", __LINE__, #x); return 1; } } while (0)

bool near(float a, float b, float tolerance = 0.001f) { return fabsf(a-b) < tolerance; }
int main() {
  RowerMetrics m; m.reset(0);
  m.tick(10000000); assert(!m.started && m.activeUs == 0 && m.distance() == 0);
  m.reset(0);
  for (uint32_t n=1; n<=200; ++n) m.pulse(n*100000);
  printf("speed=%f seconds=%f pulses=%u\n",m.speed,m.seconds(),m.pulses);
  assert(m.pulses == 200 && near(m.distance(),200/3.156f));
  assert(near(m.speed,10/3.156f,0.01f));
  assert(m.strokes == 0); // Constant frequency must not generate strokes.
  assert(near(m.seconds(),19.9f));
  assert(near(m.averagePace(),500*m.seconds()/m.distance()));
  m.tick(30000000); assert(m.paused && near(m.seconds(),24.9f));
  assert(m.speed == 0 && m.strokeRate == 0 && m.pace() == 0);
  m.tick(35000000); assert(near(m.seconds(),24.9f));
  m.pulse(40000000); assert(!m.paused && near(m.seconds(),24.9f));
  m.tick(40100000); assert(near(m.seconds(),25.0f));
  assert(m.pulses == 201); // First pulse at start/resume is retained.

  m.reset(0xffffff00u); m.pulse(0xffffff00u); m.pulse(99744u);
  assert(near(m.seconds(),0.1f)); // micros rollover during training.
  m.tick(6099744u); assert(m.paused && near(m.seconds(),5.1f));
  const auto stopped = m.activeUs;
  m.tick(200000u); assert(m.activeUs == stopped && m.paused); // long idle stays paused.

  m.reset(0); uint32_t t=100000; m.pulse(t); t+=100000; m.pulse(t);
  t+=30000; m.pulse(t); assert(m.strokes == 1 && m.strokeRate == 0);
  for (int n=0;n<30;++n) { t+=30000; m.pulse(t); }
  assert(m.strokes == 1); // Holdoff and constant speed prevent repeats.
  t+=100000; m.pulse(t); t+=30000; m.pulse(t);
  assert(m.strokes == 2 && near(m.strokeRate,60000000.0f/1030000,0.001f));
  assert(near(m.averageStrokeRate(),m.strokes*60.0f/m.seconds()));
  m.reset(t); assert(m.strokes == 0 && m.pulses == 0 && m.speed == 0 && !m.started);
  m.pulse(t+100000); m.pulse(t+100000); // No zero division, even for duplicate inputs.
  assert(isfinite(m.speed) && isfinite(m.averageStrokeRate()));
  m.reset(0);
  for (uint32_t n=1;n<=300;++n) m.pulse(n*100000);
  assert(m.averageEquivalentPower() > 0 && m.averageEquivalentPower() < m.equivalentPower());
  puts("PASS: distance, measured sampling time, steady speed, pause/resume, rollover, detector holdoff/cadence, reset, zero-gap safety, power integration");
}
