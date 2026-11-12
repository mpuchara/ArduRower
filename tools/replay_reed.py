"""Replay a complete reedtrace CSV or CSV.gz through the host C++ engine.

Build the executable with g++ tools/replay_reed.cpp -o replay_reed.
"""
import argparse
import gzip
import json
from pathlib import Path
import subprocess


def read_trace(path):
    path = Path(path)
    if path.suffix == ".gz":
        return gzip.decompress(path.read_bytes()).decode("utf-8")
    return path.read_text(encoding="utf-8")


def replay(executable, mode, trace):
    result = subprocess.run(
        [str(executable), mode], input=trace, text=True,
        capture_output=True, check=True,
    )
    return json.loads(result.stdout)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("trace", type=Path)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--mode", choices=["raw", "filtered", "legacy"], default="raw")
    parser.add_argument("--baseline-executable", type=Path)
    args = parser.parse_args()
    source = read_trace(args.trace)
    result = {"current": replay(args.executable.resolve(), args.mode, source)}
    if args.baseline_executable:
        result["baseline"] = replay(
            args.baseline_executable.resolve(), "legacy", source,
        )
    print(json.dumps(result, indent=2))
