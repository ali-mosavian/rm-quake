#!/bin/bash
# Regression test: a hard landing hurts, and armor takes id's share of it.
#
#   tools/test-fall.sh <build-dir>
#
# 1. pl_land read vel.z, which the slide had already clipped to 0 on the tick
#    before the ground check saw the floor, so no fall ever hurt or made a sound:
#    a 426-unit drop left health 100. id tests jump_flag, the speed from before.
# 2. The armor's share, id's ceil of a float32 product, was -(long)-x, which
#    F_FTOL@'s chop makes trunc: four drowning bites of 4, 6, 8, 10 under red
#    armor saved 3+4+6+8 (armor 179, health 93) where id saves 4+5+7+8. The 10
#    bite is 8.00000012 at 80 bits and 8.0 stored, so it also needs the store.
# 3. The pentagram returned before the armor's share was taken, so armor never
#    wore under it; id takes the share first. e1m8's yellow armor, its pentagram,
#    then its lava: eleven bites of 10 save 6 each, armor 150 to 84, health 100.
#    Without the pentagram the same bites leave health 56 armor 84.
#
# dm3ish's column at (552,56) has a solid floor at z 0, is clear to 492 and has
# no entity within 96 units. The red armor is at (-272,-560,280); the pool at (500,0).
set -uo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
OUT="${1:?usage: test-fall.sh <build-dir>}"
[[ -f "$OUT/dm3ish.qmp" ]] || { echo "SKIP: no dm3ish.qmp in $OUT"; exit 0; }

run() { TIMEOUT="${TIMEOUT:-300}" "${RUN_SH:-$HERE/run.sh}" "$OUT" "$@" >/dev/null 2>&1; }
kit() { tr -d '\r' < "$OUT/cstep.txt" 2>/dev/null | grep -m1 '^fight='; }
field() { sed -n "s/.*[ =]$1 \([0-9-]*\).*/\1/p" <<< "$2"; }

fail=0
check() {   # check <name> <run args> <health> <armor> [map]
    run "${5:-dm3ish}.qmp -nostats -noai -nosound $2" || true
    local k; k=$(kit)
    if [[ "$(field health "$k")" != "$3" || "$(field armor "$k")" != "$4" ]]; then
        echo "FAIL: $1 gave [$k], wanted health $3 armor $4" >&2
        fail=1
    else
        echo "ok: $1 -- health $3 armor $4"
    fi
}

check "a 426-unit fall" "-at 552 56 450 -ticks 120" 95 0
check "the red armor, then four drowning bites" "-at -272 -560 280 -warp 30 500 0 -100 -ticks 960" 96 176
if [[ -f "$OUT/e1m8.qmp" ]]; then
    check "e1m8's yellow armor, the pentagram, then lava" "-at 672 312 -736 -warp 30 672 536 -712 -warp 60 752 544 -768 -ticks 180" 100 84 e1m8
else
    echo "SKIP: no e1m8.qmp in $OUT"
fi

exit $fail
