#!/usr/bin/env python3
"""Guard Ghost Race navigation and BLE subscribe compatibility regressions."""
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
    'BOOT: 1x WYBOR / 1.5s START',
    'trendDrawn=trendRevision;',
    'if (ghost.phase==ghostrace::Phase::Armed && metrics.started)',
    'ghost.arm(raceDistances[index], found ? &storedGhost : nullptr,',
    'if (ghost.advance(now,metrics.distance()))',
    'TOUCH: deltaRaw=',
    'GHOST: horizontal swipe detected;',
    'horizontalSwipe=cydtouch::pageSwipe(',
    '#include "TouchGestures.h"',
    'tft.drawString("<  GHOST  >"',
    'registerForNotify(hrNotify,true,false)',
    'cccd->writeValue(enableNotify,2,true)',
    'HR: CCCD WRITE result=',
    'HR: GATT notify registration status=',
    'HR: raw GATT notification',
    'ESP_GATTC_WRITE_DESCR_EVT',
    'ESP_GATTC_REG_FOR_NOTIFY_EVT',
)
for marker in checks:
    assert marker in src, marker

# Entering the race menu must not force the user into a time-consuming touch
# calibration wizard; the BOOT button navigation path works without it.
begin=src.index("void toggleMode() {")
end=src.index("void updateButton(",begin)
assert "pendingCalibration=true" not in src[begin:end]
assert "ghost.cancel();" in src, "Preserve active race cancellation"
assert 'FIRMWARE_VERSION = "0.09.5"' in src
print("PASS: race menu touch/double-BOOT fallback, no forced wizard, HR CCCD status tracing")
