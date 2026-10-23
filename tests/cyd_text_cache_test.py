#!/usr/bin/env python3
"""Prevent the Ghost Race on-device reboot caused by an undersized text cache."""
from pathlib import Path
import re

src = (Path(__file__).resolve().parents[1] / "cyd_rower/src/main.cpp").read_text(encoding="utf-8")

def extract_integer(regex, title):
    match = re.search(regex, src)
    assert match is not None, f"Missing {title}"
    return int(match.group(1))

size = extract_integer(r"kTextSlotCount\s*=\s*(\d+)\s*;", "text cache slot count")
assert size >= 21, "Ghost Race needs text slots 0 through 20"
assert re.search(r"String\s+previousField\s*\[\s*kTextSlotCount\s*\]", src), "String cache is undersized"
assert re.search(r"bool\s+previousValid\s*\[\s*kTextSlotCount\s*\]", src), "Validity cache differs"
assert "slot >= kTextSlotCount" in src, "No runtime bounds guard"
assert "i < kTextSlotCount" in src, "Cache invalidation still hardcodes an old limit"

uses = re.findall(r"\b(?:textPatch|cardValue)\s*\(\s*(\d+)\s*,", src)
assert uses, "No widgets detected"
max_slot = max(map(int, uses))
assert max_slot < size, f"UI widget slot {max_slot} exceeds cache {size}"
assert max_slot >= 20, "Ghost Race widget coverage was accidentally removed"
assert "esp_reset_reason()" in src, "Reset diagnostics unavailable"
print(f"PASS: {len(uses)} widget references, highest slot={max_slot}, cache={size}, bounds guards")
