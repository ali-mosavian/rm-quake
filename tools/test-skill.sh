#!/bin/bash
# Regression test: the skill was baked into the assets, so start's trigger_setskill did nothing
# and every map played easy's monsters whatever the player chose.
#
#   1. start, standing in the hard skill brush (*11, message 2): the summary says skill 2.
#   2. start at the e1m1 slipgate with -skill 2 and fire held: e1m1 loads by the carried
#      skill, hard's 42 monsters. With -skill 0 it loads easy's 10.
#
#   tools/test-skill.sh build/cport-x
set -uo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/.." && pwd)"
OUT="${1:?usage: test-skill.sh <build-dir>}"
RUN="${RUN_SH:-$HERE/run.sh}"
log="$OUT/cstep.txt"
fail=0

# always: a .qmp staged before ents<N>.bin existed loads nothing
cp "$ROOT/data/maps/start/start.qmp" "$ROOT/data/maps/e1m1/e1m1.qmp" "$OUT/"
cp "$ROOT/data/stuff.ini" "$OUT/" 2>/dev/null

TIMEOUT="${TIMEOUT:-300}" "$RUN" "$OUT" "start.qmp -nostats -noai -at 864 1352 24 -ticks 30" >/dev/null 2>&1
skill=$(tr -d '\r' < "$log" 2>/dev/null | grep -m1 '^gs_state ' | sed -n 's/.* skill \([0-9]*\).*/\1/p')
[[ "$skill" == "2" ]] || { echo "FAIL: standing in start's hard skill brush leaves skill ${skill:-unset}, not 2" >&2; fail=1; }

for arm in "2 42" "0 10"; do
    read -r s want <<< "$arm"
    rm -f "$OUT/CARRY.BIN"
    TIMEOUT="${TIMEOUT:-600}" "$RUN" "$OUT" "start.qmp -nostats -noai -skill $s -at -64 1628 176 -fire -ticks 300" >/dev/null 2>&1
    mon=$(tr -d '\r' < "$log" 2>/dev/null | grep '^mod_load_world ok' | tail -1 | sed -n 's/.* mon=\([0-9]*\).*/\1/p')
    maps=$(tr -d '\r' < "$log" 2>/dev/null | grep -c '^asset_map ok')
    if [[ "$maps" -lt 2 || "$mon" != "$want" ]]; then
        echo "FAIL: -skill $s at start's e1m1 slipgate: $maps maps loaded, e1m1 has ${mon:-no} monsters, want $want" >&2
        fail=1
    fi
done

cp "$ROOT/tools/ref/carry-skill0.bin" "$OUT/CARRY.BIN"
KEEP_CARRY=1 TIMEOUT="${TIMEOUT:-300}" "$RUN" "$OUT" \
  "e1m1.qmp -nostats -noai -carry -ticks 2 +skill 2" >/dev/null 2>&1
grep -q '^carry health 100 shells 78 nails 50 rockets 0 armor 69 items 1 weapon 1$' "$log" || {
    echo "FAIL: real carry fixture was not applied" >&2
    fail=1
}
mon=$(tr -d '\r' < "$log" 2>/dev/null | grep -m1 '^mod_load_world ok' | sed -n 's/.* mon=\([0-9]*\).*/\1/p')
[[ "$mon" == "42" ]] || {
    echo "FAIL: -carry +skill 2 loaded ${mon:-no} monsters, want hard's 42" >&2
    fail=1
}

[[ $fail -eq 0 ]] && echo "ok: trigger_setskill sets the skill, and e1m1 loads 42 monsters on hard and 10 on easy"
exit $fail
