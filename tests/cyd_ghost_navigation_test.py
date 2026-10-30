#!/usr/bin/env python3
"""Guard touch-only Ghost Race navigation and no-blocking-wizard behavior."""
from pathlib import Path
root = Path(__file__).resolve().parents[1]
src = (root/"cyd_rower/src/main.cpp").read_text(encoding="utf-8")

checks = (
    'XPT2046_Touchscreen touch(33,36);',
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
    'DOTKNIJ 500/1000/2000, SWIPE: WROC',
    'GHOST: SELECT 500',
    'GHOST: SELECT 1000',
    'GHOST: SELECT 2000',
    'ghostWorkout!=displayedGhostWorkout',
    'ghost.meterLead(millis())',
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
assert 'FIRMWARE_VERSION = "0.09.15"' in src
print("PASS: HA-style IRQ contact/release, calibrated menu, single Ghost toggle per finger")
