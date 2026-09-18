#!/bin/bash
# Regressions: drawing the first EMS-backed sprite used to overwrite the
# second, and a split bubble allocated into an earlier slot vanished when
# the live count was tallied only while walking forward.
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
OUT="${1:?usage: test-sprite-draw.sh <build-dir>}"

[[ -f "$OUT/e1m1.qmp" ]] || { echo "SKIP: no e1m1.qmp in $OUT"; exit 0; }

TIMEOUT="${TIMEOUT:-120}" "${RUN_SH:-$HERE/run.sh}" "$OUT" \
    "e1m1.qmp -nostats -noai -nosound -at 600 968 -344 -yaw 270 +save s9;quit" >/dev/null

python3 - "$OUT/s9.sav" <<'PY'
import struct
import sys

path = sys.argv[1]
data = bytearray(open(path, "rb").read())
header = data.rfind(b"SPR3")
if header < 0 or struct.unpack_from("<4H", data, header + 4) != (8, 64, 8, 8):
    raise SystemExit("FAIL: save has no 8/64/8/8-record SPR3 trailer")
records = header + 12
for i in range(8):
    struct.pack_into("<4fh", data, records + i * 18, 0.0, 0.0, 0.0, 0.0, 0)
struct.pack_into("<4fh", data, records,      568.0, 1044.0, -316.0, 0.0, -1)
struct.pack_into("<4fh", data, records + 18, 632.0, 1044.0, -316.0, 0.0, -1)
bubbles = records + 8 * 18
for i in range(64):
    struct.pack_into("<7f3h", data, bubbles + i * 34, *(0.0,) * 7, 0, 0, 0)
# Slot 1 splits into free slot 0. The parent is removed, so only the child
# remains; a forward-loop tally used to report zero and suppress its draw.
struct.pack_into("<7f3h", data, bubbles + 34,
                 600.0, 1044.0, -316.0, 0.0, 0.0, 15.0, 0.01, 0, 3, -1)
spawners = bubbles + 64 * 34
for i in range(8):
    struct.pack_into("<fh", data, spawners + i * 6, 0.0, 0)
open(path, "wb").write(data)
PY

TIMEOUT="${TIMEOUT:-120}" "${RUN_SH:-$HERE/run.sh}" "$OUT" \
    "e1m1.qmp -lm -nostats -noai -nosound -at 600 968 -344 -yaw 270 -ticks 1 +load s9" \
    >/dev/null

report=$(tr -d '\r' < "$OUT/cstep.txt" | grep '^sprites live=' | tail -1 || true)
live=$(sed -n 's/^sprites live=\([0-9][0-9]*\).*/\1/p' <<< "$report")
drawn=$(sed -n 's/.* drawn=\([0-9][0-9]*\).*/\1/p' <<< "$report")
bubbles_report=$(tr -d '\r' < "$OUT/cstep.txt" | grep '^bubbles live=' | tail -1 || true)
bubble_live=$(sed -n 's/^bubbles live=\([0-9][0-9]*\).*/\1/p' <<< "$bubbles_report")
bubble_drawn=$(sed -n 's/.* drawn=\([0-9][0-9]*\).*/\1/p' <<< "$bubbles_report")
spawn_left=$(sed -n 's/.* spawn=\([0-9][0-9]*\).*/\1/p' <<< "$bubbles_report")
if [[ "${live:-0}" -ne 2 || "${drawn:-0}" -ne 2 ||
      "${bubble_live:-0}" -ne 1 || "${bubble_drawn:-0}" -lt 1 ||
      "${spawn_left:-1}" -ne 0 ||
      ! -f "$OUT/BENCH.BMP" ]]; then
    echo "FAIL: barrel explosion reported [$report]" >&2
    echo "FAIL: split bubble reported [$bubbles_report]" >&2
    exit 1
fi

python3 - "$OUT/s9.sav" <<'PY'
import struct
import sys

path = sys.argv[1]
data = bytearray(open(path, "rb").read())
header = data.rfind(b"SPR3")
records = header + 12
bubbles = records + 8 * 18
for i in range(64):
    struct.pack_into("<7f3h", data, bubbles + i * 34, *(0.0,) * 7, 0, 0, 0)
spawners = bubbles + 64 * 34
for i in range(8):
    struct.pack_into("<fh", data, spawners + i * 6, 0.0, 0)
# Both DeathBubbles calls must become due inside the first fixed tick.
struct.pack_into("<fh", data, spawners, 0.01, 1)
struct.pack_into("<fh", data, spawners + 6, 0.01, 1)
open(path, "wb").write(data)
PY

TIMEOUT="${TIMEOUT:-120}" "${RUN_SH:-$HERE/run.sh}" "$OUT" \
    "e1m1.qmp -lm -nostats -noai -nosound -at 600 968 -344 -yaw 270 -ticks 1 +load s9" \
    >/dev/null

bubbles_report=$(tr -d '\r' < "$OUT/cstep.txt" | grep '^bubbles live=' | tail -1 || true)
bubble_live=$(sed -n 's/^bubbles live=\([0-9][0-9]*\).*/\1/p' <<< "$bubbles_report")
spawn_left=$(sed -n 's/.* spawn=\([0-9][0-9]*\).*/\1/p' <<< "$bubbles_report")
if [[ "${bubble_live:-0}" -ne 2 || "${spawn_left:-1}" -ne 0 ]]; then
    echo "FAIL: independent saved spawners reported [$bubbles_report]" >&2
    exit 1
fi

echo "ok: EMS explosions, earlier-slot bubble reuse and independent spawners survived"
