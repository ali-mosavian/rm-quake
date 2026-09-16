#!/bin/bash
# Regression test: a failed map reload restores hardware before DOS runs
# another program. Raw exit(1) left the timer and input vectors pointing
# into QCPORT's freed image, so the following run hung or faulted.
#
#   tools/test-reload-fatal.sh build/cport-mapcycle
set -uo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
OUT="${1:?usage: test-reload-fatal.sh <build-dir>}"
RUN="${RUN_SH:-$HERE/run.sh}"
MAP="$OUT/e1m2.qmp"
SAVE="$OUT/e1m2.qmp.reload-test"

[[ -f "$OUT/e1m1.qmp" && -f "$MAP" ]] || { echo "SKIP: e1m1/e1m2 not staged"; exit 0; }

cp "$MAP" "$SAVE"
trap 'mv -f "$SAVE" "$MAP"' EXIT
python3 - "$MAP" <<'PY'
import struct
import sys

path = sys.argv[1]
data = bytearray(open(path, "rb").read())
at = data.index(b"sbar.raw\0")
struct.pack_into("<L", data, at + 20, 1)
open(path, "wb").write(data)
PY

WALK="-at 1312 660 -200 -yaw 90 -walk -jump"
QARGS2="dm3ish.qmp -nostats -noai -ticks 2" \
TIMEOUT="${TIMEOUT:-300}" "$RUN" "$OUT" \
    "e1m1.qmp -nostats -noai $WALK -fire -ticks 300" >/dev/null 2>&1

log="$OUT/cstep.txt"
starts=$(tr -d '\r' < "$log" 2>/dev/null | grep -c '^start$')
frames=$(tr -d '\r' < "$log" 2>/dev/null | grep -c '^frames=')
[[ -s "$OUT/error.log" ]] || { echo "FAIL: corrupt e1m2 did not fail" >&2; exit 1; }
[[ "$starts" -eq 2 && "$frames" -ge 2 ]] || {
    echo "FAIL: fatal reload left DOS unsafe; next run reached $starts starts and $frames frame reports" >&2
    exit 1
}

echo "ok: failed e1m2 reload restored hardware and the next EXE ran"
