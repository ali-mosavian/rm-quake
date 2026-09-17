#!/bin/bash
# Debug capture writes one presented frame at each crossed 60-tick boundary.
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
OUT="${1:?usage: test-debug-capture.sh <QR_DEBUG_CAPTURE build-dir>}"

rm -f "$OUT"/dbg*.bmp
TIMEOUT="${TIMEOUT:-180}" "$HERE/run.sh" "$OUT" \
    "dm3ish.qmp -nostats -nosound -noai -ticks 121" >/dev/null 2>&1

[[ -s "$OUT/dbg00000.bmp" && -s "$OUT/dbg00001.bmp" ]] || {
    echo "FAIL: debug capture did not save ticks 60 and 120" >&2
    exit 1
}
[[ $(find "$OUT" -maxdepth 1 -iname 'dbg*.bmp' | wc -l | tr -d ' ') == 2 ]] || {
    echo "FAIL: debug capture saved more than one frame per boundary" >&2
    exit 1
}
grep -A1 '^dbg00000.bmp$' "$OUT/cstep.txt" | grep -Eq '^6[0-4]$'
grep -A1 '^dbg00001.bmp$' "$OUT/cstep.txt" | grep -Eq '^12[0-4]$'
echo "ok: debug capture saved frames after crossing the 60/120-tick boundaries"
