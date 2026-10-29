#!/usr/bin/env python3
"""Fail-closed installer must not flash an outdated CYD firmware image."""
from pathlib import Path
import re
ROOT = Path(__file__).resolve().parents[1]
site_source = (ROOT / "web/build_site.py").read_text(encoding="utf-8")
html = (ROOT / "index.html").read_text(encoding="utf-8")
firmware = (ROOT / "cyd_rower/src/main.cpp").read_text(encoding="utf-8")

page_version = re.search(r"const EXPECTED_PREFIX\s*=\s*'([^']+)'", html)
build_version = re.search(r'VERSION\s*=\s*"([^"]+)"\s*\+\s*REVISION', site_source)
device_version = re.search(r'FIRMWARE_VERSION\s*=\s*"([^"]+)"', firmware)
assert page_version and build_version and device_version
assert page_version.group(1) == build_version.group(1), "Installer HTML and build version disagree"
assert page_version.group(1).startswith(device_version.group(1) + "-"), "Firmware screen and installer version disagree"
assert 'f"{slug}-{name}-{REVISION}.bin"' in site_source, "Firmware file names are not immutable by revision"
assert '"revision": REVISION' in site_source, "No revision metadata in version.json"
assert '"version": VERSION' in site_source, "No version metadata"
assert 'manifest.version !== version.version' in html, "Installer doesn't compare manifest to version file"
assert 'parts.length !== 4' in html, "Installer doesn't check four correct binary parts"
assert "p.path.endsWith('-' + version.revision + '.bin')" in html, "Installer doesn't verify matching flashed binary"
assert 'manifestURL = MANIFEST_PATH + ' in html, "Installer doesn't use a versioned manifest query"
assert '<div id="install-ready" hidden>' in html, "Installer button is enabled before validation"
assert "button.setAttribute('manifest', manifestURL)" in html
assert "document.getElementById('install-widget').replaceChildren(fragment)" in html
assert 'FIRMWARE_VERSION,108,8,1' in firmware, "Version missing in dashboard"
assert 'FIRMWARE_VERSION,109,8,1' in firmware, "Version missing in Ghost Race"
assert 'FW ") + FIRMWARE_VERSION' in firmware, "Version missing in boot splash"
print("PASS: fail-closed installer verifies page, manifest, revisioned binaries and visible firmware revision")
