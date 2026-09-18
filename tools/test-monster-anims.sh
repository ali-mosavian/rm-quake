#!/bin/bash
# Regression test: QuakeC animation events keep their frame, movement and deadlines.
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
OUT="$(cd "${1:?usage: test-monster-anims.sh <build-dir>}" && pwd)"
ROOT="$(cd "$HERE/.." && pwd)"

TIMEOUT="${TIMEOUT:-60}" "$HERE/run.sh" "$OUT" \
  "dm3ish.qmp -nostats -nosound -noai -ticks 1" >/dev/null
grep -q '^mdl_anim_selftest 1$' "$OUT/cstep.txt" || {
  echo "monster animation frame/movement self-test failed" >&2
  exit 1
}
grep -q '^combat_selftest 1$' "$OUT/cstep.txt" || {
  echo "monster pain-entry deadline self-test failed" >&2
  exit 1
}
grep -q '^d_alias_selftest 1$' "$OUT/cstep.txt" || {
  echo "alias model winding disagrees with id's front-face test" >&2
  exit 1
}

# The sixteen animation slots enlarge every resident model descriptor.
# e1m4 is the first map whose door block no longer fit when effect model
# metadata was allocated before the larger map-owned arrays.
cp "$ROOT/data/maps/e1m4/e1m4.qmp" "$OUT/"
TIMEOUT="${TIMEOUT:-60}" "$HERE/run.sh" "$OUT" \
  "e1m4.qmp -nostats -noai -ticks 1" >/dev/null
grep -q '^frames=' "$OUT/cstep.txt" || {
  echo "e1m4 exhausted conventional memory before its first frame" >&2
  exit 1
}
grep -q '^sc_init ok ' "$OUT/cstep.txt" || {
  echo "e1m4 loaded but lost its lightmapped surface cache" >&2
  exit 1
}
grep -q '^map_release ok ' "$OUT/cstep.txt" || {
  echo "e1m4 did not release its map allocations" >&2
  exit 1
}
if [[ -s "$OUT/ERROR.LOG" ]]; then
  echo "e1m4 left an error log" >&2
  exit 1
fi

echo "ok: monster animation frames, movement, solidity, pain deadlines and e1m4 memory"
