#!/bin/bash
# Regression: e1m4's two air_bubbles entities keep emitting the stock bubble
# sprite. Dropping the entities left the pool permanently empty.
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
OUT="${1:?usage: test-air-bubbles.sh <build-dir>}"

[[ -f "$OUT/e1m4.qmp" ]] || { echo "SKIP: no e1m4.qmp in $OUT"; exit 0; }

run() {
    CYCLES=max TIMEOUT="${TIMEOUT:-120}" "${RUN_SH:-$HERE/run.sh}" "$OUT" \
        "e1m4.qmp -lm -nostats -noai -nosound -at -256 1240 2272 -ticks $1" \
        >/dev/null
}

run 30
early=$(tr -d '\r' < "$OUT/cstep.txt" | grep '^bubbles live=' | tail -1 || true)
early_live=$(sed -n 's/^bubbles live=\([0-9][0-9]*\).*/\1/p' <<< "$early")
[[ "${early_live:-1}" -eq 0 ]] || {
    echo "FAIL: e1m4 emitted before the stock one-second delay [$early]" >&2
    exit 1
}

run 600
report=$(tr -d '\r' < "$OUT/cstep.txt" | grep '^bubbles live=' | tail -1 || true)
live=$(sed -n 's/^bubbles live=\([0-9][0-9]*\).*/\1/p' <<< "$report")
emitters=$(sed -n 's/.* emitters=\([0-9][0-9]*\).*/\1/p' <<< "$report")
if [[ "${emitters:-0}" -ne 2 || "${live:-0}" -lt 1 || ! -f "$OUT/BENCH.BMP" ]]; then
    echo "FAIL: e1m4 air bubbles reported [$report]" >&2
    exit 1
fi

# A v5 save carries the recurring deadlines. The two live records begin at
# the BSP origins; forcing them due proves the restored records drive ticks.
CYCLES=max TIMEOUT="${TIMEOUT:-120}" "${RUN_SH:-$HERE/run.sh}" "$OUT" \
    "e1m4.qmp -nostats -noai -nosound +save s8;toggleconsole;quit" >/dev/null
python3 - "$OUT/s8.sav" "$OUT/s7.sav" <<'PY'
import struct
import sys

v5_path, v4_path = sys.argv[1:]
data = bytearray(open(v5_path, "rb").read())
header = data.rfind(b"SPR3")
if header < 0 or struct.unpack_from("<4H", data, header + 4) != (8, 64, 8, 8):
    raise SystemExit("FAIL: save has no air-emitter SPR3 trailer")
records = header + 12
explosions = data[records:records + 8 * 18]
bubbles = data[records + 8 * 18:records + 8 * 18 + 64 * 34]
spawners = data[records + 8 * 18 + 64 * 34:records + 8 * 18 + 64 * 34 + 8 * 6]
emitters = records + 8 * 18 + 64 * 34 + 8 * 6
want = ((520.0, 1064.0, 440.0), (884.0, 1616.0, 440.0))
for i, origin in enumerate(want):
    record = struct.unpack_from("<3ffh", data, emitters + i * 18)
    if record[:3] != origin or record[4] != -1:
        raise SystemExit(f"FAIL: emitter {i} is {record}, expected {origin}")
    struct.pack_into("<f", data, emitters + i * 18 + 12, 0.01)
open(v5_path, "wb").write(data)

# v4 had 32 bubbles and no emitter records. Its migration must retain those
# records and restart the map emitters relative to restored animation time.
v4 = bytearray(data[:header])
struct.pack_into("<h", v4, 4, 4)
struct.pack_into("<f", v4, 1782, 10.0)  # SaveRender.anim_time in v4/v5
old_bubbles = bytearray(bubbles[:32 * 34])
struct.pack_into("<7f3h", old_bubbles, 0,
                 -256.0, 1240.0, 2294.0, 0.0, 0.0, 0.0, 1000.0, 0, 0, -1)
old_spawners = bytearray(spawners)
struct.pack_into("<fh", old_spawners, 0, 1000.0, 3)
v4 += struct.pack("<4s3H", b"SPR2", 8, 32, 8)
v4 += explosions + old_bubbles + old_spawners
open(v4_path, "wb").write(v4)
PY

