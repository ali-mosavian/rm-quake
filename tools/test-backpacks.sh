#!/bin/bash
# Regression: an ogre's normal death drops id's two-rocket backpack. The
# pickup must survive long enough to be touched and add rockets; the old
# code only dropped five shells from soldiers and emitted nothing here.
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
OUT="${1:?usage: test-backpacks.sh <build-dir>}"

[[ -f "$OUT/e1m2.qmp" ]] || { echo "SKIP: no e1m2.qmp in $OUT"; exit 0; }

# Twenty shotgun rounds kill the stationary ogre without crossing its -80
# gib threshold. Warp onto its origin after the death animation's drop point.
ARGS="e1m2.qmp -nostats -noai -nosound -at 1018 -226 320 -yaw 270 -fire -warp 500 1034 -59 319 -ticks 600"

check_run() {
    local want_rockets="$1"
    local line released kills rockets took

    line=$(tr -d '\r' < "$OUT/cstep.txt" 2>/dev/null | grep -m1 '^fight=' || true)
    released=$(tr -d '\r' < "$OUT/cstep.txt" 2>/dev/null | grep -m1 '^map_release ok ' || true)
    kills=$(sed -n 's/.* kills \([0-9][0-9]*\).*/\1/p' <<< "$line")
    rockets=$(sed -n 's/.* rockets \([0-9][0-9]*\).*/\1/p' <<< "$line")
    took=$(sed -n 's/.* took \([0-9][0-9]*\).*/\1/p' <<< "$line")

    if [[ "${kills:-0}" -lt 1 || "${rockets:-0}" -ne "$want_rockets" || "${took:-0}" -lt 1 || -z "$released" ]]; then
        echo "FAIL: normal ogre death/pickup gave [$line], wanted one kill, $want_rockets rockets, and one taken item" >&2
        [[ -n "$released" ]] || echo "FAIL: dropped-pickup run did not release the map cleanly" >&2
        return 1
    fi
}

CYCLES=max TIMEOUT="${TIMEOUT:-120}" "${RUN_SH:-$HERE/run.sh}" "$OUT" "$ARGS" >/dev/null 2>&1 || true
check_run 2

# BackpackTouch removes a bag at the ammo cap too; ordinary ammo_touch refuses it.
python3 - "$OUT/CARRY.BIN" <<'PY'
import struct
import sys

open(sys.argv[1], "wb").write(struct.pack(
    "<lhhfhhhhhhh", 1 | 4096, 100, 0, 0.0, 25, 0, 100, 1, 0, 0, 0
))
PY
KEEP_CARRY=1 CYCLES=max TIMEOUT="${TIMEOUT:-120}" "${RUN_SH:-$HERE/run.sh}" "$OUT" \
    "${ARGS/e1m2.qmp/e1m2.qmp -carry}" >/dev/null 2>&1 || true
check_run 100

echo "ok: the ogre dropped its two-rocket backpack; BackpackTouch takes it at the ammo cap too"
