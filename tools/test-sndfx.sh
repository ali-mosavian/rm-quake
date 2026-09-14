#!/bin/bash
# Regression test: episode 1's world, player and weapon sounds reach the
# mixer, read from the exit line `snd seen=<id bitmap> live=<ids> wraps=N
# water=V sky=V`.
#
#   1. e1m1's torches and fluoros loop: snd_loops is the map's ambient
#      count (5 before: comp_hum and the drone only), and a leaf with
#      water and sky nibbles fades its two ambients in.
#   2. e1m4's 71 statics all register past the old 32 channels, and its
#      trains start no plat sound: sharing the plat array, they started
#      ids 100/101 twice a tick, 60 starts in 30 ticks standing still.
#   3. A door's move loop ends at its stop: after the double door and
#      the button's door shut, no door move id is live.
#   4. Nails on a wall tink or ricochet.
#   5. Under water: inh2o on entry, drown past 12 s, health down.
#   6. Teleport fog is play_teleport's, 0.2 s after: none 0.1 s after
#      e1m1's water teleporter, some 0.5 s after.
#   7. Monsters teleport: e1m6's three closet wizards stay put until
#      their trigger_once, then go, with fog. A named teleporter took
#      nothing before; unnamed ones took only the player.
#   8. start's crucified zombies moan idle_w2, and shot at they neither
#      hunt nor walk: they spawned as ordinary zombies.
#   9. Chthon: the rune wakes him, two plates raise the electrodes, the
#      third fires the bolt -- power, pain, death. The bolt did nothing.
#
#   tools/test-sndfx.sh build/cport-snd      (ARMS="1 3" for some)
set -uo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/.." && pwd)"
OUT="${1:?usage: test-sndfx.sh <build-dir>}"
RUN="${RUN_SH:-$HERE/run.sh}"
ARMS="${ARMS:-1 2 3 4 5 6 7 8 9}"
log="$OUT/cstep.txt"
fail=0

for m in e1m1 e1m4 e1m6 e1m7 start; do
    [[ -f "$OUT/$m.qmp" ]] || cp "$ROOT/data/maps/$m/$m.qmp" "$OUT/" 2>/dev/null
done
cp "$ROOT/data/stuff.ini" "$OUT/" 2>/dev/null

run() { TIMEOUT="${TIMEOUT:-600}" "$RUN" "$OUT" "$* -nostats -bench 4000" >/dev/null 2>&1; }
line() { tr -d '\r' < "$log" 2>/dev/null | grep -m1 "^$1"; }
field() { line "$1" | sed -n "s/.* $2=\([^ ]*\).*/\1/p"; }
# seen <id>: the id's bit in the hex bitmap, byte id>>3 first
seen() {
    local hex byte; hex=$(line "snd seen=" | sed 's/snd seen=\([0-9A-F]*\).*/\1/')
    byte=${hex:$(( ($1 >> 3) * 2 )):2}
    [[ -n "$byte" ]] || return 1
    (( 0x$byte & (1 << ($1 & 7)) ))
}
live() { field "snd seen=" live | tr ',' '\n' | grep -qx "$1"; }
fog() { seen 106 || seen 107 || seen 108 || seen 109 || seen 110; }
bad() { echo "FAIL: $*" >&2; fail=1; }

if [[ " $ARMS " == *" 1 "* ]]; then
    run "e1m1.qmp -noai -at 88 1420 -190 -yaw 270 -ticks 120"
    # counted from the bsp on easy: 4 comp_hum, a drone, 6 fluoros, 2 sparks
    want=13; loops=$(field "snd started=" loops)
    [[ "${loops:-x}" == "$want" ]] || bad "e1m1 loops=${loops:-none}, the map has $want ambients"
    w=$(field "snd seen=" water); s=$(field "snd seen=" sky)
    [[ "${w:-0}" -gt 0 && "${s:-0}" -gt 0 ]] || bad "leaf ambients water=${w:-none} sky=${s:-none} in a water+sky leaf"
fi

