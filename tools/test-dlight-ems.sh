#!/bin/sh
# Regression test: the 32 dynamic lights used to consume 960 bytes of main's
# stack frame even though their state is only updated once per rendered frame.
set -eu

HERE=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
ROOT=$(CDPATH= cd -- "$HERE/.." && pwd)
OUT=${1:?usage: test-dlight-ems.sh <build-dir>}

cp "$ROOT/data/assets/dm3ish.qmp" "$OUT/"
TIMEOUT=${TIMEOUT:-600} "$HERE/run.sh" "$OUT" \
  "dm3ish.qmp -nosound -noai -ticks 1 -nostats -bench 4000" >/dev/null

grep -q '^dl_selftest 1$' "$OUT/cstep.txt" || {
  echo "FAIL: EMS-backed dynamic-light self-test failed" >&2
  exit 1
}
grep -q '^stack .* stklen=18772$' "$OUT/cstep.txt" || {
  echo "FAIL: dynamic-light state is not in the EMS frame store" >&2
  exit 1
}
grep -q '^map_release ok ' "$OUT/cstep.txt" || {
  echo "FAIL: EMS frame store was not released with the map" >&2
  exit 1
}
echo "ok: dynamic-light state uses the EMS frame store and releases cleanly"
