#!/bin/bash
# test-layout.sh -- the game state must not depend on how the build is laid out.
#
#   tools/test-layout.sh <build> <build made with CDEFS=-DQR_PROF=0>
#
# Both builds come from one source, so their host_state_sum lines must
# match. mdl_spawn handed `&ent->pos`, a far address, to pl_trace's near
# `start`: every monster's drop to the floor traced from whatever DGROUP
# held at that offset, and on e1m4 a different soldier stood somewhere
# else, or in mid-air, in every build -- the frame and the player's
# position identical throughout.
set -uo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
A="$(cd "${1:?usage: test-layout.sh <build> <noprof-build>}" && pwd)"
B="$(cd "${2:?usage: test-layout.sh <build> <noprof-build>}" && pwd)"
CASES=(
    "e1m4-still|e1m4|-noai -ticks 120"
    "e1m1-live|e1m1|-ticks 600 -walk -fire"
)

for b in "$A" "$B"; do
    for c in "${CASES[@]}"; do
        IFS='|' read -r n m x <<< "$c"; d="$b/layout-$n"
        rm -rf "$d"; mkdir -p "$d"
        cp "$b/QCPORT.EXE" "$b/stuff.ini" "$ROOT/data/maps/$m/$m.qmp" "$d/"
        TIMEOUT="${TIMEOUT:-600}" "$ROOT/tools/run.sh" "$d" "$m.qmp -lm -nostats -nosound $x" > /dev/null 2>&1 &
    done
done
wait

fail=0
for c in "${CASES[@]}"; do
    IFS='|' read -r n _ _ <<< "$c"
    sa=$(tr -d '\r' < "$A/layout-$n/cstep.txt" 2>/dev/null | grep '^sum ')
    sb=$(tr -d '\r' < "$B/layout-$n/cstep.txt" 2>/dev/null | grep '^sum ')
    if [[ -z "$sa" ]]; then echo "FAIL $n: no sum lines"; fail=1; continue; fi
    if [[ "$sa" != "$sb" ]]; then echo "FAIL $n:"; diff <(echo "$sa") <(echo "$sb") | head -6; fail=1; continue; fi
    echo "ok $n: $(grep -c . <<< "$sa") sum lines identical"
done
exit $fail
