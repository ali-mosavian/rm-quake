#!/bin/bash
# Regression: single-player death keeps the corpse dead, clears the weapon,
# lowers and rolls the view, and DeathSound emits twenty bubbles. QuakeWorld
# emits five; using that source left the spawner empty after half a second.
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
OUT="${1:?usage: test-death-bubbles.sh <build-dir>}"

[[ -f "$OUT/e1m4.qmp" ]] || { echo "SKIP: no e1m4.qmp in $OUT"; exit 0; }

python3 - "$OUT/CARRY.BIN" <<'PY'
import struct
import sys

open(sys.argv[1], "wb").write(struct.pack(
    "<lhhfhhhhhhh", 1 | 4096, 1, 0, 0.0, 25, 0, 0, 1, 0, 0, 0
))
PY

run_death() {
    KEEP_CARRY=1 CYCLES=max TIMEOUT="${TIMEOUT:-160}" "${RUN_SH:-$HERE/run.sh}" "$OUT" \
        "e1m4.qmp -lm -nostats -noai -nosound -carry -at 456 1064 458 -yaw 0 -pitch -90 -ticks $1" \
        >/dev/null
}

# Before the spawner drains, prove that DeathSound scheduled id's twenty.
run_death 780
early=$(tr -d '\r' < "$OUT/cstep.txt" | grep '^bubbles live=' | tail -1 || true)
early_live=$(sed -n 's/^bubbles live=\([0-9][0-9]*\).*/\1/p' <<< "$early")
early_left=$(sed -n 's/.* spawn=\([0-9][0-9]*\).*/\1/p' <<< "$early")

# Later, the animation must remain a dead corpse after the spawner is empty.
run_death 960
report=$(tr -d '\r' < "$OUT/cstep.txt" | grep '^bubbles live=' | tail -1 || true)
fight=$(tr -d '\r' < "$OUT/cstep.txt" | grep '^fight=' | tail -1 || true)
death=$(tr -d '\r' < "$OUT/cstep.txt" | grep '^death set=' | tail -1 || true)
live=$(sed -n 's/^bubbles live=\([0-9][0-9]*\).*/\1/p' <<< "$report")
health=$(sed -n 's/^fight=health \([-0-9][0-9]*\).*/\1/p' <<< "$fight")
phase=$(sed -n 's/.* phase=\([0-9][0-9]*\).*/\1/p' <<< "$death")
if [[ -f "$OUT/BENCH.BMP" ]]; then
    frame=$(md5 -q "$OUT/BENCH.BMP" 2>/dev/null || md5sum "$OUT/BENCH.BMP" | cut -d' ' -f1)
else
    frame=MISSING
fi
if [[ "${early_live:-0}" -lt 1 || "${early_left:-0}" -lt 10 ||
      "${health:-1}" -ge 1 || "${live:-0}" -lt 1 ||
      "${phase:--1}" -ne 2 || "$frame" != d83bbf4a68f7ec1c17687fa5a6fee7f9 ]]; then
    echo "FAIL: underwater death reported early [$early], late [$fight] [$death] [$report], frame $frame" >&2
    exit 1
fi
echo "ok: dead view/corpse frame $frame; $early_left bubbles pending early and $live live late"
