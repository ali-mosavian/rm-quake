#!/bin/bash
# Regression test: monster visibility uses a hull-0 point trace, not hull 1.
set -uo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
OUT="${1:?usage: test-ai-visible.sh <build-dir>}"
[[ -f "$OUT/e1m2.qmp" ]] || { echo "SKIP: no e1m2.qmp in $OUT"; exit 0; }

TIMEOUT="${TIMEOUT:-180}" "${RUN_SH:-$HERE/run.sh}" "$OUT" \
    "e1m2.qmp -nostats -noai -nosound -at -150 625 340 -ticks 1" >/dev/null 2>&1
visible=$(tr -d '\r' < "$OUT/cstep.txt" | sed -n 's/^ai_visible_selftest \([0-9-]*\).*/\1/p' | tail -1)
[[ "$visible" == 1 ]] || {
    echo "FAIL: demon visibility bitmask was ${visible:-nothing}, wanted point-only 1" >&2
    exit 1
}
echo "ok: e1m2 demon is visible through hull 0 and blocked by hull 1"
