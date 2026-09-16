#!/bin/bash
# Regression test: a waist-deep player automatically water-jumps at a low ledge.
set -uo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
OUT="${1:?usage: test-waterjump.sh <build-dir>}"
TIMEOUT="${TIMEOUT:-180}" "${RUN_SH:-$HERE/run.sh}" "$OUT" \
    "dm3ish.qmp -nostats -noai -nosound -at 416 544 -32 -yaw 180 -ticks 30" >/dev/null 2>&1
line=$(tr -d '\r' < "$OUT/cstep.txt" | grep '^water ' | tail -1)
peak=$(sed -n 's/.* peak \([0-9-]*\).*/\1/p' <<< "$line")
if [[ "${peak:-0}" -le 0 ]]; then
    echo "FAIL: waist-deep ledge did not launch the player out: $line" >&2
    exit 1
fi
echo "ok: automatic water jump climbed from z=-32 to peak=$peak"
