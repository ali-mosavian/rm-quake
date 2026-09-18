#!/bin/bash
# Regression test: SV_movestep refuses e1m1's ledge when one monster corner has no support.
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
OUT="$(cd "${1:?usage: test-monster-bottom.sh <build-dir>}" && pwd)"

[[ -f "$OUT/e1m1.qmp" ]] || { echo "SKIP: no e1m1.qmp in $OUT"; exit 0; }
TIMEOUT="${TIMEOUT:-60}" "$HERE/run.sh" "$OUT" \
  "e1m1.qmp -nostats -nosound -noai -ticks 1" >/dev/null

grep -q '^mdl_bottom_selftest 1$' "$OUT/cstep.txt" || {
  echo "monster crossed e1m1's ledge with an unsupported corner" >&2
  exit 1
}

echo "ok: monster ledge step refused and origin preserved"
