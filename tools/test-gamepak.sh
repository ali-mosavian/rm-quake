#!/bin/bash
# Regression test: a build with no loose maps must load both a level and a title demo from QUAKE.QPK.
set -euo pipefail

ROOT="$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)"
OUT="${1:?usage: test-gamepak.sh <build-dir>}"
RUN="${RUN_SH:-$ROOT/tools/run.sh}"

[[ -f "$OUT/QUAKE.QPK" ]] || { echo "FAIL: no QUAKE.QPK" >&2; exit 1; }
if compgen -G "$OUT/*.qmp" >/dev/null || compgen -G "$OUT/*.qdc" >/dev/null; then
    echo "FAIL: build stages loose map or demo files" >&2
    exit 1
fi

TIMEOUT="${TIMEOUT:-45}" "$RUN" "$OUT" \
    "e1m1.qmp -lm -nostats -noai -nosound -ticks 8" >/dev/null 2>&1
grep -q '^asset_map ok e1m1.qmp$' "$OUT/cstep.txt" || {
    echo "FAIL: e1m1 did not load from QUAKE.QPK" >&2; exit 1;
}
grep -q '^restored$' "$OUT/cstep.txt" || {
    echo "FAIL: archive map did not exit cleanly" >&2; exit 1;
}

TIMEOUT="${TIMEOUT:-45}" "$RUN" "$OUT" \
    "start.qmp -nostats -nosound +startdemos demo1;wait;wait;wait;wait;wait;toggleconsole;quit" >/dev/null 2>&1
grep -q '^attract demo demo1.qdc map e1m3$' "$OUT/cstep.txt" &&
grep -q '^asset_map ok e1m3.qmp$' "$OUT/cstep.txt" || {
    echo "FAIL: archive title demo did not stream and change to e1m3" >&2; exit 1;
}
grep -q '^restored$' "$OUT/cstep.txt" || {
    echo "FAIL: archive title demo did not exit cleanly" >&2; exit 1;
}
echo "ok: maps and title demos stream from QUAKE.QPK with no loose assets"
