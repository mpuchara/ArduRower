#!/usr/bin/env python3
"""Guard touch-only Ghost Race navigation and no-blocking-wizard behavior."""
from pathlib import Path
root = Path(__file__).resolve().parents[1]
src = (root/"cyd_rower/src/main.cpp").read_text(encoding="utf-8")

checks = (
    'CydTouch touch(33,36);',
    'const bool pressed=touch.touched();',
    'if(++touchReleasedSamples<2)return;',
    'touchLastCompletedMs=now;',
    'TOUCH_REARM_MS=550',
    'GHOST: RELEASE SWIPE',
    'cydtouch::pageSwipe(dx,dy,touchCalibrated,mappedDx,mappedDy)',
    'if(touchReady && !touchCalibrated)',
    'const bool ok=calibrateRaceTouch();',
    'tc_force',
    'TOUCH: first-run calibration',
    'TOUCH: calibration requested from Ghost menu',
    'ESP.restart();',
    'tft.drawRightString("KALIBRUJ",309,33,2)',
    'GHOST: armed target=',
    'GHOST: started',
    'GHOST: finished',
    'ZWYKLY TRENING - WYJDZ',
    'GHOST: SELECT 500',
    'GHOST: SELECT 1000',
    'GHOST: SELECT 2000',
    'ghostWorkout!=displayedGhostWorkout',
    'ghost.meterLead(raceClockMs())',
    'FTMS: advertising recovery',
)

for marker in checks:
    assert marker in src, marker

assert src.index('const bool ok=calibrateRaceTouch();') < src.index('  initBLE();'), "Calibration must precede BLE"
touch=src[src.index('void updateTouch('):src.index('String formatTime(',src.index('void updateTouch('))]
assert 'touchTracker.sample(' not in touch
assert 'toggleMode();' in touch
assert 'touch.touched()' in touch
assert touch.count('toggleMode();')==1
assert 'GHOST: LIVE SWIPE' not in touch
assert 'FIRMWARE_VERSION = "0.09.28"' in src
adc=(root/'cyd_rower/src/CydTouch.h').read_text()
assert 'spi_->transfer16(' not in adc
assert 'z1=adc(0xB1),z2=adc(0xC1)' in adc
assert 'GHOST: entered menu using single BOOT click' in src
assert 'dashboardBootClicks' not in src
assert 'GHOST: entered menu using screen tap' in src
assert 'ghostWorkout?"ROZNICA / DUCH":"TEMPO /500m"' in src
assert 'Mode activeMode = Mode::Dashboard' in src
assert 'void exitGhostTraining()' in src
assert 'raceOptionsOpen=true' in src
assert 'KONTYNUUJ Z DUCHEM' in src and 'ZMIEN DYSTANS' in src
assert 'ghost.beatGhost() && !recordSaveFailed' in src
assert 'drawVictoryCup();' in src
assert 'ghost.advance(raceClockMs(),metrics.distance())' in src
assert 'g5_500' in src and 'h5_500' in src and 'w5_500' in src
assert 'metrics.paused?"AUTO PAUZA"' in src
print("PASS: byte-oriented pressure/release, calibrated menu, one BOOT entry and one Ghost toggle per finger")
