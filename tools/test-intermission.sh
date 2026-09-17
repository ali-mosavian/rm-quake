#!/bin/bash
# Regression test: reaching an exit draws id's intermission art and tally.
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/.." && pwd)"
OUT="$(cd "${1:?usage: test-intermission.sh <build-dir>}" && pwd)"
PAK="${PAK:-$HOME/dos/QUAKE_SW/ID1/PAK0.PAK}"
E1M1="$ROOT/data/maps/e1m1/e1m1.qmp"

[[ -f "$E1M1" ]] || { echo "SKIP: no e1m1.qmp"; exit 0; }
[[ -f "$PAK" ]] || { echo "SKIP: no PAK0.PAK at $PAK"; exit 0; }

work="$(mktemp -d "$OUT/intermission-test.XXXXXX")"
trap 'rm -rf "$work"' EXIT
cp "$OUT/QCPORT.EXE" "$OUT/stuff.ini" "$OUT/default.cfg" "$E1M1" "$work/"

TIMEOUT="${TIMEOUT:-300}" "$HERE/run.sh" "$work" \
  "e1m1.qmp -nostats -nosound -noai -at 1312 544 -180 -ticks 3 +wait;screenshot;wait" >/dev/null
grep -q '^gs_state 4 ' "$work/cstep.txt"
grep -q '^map_release ok ' "$work/cstep.txt"
grep -q '^restored$' "$work/cstep.txt"
python3 "$HERE/check-intermission.py" "$PAK" "$work/SCRN0.BMP"
