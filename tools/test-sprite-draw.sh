#!/bin/bash
# Regression: the second EMS-backed sprite record became framebuffer bytes
# after the first masked quad remapped slot 2, so only one sprite survived.
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
OUT="${1:?usage: test-sprite-draw.sh <build-dir>}"

[[ -f "$OUT/e1m1.qmp" ]] || { echo "SKIP: no e1m1.qmp in $OUT"; exit 0; }

TIMEOUT="${TIMEOUT:-120}" "${RUN_SH:-$HERE/run.sh}" "$OUT" \
    "e1m1.qmp -nostats -noai -nosound -at 72 1980 -208 -yaw 270 +save s9;quit" >/dev/null

python3 - "$OUT/s9.sav" <<'PY'
import struct
import sys

path = sys.argv[1]
data = bytearray(open(path, "rb").read())
header = data.rfind(b"SPR1")
if header < 0 or struct.unpack_from("<H", data, header + 4)[0] != 8:
    raise SystemExit("FAIL: save has no eight-record SPR1 trailer")
records = header + 6
for i in range(8):
    struct.pack_into("<4fh", data, records + i * 18, 0.0, 0.0, 0.0, 0.0, 0)
struct.pack_into("<4fh", data, records,      40.0, 2056.0, -180.0, 0.0, -1)
struct.pack_into("<4fh", data, records + 18, 104.0, 2056.0, -180.0, 0.0, -1)
open(path, "wb").write(data)
PY

TIMEOUT="${TIMEOUT:-120}" "${RUN_SH:-$HERE/run.sh}" "$OUT" \
    "e1m1.qmp -lm -nostats -noai -nosound -at 72 1980 -208 -yaw 270 +load s9;wait;quit" \
    >/dev/null

report=$(tr -d '\r' < "$OUT/cstep.txt" | grep '^sprites live=' | tail -1 || true)
live=$(sed -n 's/^sprites live=\([0-9][0-9]*\).*/\1/p' <<< "$report")
drawn=$(sed -n 's/.* drawn=\([0-9][0-9]*\).*/\1/p' <<< "$report")
if [[ "${live:-0}" -ne 2 || "${drawn:-0}" -ne 2 || ! -f "$OUT/BENCH.BMP" ]]; then
    echo "FAIL: barrel explosion reported [$report]" >&2
    exit 1
fi
echo "ok: two EMS sprite records survived remaps and drew two masked quads"
