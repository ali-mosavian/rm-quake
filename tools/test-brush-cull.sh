#!/bin/bash
# Regression test: a translated brush must be culled in model space.
#
# What went wrong: e1m1's first double door moved its vertices, but face
# planes, BSP nodes and frustum bounds stayed at the map coordinates. At the
# fully-open position front-face culling left only 827 door pixels; the
# correctly translated tests leave more than 5,400.
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/.." && pwd)"
OUT="${1:?usage: test-brush-cull.sh <build-dir>}"
RUN="${RUN_SH:-$HERE/run.sh}"
COMMON="e1m1.qmp -lm -nostats -noai -nosound -nomdl -noview -bench 4000 -ticks 30 -at 300 576 40 -yaw 180"

[[ -f "$OUT/e1m1.qmp" ]] || cp "$ROOT/data/maps/e1m1/e1m1.qmp" "$OUT/"

shot() {
    TIMEOUT="${TIMEOUT:-240}" "$RUN" "$OUT" "$COMMON $2" >/dev/null 2>&1
    [[ -s "$OUT/BENCH.BMP" ]] || { echo "FAIL: $1 wrote no frame" >&2; exit 1; }
    cp "$OUT/BENCH.BMP" "$OUT/bc-$1.bmp"
}

shot doors ""
shot noents "-noents"
shot doors-nocull "-nocull"
shot noents-nocull "-noents -nocull"

python3 - "$OUT/bc-doors.bmp" "$OUT/bc-noents.bmp" \
    "$OUT/bc-doors-nocull.bmp" "$OUT/bc-noents-nocull.bmp" <<'PY'
import struct
import sys


def pixels(path):
    data = open(path, "rb").read()
    offset, = struct.unpack_from("<I", data, 10)
    width, height = struct.unpack_from("<ii", data, 18)
    stride = (width + 3) & ~3
    return b"".join(
        data[offset + y * stride:offset + y * stride + width]
        for y in range(abs(height))
    )


doors, noents, doors_nocull, noents_nocull = map(pixels, sys.argv[1:])
changed = sum(a != b for a, b in zip(doors, noents))
print(f"{changed} pixels come from the translated double door")
if changed < 5000:
    print(f"FAIL: translated door left {changed} pixels, wanted at least 5000", file=sys.stderr)
    raise SystemExit(1)

# Compare the brush contribution, not the whole frame: -nocull also changes
# world-face handling, and the world is not the subject of this regression.
mismatch = sum(
    (a, b) != (c, d)
    for a, b, c, d in zip(doors, noents, doors_nocull, noents_nocull)
)
print(f"{mismatch} translated-door pixels differ from the uncull reference")
if mismatch > 64:
    print(f"FAIL: translated door differs from uncull reference at {mismatch} pixels", file=sys.stderr)
    raise SystemExit(1)
PY

echo "ok: translated brush faces are culled in model space"
