#!/bin/bash
# Regression test: trigger_monsterjump throws a walking monster. Its brush was hidden and
# nothing else, so e1m6's ogre ran off the ledge behind the t44 doors instead of jumping.
# The player presses the doors' button and waits; the summary's `mjumps=` must count one.
#
#   tools/test-monsterjump.sh build/cport-x
set -uo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/.." && pwd)"
OUT="${1:?usage: test-monsterjump.sh <build-dir>}"
RUN="${RUN_SH:-$HERE/run.sh}"
log="$OUT/cstep.txt"

# always: an e1m6.qmp staged before the trigger kind existed reads 0 on every arm
cp "$ROOT/data/maps/e1m6/e1m6.qmp" "$OUT/"
cp "$ROOT/data/stuff.ini" "$OUT/" 2>/dev/null

TIMEOUT="${TIMEOUT:-600}" "$RUN" "$OUT" "e1m6.qmp -nostats -bench 4000 -at -570 1280 24 -yaw 0 -ticks 900" >/dev/null 2>&1
jumps=$(tr -d '\r' < "$log" 2>/dev/null | grep -m1 "^ai hunting=" | sed -n 's/.* mjumps=\([0-9]*\).*/\1/p')
if [[ "${jumps:-0}" -lt 1 ]]; then
    echo "FAIL: no monster jumped at e1m6's trigger_monsterjump in 15 s (mjumps=${jumps:-none})" >&2
    exit 1
fi
echo "ok: e1m6's trigger_monsterjump threw $jumps"
