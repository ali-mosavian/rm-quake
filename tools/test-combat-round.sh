#!/bin/bash
# Regression test: T_Damage keeps float damage until ceil(damage-save).
set -uo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
OUT="${1:?usage: test-combat-round.sh <build-dir>}"
TIMEOUT="${TIMEOUT:-120}" "${RUN_SH:-$HERE/run.sh}" "$OUT" \
    "dm3ish.qmp -nostats -noai -nosound -ticks 2" >/dev/null 2>&1
got=$(tr -d '\r' < "$OUT/cstep.txt" | sed -n 's/^combat_selftest //p' | tail -1)
[[ "$got" == 1 ]] || { echo "FAIL: fractional damage selftest returned ${got:-nothing}" >&2; exit 1; }
echo "ok: fractional damage is ceiled after armor"
