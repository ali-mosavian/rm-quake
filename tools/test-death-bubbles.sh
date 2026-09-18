#!/bin/bash
# Regression: single-player DeathSound emits twenty bubbles. QuakeWorld emits
# five; using that source left the spawner empty after five tenths of a second.
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

KEEP_CARRY=1 CYCLES=max TIMEOUT="${TIMEOUT:-160}" "${RUN_SH:-$HERE/run.sh}" "$OUT" \
    "e1m4.qmp -lm -nostats -noai -nosound -carry -at 456 1064 458 -yaw 0 -pitch -90 -ticks 780" \
    >/dev/null

report=$(tr -d '\r' < "$OUT/cstep.txt" | grep '^bubbles live=' | tail -1 || true)
fight=$(tr -d '\r' < "$OUT/cstep.txt" | grep '^fight=' | tail -1 || true)
live=$(sed -n 's/^bubbles live=\([0-9][0-9]*\).*/\1/p' <<< "$report")
left=$(sed -n 's/.* spawn=\([0-9][0-9]*\).*/\1/p' <<< "$report")
health=$(sed -n 's/^fight=health \([-0-9][0-9]*\).*/\1/p' <<< "$fight")
if [[ "${health:-1}" -ge 1 || "${live:-0}" -lt 1 || "${left:-0}" -lt 10 ||
      ! -f "$OUT/BENCH.BMP" ]]; then
    echo "FAIL: underwater death reported [$fight] [$report]" >&2
    exit 1
fi
echo "ok: underwater death has $live live bubbles and $left of id's twenty still scheduled"