if [[ " $ARMS " == *" 2 "* ]]; then
    run "e1m4.qmp -noai -ticks 30"
    # 29 drips, 2 swamps, 40 torches and flames
    want=71; loops=$(field "snd started=" loops)
    [[ "${loops:-x}" == "$want" ]] || bad "e1m4 loops=${loops:-none}, the map has $want ambients"
    for id in 100 101 102 103 104 105; do
        if seen $id || [[ -z "$(line 'snd seen=')" ]]; then bad "e1m4 at rest started plat id $id (started=$(field 'snd started=' started))"; break; fi
    done
fi

if [[ " $ARMS " == *" 3 "* ]]; then
    run "e1m1.qmp -noai -at 330 576 40 -yaw 180 -walk -ticks 1200"
    { seen 47 && seen 46; } || bad "the double door never played hydro1/hydro2"
    for id in 45 47 49 51; do live $id && bad "door move $id still live 20 s after the walk"; done
    [[ "$(field 'snd seen=' wraps)" -gt 0 ]] 2>/dev/null || bad "no looping channel wrapped"
fi

if [[ " $ARMS " == *" 4 "* ]]; then
    run "e1m1.qmp -noai -at 112 2352 16 -yaw 270 -fire -ticks 120"
    seen 138 || seen 139 || seen 140 || seen 141 || bad "nails hit the wall with no tink or ricochet"
fi

if [[ " $ARMS " == *" 5 "* ]]; then
    run "e1m1.qmp -noai -at 600 968 -344 -ticks 800"
    seen 128 || bad "no inh2o on entering water"
    seen 124 || seen 125 || bad "13 s under water with no drown sound"
    hp=$(line "fight=" | sed 's/fight=health \([0-9-]*\).*/\1/')
    [[ "${hp:-100}" -lt 100 ]] || bad "13 s under water left health ${hp:-none}"
fi

if [[ " $ARMS " == *" 6 "* ]]; then
    run "e1m1.qmp -noai -at 1312 1088 -368 -ticks 6"
    px=$(line "frames=" | sed -n 's/.* pos=\([-0-9]*\),.*/\1/p')
    [[ -n "$px" && "$px" != 1312 ]] || bad "the player never left e1m1's teleporter (pos x ${px:-none})"
    fog && bad "teleport fog started 0.1 s after the teleport"
    run "e1m1.qmp -noai -at 1312 1088 -368 -ticks 30"
    fog || bad "no teleport fog 0.5 s after the teleport"
fi

if [[ " $ARMS " == *" 7 "* ]]; then
    run "e1m6.qmp -ticks 120"
    t=$(field "ai hunting=" teles)
    [[ "${t:-x}" == 0 ]] || bad "e1m6 from its spawn: teles=${t:-none}, want 0 before any teleporter is used"
    run "e1m6.qmp -at -96 1408 224 -ticks 120"
    t=$(field "ai hunting=" teles)
    [[ "${t:-0}" -ge 3 ]] || bad "e1m6's closet wizards teleported ${t:-no} times, want 3"
    fog || bad "no fog for the wizards' teleport"
fi

if [[ " $ARMS " == *" 8 "* ]]; then
    run "start.qmp -at 940 988 -152 -yaw 317 -pitch 14 -fire -ticks 1200"
    seen 168 || bad "start's crucified zombies never played idle_w2"
    h=$(field "ai hunting=" hunting); mv=$(field "ai hunting=" moved)
    [[ "${h:-x}" == 0 && "${mv:-x}" == 0 ]] || bad "crucified zombies hunting=${h:-none} moved=${mv:-none}, want 0 and 0"
fi

if [[ " $ARMS " == *" 9 "* ]]; then
    run "e1m7.qmp -noai -at 8 64 8 -warp 30 352 376 208 -warp 60 352 -248 208 -warp 330 824 64 160 -ticks 600"
    seen 165 || bad "the bolt played no misc/power"
    seen 166 || bad "Chthon took no point (boss1/pain)"
    seen 167 || bad "Chthon did not die (boss1/death)"
fi

[[ $fail -eq 0 ]] && echo "ok: arms $ARMS"
exit $fail