CYCLES=max TIMEOUT="${TIMEOUT:-120}" "${RUN_SH:-$HERE/run.sh}" "$OUT" \
    "e1m4.qmp -nostats -noai -nosound -at -256 1240 2272 -ticks 1 +load s8" >/dev/null
saved=$(tr -d '\r' < "$OUT/cstep.txt" | grep '^bubbles live=' | tail -1 || true)
saved_live=$(sed -n 's/^bubbles live=\([0-9][0-9]*\).*/\1/p' <<< "$saved")
[[ "${saved_live:-0}" -eq 2 ]] || { echo "FAIL: v5 emitter restore [$saved]" >&2; exit 1; }

CYCLES=max TIMEOUT="${TIMEOUT:-120}" "${RUN_SH:-$HERE/run.sh}" "$OUT" \
    "e1m4.qmp -nostats -noai -nosound -at -256 1240 2272 -ticks 30 +load s7" >/dev/null
migrated=$(tr -d '\r' < "$OUT/cstep.txt" | grep '^bubbles live=' | tail -1 || true)
migrated_live=$(sed -n 's/^bubbles live=\([0-9][0-9]*\).*/\1/p' <<< "$migrated")
migrated_spawn=$(sed -n 's/.* spawn=\([0-9][0-9]*\).*/\1/p' <<< "$migrated")
migrated_emitters=$(sed -n 's/.* emitters=\([0-9][0-9]*\).*/\1/p' <<< "$migrated")
if [[ "${migrated_emitters:-0}" -ne 2 || "${migrated_live:-0}" -ne 1 ||
      "${migrated_spawn:-0}" -ne 3 ]]; then
    echo "FAIL: v4 state preservation or emitter delay [$migrated]" >&2
    exit 1
fi

CYCLES=max TIMEOUT="${TIMEOUT:-120}" "${RUN_SH:-$HERE/run.sh}" "$OUT" \
    "e1m4.qmp -nostats -noai -nosound -at -256 1240 2272 -ticks 90 +load s7" >/dev/null
migrated=$(tr -d '\r' < "$OUT/cstep.txt" | grep '^bubbles live=' | tail -1 || true)
migrated_live=$(sed -n 's/^bubbles live=\([0-9][0-9]*\).*/\1/p' <<< "$migrated")
migrated_emitters=$(sed -n 's/.* emitters=\([0-9][0-9]*\).*/\1/p' <<< "$migrated")
if [[ "${migrated_emitters:-0}" -ne 2 || "${migrated_live:-0}" -lt 3 ]]; then
    echo "FAIL: v4 emitter migration [$migrated]" >&2
    exit 1
fi

# A corrupt preceding skill count once moved the selected run before air.bin.
python3 - "$OUT/dm3ish.qmp" "$OUT/badair.qmp" <<'PY'
import struct
import sys

data = bytearray(open(sys.argv[1], "rb").read())
_, _, count, directory, _, _ = struct.unpack_from("<4slHlL16s", data)
for i in range(count):
    name, offset, size = struct.unpack_from("<16sll", data, directory + i * 24)
    if name.split(b"\0")[0] == b"air.bin":
        if size < 8:
            raise SystemExit("FAIL: air.bin is shorter than its counts")
        struct.pack_into("<4h", data, offset, -1, 1, 0, 0)
        break
else:
    raise SystemExit("FAIL: e1m4.qmp has no air.bin")
open(sys.argv[2], "wb").write(data)
PY
CYCLES=max TIMEOUT="${TIMEOUT:-120}" "${RUN_SH:-$HERE/run.sh}" "$OUT" \
    "badair.qmp -skill 1 -ticks 1" >/dev/null
grep -q 'air.bin has invalid emitters' "$OUT/error.log" || {
    echo "FAIL: corrupt preceding skill count was not rejected" >&2
    exit 1
}

echo "ok: e1m4 delays, recurs dry, and restores 2 emitters from v5 and v4 saves"
