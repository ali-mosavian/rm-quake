#!/bin/bash
# Regression test: no rune was ever held. The sigil set nothing, start's episode
# gates shipped hidden and its boss gate solid, and start always spawned at
# info_player_start.
#
#   1. start spawns at info_player_start2 with a rune held, at info_player_start without.
#   2. episode 1's gate (*41) blocks its hall with rune 1 and is gone without it.
#   3. the boss gate (*38), the slab over the pit to end, goes with all four.
#   4. standing on e1m7's rune sets bit 1.
#   5. the bit carries: a second in e1m7's lava, then its exit, with -sflags 1 -skill 2,
#      starts start at start2 on skill 2 with a fresh kit's 100 health.
#
#   tools/test-serverflags.sh build/cport-x
set -uo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/.." && pwd)"
OUT="${1:?usage: test-serverflags.sh <build-dir>}"
RUN="${RUN_SH:-$HERE/run.sh}"
log="$OUT/cstep.txt"
fail=0

# always: a .qmp staged before the gate records existed loads nothing
cp "$ROOT/data/maps/start/start.qmp" "$ROOT/data/maps/e1m7/e1m7.qmp" "$ROOT/data/maps/e1m1/e1m1.qmp" "$OUT/"
cp "$ROOT/data/stuff.ini" "$OUT/" 2>/dev/null

run()  { TIMEOUT="${TIMEOUT:-300}" "$RUN" "$OUT" "$1 -nostats -noai -nosound" >/dev/null 2>&1; }
last() { tr -d '\r' < "$log" 2>/dev/null | grep "^$1" | tail -1; }
# pos is x, up, bsp y
where() { IFS=, read -r px pu py <<< "$(last 'frames=' | sed 's/.*pos=//')"; }

for arm in "0 288" "1 1536"; do
    read -r f want <<< "$arm"
    run "start.qmp -sflags $f -ticks 2"; where
    [[ "$px" == 544 && "$py" == "$want" ]] ||
        { echo "FAIL: start with -sflags $f spawns at ${px:-?},${py:-?}, not 544,$want" >&2; fail=1; }
done

# every map after e1m7 loads with rune 1 and has no start2: its own start still
run "e1m1.qmp -sflags 1 -ticks 2"; where
[[ "$px" == 480 && "$py" == -352 ]] ||
    { echo "FAIL: e1m1, which has no start2, spawns at ${px:-?},${py:-?} with a rune held, not 480,-352" >&2; fail=1; }

for f in 0 1; do
    run "start.qmp -sflags $f -at 344 1728 24 -yaw 180 -walk -ticks 120"; where
    if [[ -z "$px" ]] || { (( f == 0 )) && (( px >= 289 )); } || { (( f == 1 )) && (( px < 303 )); }; then
        echo "FAIL: walking west at episode 1's gate with -sflags $f ends at x ${px:-?}" >&2
        fail=1
    fi
done

# the floor under the slab is at z -98: an eye above 0 stands on it, one near -50 fell through
for f in 0 15; do
    run "start.qmp -sflags $f -at 544 1808 24 -ticks 60"; where
    if [[ -z "$pu" ]] || { (( f == 0 )) && (( pu <= 0 )); } || { (( f == 15 )) && (( pu > -30 )); }; then
        echo "FAIL: on the boss gate with -sflags $f the eye ends at ${pu:-?}" >&2
        fail=1
    fi
done

run "e1m7.qmp -at 8 64 24 -ticks 10"
sf=$(last 'sflags ' | awk '{print $2}')
[[ "$sf" == 1 ]] || { echo "FAIL: standing on e1m7's rune leaves sflags ${sf:-unset}, not 1" >&2; fail=1; }

# Chthon's lava at 404 64 -44 for one 0.2 s bite, then -warp into the exit volume *14;
# NEXT.BAT forwards no -warp. -fire walks as well as fires, so start's leg leaves start2:
# a second in it is past y 1536 only from start2, never from 288 at walking speed.
run "e1m7.qmp -sflags 1 -skill 2 -at 404 64 -44 -warp 10 1016 52 -1000 -fire -ticks 300"
map2=$(last 'asset_map ok ' | awk '{print $3}')
sf=$(last 'sflags ' | awk '{print $2}')
sk=$(last 'gs_state ' | sed -n 's/.* skill \([0-9]*\).*/\1/p')
hurt=$(tr -d '\r' < "$log" 2>/dev/null | grep -m1 '^fight=' | sed 's/.*health \([0-9-]*\).*/\1/')
hp=$(last 'fight=' | sed 's/.*health \([0-9-]*\).*/\1/')
IFS=, read -r sx sy sz <<< "$(last 'sum 60 ' | awk '{print $4}')"
if [[ "$map2" != start.qmp || "$sf" != 1 || "$sk" != 2 || -z "$sy" ]] || (( sx < 520 || sx > 570 || sy < 1536 )); then
    echo "FAIL: e1m7's exit with rune 1 on skill 2 opened ${map2:-nothing}: sflags ${sf:-?} skill ${sk:-?}, a second in at ${sx:-?},${sy:-?}; want start.qmp, 1, 2, past start2's 544,1536" >&2
    fail=1
fi
if [[ -z "$hurt" || "$hurt" -ge 100 || "$hp" != 100 ]]; then
    echo "FAIL: e1m7 ended on ${hurt:-?} health (want under 100) and start with a rune began on ${hp:-?}, not a fresh 100" >&2
    fail=1
fi

[[ $fail -eq 0 ]] && echo "ok: the rune is held, carried to start, and start's spawn and gates follow it"
exit $fail
