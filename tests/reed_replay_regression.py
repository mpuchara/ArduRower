"""Physical reed recordings, labelled by the user, replayed through production code."""
import importlib.util
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("replay_reed", ROOT / "tools/replay_reed.py")
replay = importlib.util.module_from_spec(spec)
spec.loader.exec_module(replay)
executable = Path(sys.argv[1]).resolve()
baseline = Path(sys.argv[2]).resolve() if len(sys.argv) > 2 else None
manifest = json.loads((ROOT / "tests/fixtures/reed_sessions.json").read_text())

for session in manifest["sessions"]:
    trace = replay.read_trace(ROOT / "tests/fixtures" / session["file"])
    actual = replay.replay(executable, "raw", trace)
    assert actual["strokes"] == session["expected_strokes"], (session, actual)
    assert actual["short_low"] > 0, "recording must exercise real contact chatter"
    if baseline:
        old = replay.replay(baseline, "legacy", trace)
        assert old["strokes"] == session["baseline_strokes"], (session, old)
        # These labelled captures contain clean travel intervals as well as
        # chatter. Removing chatter must preserve their virtual distance.
        assert abs(actual["meters"] - old["meters"]) < 0.002, (actual, old)
        assert abs(actual["peak_kmh"] - old["peak_kmh"]) < 0.05, (actual, old)
    print(f"PASS: {session['file']}, {actual['strokes']} strokes, "
          f"{actual['meters']:.3f} m, rejected short closures {actual['short_low']}")
