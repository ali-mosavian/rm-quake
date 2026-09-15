#!/bin/bash
# Regression test: tools/x87cmp.py, the check a module passes before it
# joins tools/bcc.sh's COLD list.
#
# The fixtures are real bcc -S output of mdl_ai.c and weapons.c at -Ox,
# -Ox -Os and -O1, cut to one function each. -Os must read identical and
# -O1 must not: mdl_leap_height reloads from 32 bits a value -Ox keeps at
# 80, and mdl_slab's -O1 code differs too. A comparer that stopped
# reading any x87 op would pass the -O1 arms, and this fails.
# dmg-* and mdmg-* are pl_damage and mdl_damage before and after their
# move to combat.c, cut with their jump tables and constant pools; the
# *pool and *call files are those cuts with one byte or one call changed.
#
#   tools/test-x87cmp.sh
set -uo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
F="$HERE/ref/x87"
fail=0

check() {   # module a b func 0|some
    local n
    n=$(python3 "$HERE/x87cmp.py" "$F/$1-$2.asm" "$F/$1-$3.asm" "$4" 2>/dev/null |
        sed -n 's/.* \([0-9]*\) mismatches,.*/\1/p' | tail -1)
    if [[ -z "$n" ]]; then
        echo "FAIL: no count for $4 at $2 against $3 -- x87cmp did not run" >&2; fail=1
    elif [[ $5 == 0 ]] && (( n != 0 )); then
        echo "FAIL: $4 differs between $2 and $3 in $n segments" >&2; fail=1
    elif [[ $5 == some ]] && (( n == 0 )); then
        echo "FAIL: $4 reads identical at $2 and $3 -- the comparer is blind" >&2; fail=1
    elif [[ $5 =~ ^[1-9][0-9]*$ ]] && (( n != $5 )); then
        echo "FAIL: $4 at $2 against $3 reads $n mismatches, wants $5" >&2; fail=1
    fi
}

check mdl_ai  Ox Ox mdl_leap_height 0
check mdl_ai  Ox Os mdl_leap_height 0
check mdl_ai  Ox O1 mdl_leap_height some
check weapons Ox Os mdl_slab 0
check weapons Ox O1 mdl_slab some
# A name that is not a proc in the listing -- bcc's are "_pl_damage", not
# "pl_damage" -- read as 0 mismatches over an empty function.
check mdl_ai  Ox Ox no_such_function some
# pl_damage moved from mdl_ai.c to combat.c (s29 -> s30): its constants sit
# at other offsets of another pool, so they compare by value. A pool byte
# changed must show; cuts without their pools compare offsets, not "const()".
check dmg     s29   s30     _pl_damage 0
check dmg     s29   s30pool _pl_damage some
check dmg     s29np s30np   _pl_damage some
# mdl_damage's gib compare swapped its operands (jge -> jle, the far table):
# one mismatch, and the walk goes on past it -- the udeath call, reached
# only through that compare, mutated must be a second.
check mdmg    s29   s30     _mdl_damage 1
check mdmg    s29   s30call _mdl_damage 2

[[ $fail == 0 ]] && echo "ok: -Os reads identical to -Ox, -O1 does not"
exit $fail
