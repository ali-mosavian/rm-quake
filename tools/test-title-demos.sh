#!/bin/bash
# Regression test: id's startdemos loop loads the map recorded in each QCR3,
# advances at EOF, and releases each old map. The short fixtures are real
# recorder output; before the final-dt fix the first replay diverged by one tick.
set -uo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/.." && pwd)"
OUT="${1:?usage: test-title-demos.sh <build-dir>}"
RUN="${RUN_SH:-$HERE/run.sh}"
LOG="$OUT/cstep.txt"

cp "$ROOT/data/maps/e1m1/e1m1.qmp" "$OUT/"
cp "$ROOT/data/stuff.ini" "$OUT/"
cp "$HERE/fixtures/titlea.qcr" "$HERE/fixtures/titleb.qcr" "$OUT/"

TIMEOUT="${TIMEOUT:-120}" "$RUN" "$OUT" \
    "e1m1.qmp +startdemos titlea titleb;wait;wait;wait;wait;wait;wait;quit" >/dev/null 2>&1

demos=$(tr -d '\r' < "$LOG" | sed -n 's/^attract demo \([^ ]*\) map .*/\1/p' | head -2 | paste -sd ' ' -)
[[ "$demos" == "titlea.qcr titleb.qcr" ]] || { echo "FAIL: demo order ${demos:-none}" >&2; exit 1; }
! grep -q 'DIVERGED' "$LOG" || { grep 'DIVERGED' "$LOG" >&2; exit 1; }
releases=$(grep -c '^map_release ok ' "$LOG")
[[ "$releases" -eq 3 ]] || { echo "FAIL: $releases map releases, want 3" >&2; exit 1; }
echo "ok: titlea -> titleb, $releases clean map releases"
