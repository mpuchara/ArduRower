#!/usr/bin/env python3
"""Source-level guard for the physical ST7789 flicker regression."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
source = (ROOT / "cyd_rower/src/main.cpp").read_text(encoding="utf-8")
platform = (ROOT / "cyd_rower/platformio.ini").read_text(encoding="utf-8")
site = (ROOT / "web/build_site.py").read_text(encoding="utf-8")
html = (ROOT / "index.html").read_text(encoding="utf-8")


def body(name):
    start = re.search(r"\bvoid\s+" + re.escape(name) + r"\(.*?\)\s*\{", source, re.S)
    assert start, name
    depth = 1
    i = start.end()
    beginning = i
    while depth:
        if source[i] == "{":
            depth += 1
        if source[i] == "}":
            depth -= 1
        i += 1
    return source[beginning:i-1]


# No full-screen clear in any frequently-called function; exactly one layout
# clear per page transition and one splash-screen clear during startup.
assert source.count("tft.fillScreen(") <= 5  # startup, view changes and touch calibration
assert "tft.fillScreen(" in body("pageLayout")
assert "tft.fillScreen(" in body("initDisplay")
assert "tft.fillScreen(" in body("paintRaceLayout")
assert "tft.fillScreen(backgroundColor);" in source
for method in ("updateUI", "renderRaceUI", "textPatch", "cardValue", "plot", "paintProgress", "loop"):
    assert "tft.fillScreen(" not in body(method), method
assert "scratch.pushSprite(" in source
assert "trendDrawn!=trendRevision" in source
assert "const bool swipe=cydtouch::pageSwipe(" in source
assert "TouchGestures.h" in source
assert "ghost.start(now)" in source
assert "saveBest(selectedDistance,ghost.attempt)" in source
assert "previousValid[slot] && previousField[slot] == text" in source

# Only build/publish exactly the hardware variant confirmed by the user.
assert platform.count("[env:") == 1
assert "[env:esp32-2432s028-st7789-gpio21]" in platform
assert 'st7789-gpio27' not in site
assert 'ili9341-gpio21' not in site
assert 'st7789-gpio27' not in html
assert 'ili9341-gpio21' not in html
print("PASS: retained layout, patch refresh, confirmed ST7789/GPIO21 profile")
