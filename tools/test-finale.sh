#!/bin/bash
# Regression test: e1m7 shows its finale and sell screen before returning to start.
set -uo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
OUT="${1:?usage: test-finale.sh <build-dir>}"
RUN="${RUN_SH:-$HERE/run.sh}"
[[ -f "$OUT/e1m7.qmp" && -f "$OUT/start.qmp" ]] || { echo "SKIP: e1m7/start not staged"; exit 0; }

run() { TIMEOUT="${TIMEOUT:-180}" "$RUN" "$OUT" "$1 -nostats -noai -nosound" >/dev/null 2>&1; }
state() { tr -d '\r' < "$OUT/cstep.txt" | sed -n 's/^gs_state \([0-9]*\).*/\1/p' | tail -1; }

run "e1m7.qmp -at 1016 52 -1000 -fire -ticks 160"
[[ "$(state)" == 6 ]] || { echo "FAIL: e1m7 skipped finale (state $(state))" >&2; exit 1; }

run "e1m7.qmp -at 1016 52 -1000 -jump -ticks 225"
[[ "$(state)" == 7 ]] || { echo "FAIL: finale did not reach sell screen (state $(state))" >&2; exit 1; }

run "e1m7.qmp -at 1016 52 -1000 -fire -ticks 320"
map=$(tr -d '\r' < "$OUT/cstep.txt" | sed -n 's/^asset_map ok //p' | tail -1)
[[ "$map" == start.qmp ]] || { echo "FAIL: sell screen did not return to start ($map)" >&2; exit 1; }
echo "ok: e1m7 finale, sell screen, then start"
