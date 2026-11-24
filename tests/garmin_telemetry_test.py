#!/usr/bin/env python3
"""Verify the Connect IQ product, FIT schema, graph metadata and return HR contract."""
from pathlib import Path
import re
import xml.etree.ElementTree as ET

root = Path(__file__).resolve().parents[1]
base = root / "connect_iq"
source = (base / "source/ArduRowerApp.mc").read_text(encoding="utf-8")
link = (base / "source/RowerLink.mc").read_text(encoding="utf-8")
namespace = {"iq": "http://www.garmin.com/xml/connectiq"}
manifest = ET.parse(base / "manifest.xml")
application = manifest.find("iq:application", namespace)
assert (application.attrib["version"], application.attrib["id"]) == (
    "0.1.7", "a681ec0b07524e0dadd096027ee291b1"
)
assert application.find("iq:products/iq:product", namespace).attrib["id"] == "fenix6xpro"

fields = re.findall(r'createField\("([^"]+)",(\d+),Fit.DATA_TYPE_(\w+),\{:units=>"([^"]*)"', source)
assert [int(field[1]) for field in fields] == list(range(11))
assert [field[3] for field in fields] == ["m","m/s","spm","W","","strokes","s","s/500m","spm","s/500m","W"]
assert [field[2] for field in fields[:5]] == ["FLOAT","FLOAT","FLOAT","FLOAT","UINT8"]
assert sum({"FLOAT": 4, "UINT8": 1, "UINT16": 2}[field[2]] for field in fields) == 31

charts = ET.parse(base / "resources/contributions/fitcontributions.xml").findall("fitField")
strings = {item.attrib["id"]: item.text for item in ET.parse(base / "resources/strings.xml").findall("string")}
assert [int(chart.attrib["id"]) for chart in charts] == list(range(11))
for chart, field in zip(charts, fields):
    assert chart.attrib["displayInChart"] == "true"
    assert strings[chart.attrib["unitLabel"].split(".")[-1]] == (field[3] or "0/1")
    assert strings[chart.attrib["chartTitle"].split(".")[-1]]

for token in ("00001826-0000-1000-8000-00805f9b34fb",
              "00002ad1-0000-1000-8000-00805f9b34fb",
              "c9f10002-6f50-4fca-9b8d-3f8d90ae1b65", "WR S4BL3",
              "Ble.WRITE_TYPE_WITH_RESPONSE", "sequence=(sequence+1)&0xffff"):
    assert token in link
print("PASS: fenix6xpro app identity, 31 FIT bytes, 11 chart definitions and HR contract")
