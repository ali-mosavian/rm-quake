"""Regression: title tracks come from id's demos and contain moving cameras."""

import math
import struct
import subprocess
import sys
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
PAK = Path(sys.argv[1]) if len(sys.argv) > 1 else Path.home() / "dos/QUAKE_SW/ID1/PAK0.PAK"
EXPECTED = {"demo1": "e1m3", "demo2": "e1m4", "demo3": "e1m6"}


def read_track(path: Path) -> tuple[str, list[tuple[float, ...]]]:
    data = path.read_bytes()
    assert data[:4] == b"QDC1", f"{path.name}: bad magic"
    map_name = data[4:13].split(b"\0", 1)[0].decode("ascii")
    records = [row for row in struct.iter_unpack("<6f", data[13:])]
    return map_name, records


def main() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        out = Path(tmp)
        for demo, expected_map in EXPECTED.items():
            track = out / f"{demo}.qdc"
            subprocess.run(
                [sys.executable, ROOT / "tools/mkidcam.py", PAK, f"{demo}.dem", track],
                check=True,
            )
            map_name, records = read_track(track)
            assert map_name == expected_map, f"{demo}: {map_name}, want {expected_map}"
            assert len(records) > 100, f"{demo}: only {len(records)} camera records"
            assert all(a[0] < b[0] for a, b in zip(records, records[1:])), f"{demo}: time did not advance"
            travel = sum(math.dist(a[1:4], b[1:4]) for a, b in zip(records, records[1:]))
            turn = sum(abs((b[5] - a[5] + 180) % 360 - 180) for a, b in zip(records, records[1:]))
            assert travel > 1000, f"{demo}: camera only travelled {travel:g} units"
            assert turn > 180, f"{demo}: camera only turned {turn:g} degrees"
    print("ok: id demo camera tracks move through e1m3, e1m4, e1m6")


if __name__ == "__main__":
    main()
