#!/bin/bash
# Regression test: a save restores the moved player and releases both map loads.
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
OUT="$(cd "${1:?usage: test-savegame.sh <build-dir>}" && pwd)"

work="$(mktemp -d "$OUT/savegame-test.XXXXXX")"
trap 'rm -rf "$work"' EXIT
cp "$OUT/QCPORT.EXE" "$OUT/stuff.ini" "$OUT/default.cfg" "$OUT/dm3ish.qmp" "$work/"

TIMEOUT="${TIMEOUT:-60}" "$HERE/run.sh" "$work" \
  "dm3ish.qmp -nostats -nosound -noai -ticks 100 -walk +wait;wait;wait;wait;save s0;wait;load s0;quit" >/dev/null
saved="$(sed -n 's/^savegame wrote .* pos //p' "$work/cstep.txt")"
loaded="$(sed -n 's/^savegame loaded .* pos //p' "$work/cstep.txt")"
if [[ -z "$saved" || "$saved" != "$loaded" ]]; then
  echo "save/load position mismatch: saved '$saved', loaded '$loaded'" >&2
  exit 1
fi
if [[ "$(grep -c '^map_release ok ' "$work/cstep.txt")" -ne 2 ]]; then
  echo "save/load did not release both maps" >&2
  exit 1
fi
grep -Eq '^stack_untouched [1-9][0-9]* of ' "$work/cstep.txt" || {
  echo "save/load exhausted the stack probe" >&2
  exit 1
}
if [[ ! -s "$work/s0.sav" ]]; then
  echo "save file was not written" >&2
  exit 1
fi
echo "ok: save/load restored player at $loaded and released both maps"
