#!/bin/bash
# Regression test: misc_fireball throws lava balls. Nothing armed its trigger, so
# e1m6's 15 emitters never threw one. Each throws its first within 5 s, so ten
# seconds on e1m6 must count at least 15 in the summary's `balls=`.
#
#   tools/test-fireball.sh build/cport-x
set -uo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/.." && pwd)"
OUT="${1:?usage: test-fireball.sh <build-dir>}"
RUN="${RUN_SH:-$HERE/run.sh}"
log="$OUT/cstep.txt"

[[ -f "$OUT/e1m6.qmp" ]] || cp "$ROOT/data/maps/e1m6/e1m6.qmp" "$OUT/"
cp "$ROOT/data/stuff.ini" "$OUT/" 2>/dev/null

TIMEOUT="${TIMEOUT:-600}" "$RUN" "$OUT" "e1m6.qmp -noai -nostats -bench 4000 -ticks 600" >/dev/null 2>&1
balls=$(tr -d '\r' < "$log" 2>/dev/null | grep -m1 "^ai hunting=" | sed -n 's/.* balls=\([0-9]*\).*/\1/p')
if [[ "${balls:-0}" -lt 15 ]]; then
    echo "FAIL: e1m6 threw ${balls:-no} lava balls in 10 s, want 15 or more" >&2
    exit 1
fi
echo "ok: e1m6 threw $balls lava balls in 10 s"
