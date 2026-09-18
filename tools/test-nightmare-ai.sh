#!/bin/bash
# Regression test: Nightmare soldiers refire once while their enemy remains visible.
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
OUT="$(cd "${1:?usage: test-nightmare-ai.sh <build-dir>}" && pwd)"

[[ -f "$OUT/e1m1.qmp" ]] || { echo "SKIP: no e1m1.qmp in $OUT"; exit 0; }
TIMEOUT="${TIMEOUT:-60}" "$HERE/run.sh" "$OUT" \
  "e1m1.qmp -skill 3 -at 88 1420 -190 -yaw 270 -nostats -nosound -noai -ticks 1" >/dev/null

grep -q '^mdl_nightmare_selftest 1$' "$OUT/cstep.txt" || {
  echo "Nightmare soldier did not refire exactly once with a visible enemy" >&2
  exit 1
}

echo "ok: Nightmare soldier refired exactly once"
