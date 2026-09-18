#!/bin/bash
# Regression test: Nightmare pain suppression does not replace a zombie's down timer.
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
OUT="$(cd "${1:?usage: test-nightmare-pain.sh <build-dir>}" && pwd)"

[[ -f "$OUT/e1m3.qmp" ]] || { echo "SKIP: no e1m3.qmp in $OUT"; exit 0; }
TIMEOUT="${TIMEOUT:-60}" "$HERE/run.sh" "$OUT" \
  "e1m3.qmp -skill 3 -nostats -nosound -noai -ticks 1" >/dev/null

grep -q '^combat_nightmare_selftest 1$' "$OUT/cstep.txt" || {
  echo "Nightmare did not suppress repeated pain for five seconds" >&2
  exit 1
}
grep -q '^mdl_nightmare_pain_selftest 1$' "$OUT/cstep.txt" || {
  echo "Nightmare pain suppression replaced the zombie knockdown timer" >&2
  exit 1
}
grep -q '^savegame_selftest 1$' "$OUT/cstep.txt" || {
  echo "Old zombie knockdown save did not preserve its recovery deadline" >&2
  exit 1
}

echo "ok: Nightmare pain cooldown and zombie knockdown timers are independent"
