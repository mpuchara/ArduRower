#!/usr/bin/env python3
"""Guard touch-only Ghost Race navigation and no-blocking-wizard behavior."""
from pathlib import Path
root = Path(__file__).resolve().parents[1]
src = (root/"cyd_rower/src/main.cpp").read_text(encoding="utf-8")

checks = (
    'dashboardBootClicks==1 && now-dashboardBootClickAt<=480',
    'toggleMode(); // explicit physical fallback for Ghost Race',
    'GHOST: entered menu using double BOOT click',
    'GHOST: entered menu using dashboard title tap',
    'if(mappedTouch(rx,ry,x,y) && x<104 && y<27)',
    'GHOST: armed target=',
    'GHOST: started',
    'GHOST: finished',
    'DOTKNIJ 500/1000/2000, SWIPE: WROC',
    '4095L-(long)rx',
    'TOUCH: UP raw=',
    'trendDrawn=trendRevision;',
    'if (ghost.phase==ghostrace::Phase::Armed && metrics.started)',
    'ghost.arm(raceDistances[index], found ? &storedGhost : nullptr,',
    'if (ghost.advance(now,metrics.distance()))',
    'TOUCH: deltaRaw=',
    'GHOST: horizontal swipe detected;',
    'horizontalSwipe=cydtouch::pageSwipe(',
    '#include "TouchGestures.h"',
    'tft.drawString("<  GHOST  >"',
    'GHOST: LIVE SWIPE #',
    'swipeTriggered=true;',
    'touchSwipeEvents',
    'TOUCH: DOWN #',
    'GHOST: alive phase=',
    'GHOST: uncalibrated tap raw=',
    'GHOST: automatic calibration suppressed',
    'XPT2046_Touchscreen touch(33);',
    '#include "TouchPress.h"',
    'TOUCH: RECOVERY',
    'TOUCH: TAP without UP',
    'touchTracker.consume(now,pt.x,pt.y)',
    'GHOST: SELECT 500',
    'GHOST: EXIT',
    'now-touchLastPollMs<12',
    'NimBLEDevice::createServer()',

)
for marker in checks:
    assert marker in src, marker

# Entering the race menu must not force the user into a time-consuming touch
# calibration wizard; the BOOT button navigation path works without it.
begin=src.index("void toggleMode() {")
end=src.index("void updateButton(",begin)
assert "pendingCalibration=true" not in src[begin:end]
tap=src[src.index("void touchTap("):src.index("void updateTouch(")]
assert "pendingCalibration=true" not in tap, "Ghost menu taps must never schedule blocking calibration"
assert 'startRace(0);' in tap and 'startRace(1);' in tap and 'startRace(2);' in tap
assert 'selectedDistance=(selectedDistance+1)%3;' not in tap, "Touch must select row, never cycle"
loop=src[src.index("void loop()"):]
assert "if(!calibrateRaceTouch())" not in loop, "Ghost Race must never run a 45-second touch wizard in the main loop"
assert 'FTMS: advertising recovery' in loop
assert "ghost.cancel();" in src, "Preserve active race cancellation"
assert 'FIRMWARE_VERSION = "0.09.12"' in src
print("PASS: Ghost Race touch row selection, pressure-polled release/recovery, two-way swipe, no blocking wizard")
