#!/bin/bash
# Regression test: solid actors block movement. The live e1m1 dog used to
# let the player walk from y=1420 straight through its y=1520 bounding box.
# Killing the dog must remove that obstruction.
#
#   tools/test-actor-collision.sh build/cport-e1m1
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/.." && pwd)"
OUT="${1:?usage: test-actor-collision.sh <build-dir>}"

[[ -f "$ROOT/data/maps/e1m1/e1m1.qmp" ]] || { echo "SKIP: no e1m1.qmp fixture"; exit 0; }
cp "$ROOT/data/maps/e1m1/e1m1.qmp" "$OUT/"

run() {
    TIMEOUT="${TIMEOUT:-400}" "${RUN_SH:-$HERE/run.sh}" "$OUT" "$@" >/dev/null 2>&1
    grep -q '^frames=' "$OUT/cstep.txt" || {
        echo "FAIL: actor-collision guest run did not complete" >&2
        tr -d '\r' < "$OUT/ERROR.LOG" >&2 2>/dev/null || true
        return 1
    }
}
summary() { tr -d '\r' < "$OUT/cstep.txt" 2>/dev/null | grep -m1 '^frames='; }
fight() { tr -d '\r' < "$OUT/cstep.txt" 2>/dev/null | grep -m1 '^fight='; }
map_y() { sed -n 's/.*pos=[-0-9]*,[-0-9]*,\([-0-9]*\).*/\1/p' <<< "$1"; }
field() { sed -n "s/.* $1 \([0-9-]*\).*/\1/p" <<< "$2"; }
metric() { sed -n "s/.*$1=\([0-9-]*\).*/\1/p" <<< "$2"; }
health() { sed -n 's/^fight=health \([0-9-]*\).*/\1/p' <<< "$1"; }

fail=0

run "e1m1.qmp -nostats -noai -nosound -at 88 1420 -190 -yaw 270 -walk -ticks 120"
grep -q '^mdl_leap_landing_selftest 1$' "$OUT/cstep.txt" || {
    echo "FAIL: production leap landing did not retry safely" >&2
    exit 1
}
live=$(summary)
live_y=$(map_y "$live")
cp "$OUT/BENCH.BMP" "$OUT/actor-collision-live.bmp" 2>/dev/null || {
    echo "FAIL: live-dog run wrote no frame" >&2
    exit 1
}
if [[ -z "$live_y" || "$live_y" -lt 1450 || "$live_y" -gt 1490 ]]; then
    echo "FAIL: player walked through the live dog: [$live], wanted map y 1450..1490" >&2
    fail=1
fi

run "e1m1.qmp -nostats -noai -nosound -at 88 1420 -190 -yaw 270 -fire -ticks 180"
dead=$(summary)
dead_y=$(map_y "$dead")
combat=$(fight)
if [[ -z "$dead_y" || "$dead_y" -le 1600 || "$(field kills "$combat")" -lt 1 ]]; then
    echo "FAIL: killed dog still blocked passage: [$dead] [$combat]" >&2
    fail=1
fi

# At 150 units the dog chooses its leap. Actor clipping stops it at the
# box edge, so Dog_JumpTouch must use the reported actor contact rather
# than waiting for the two boxes to overlap.
run "e1m1.qmp -nostats -nosound -at 88 1370 -190 -yaw 270 -ticks 50"
leap=$(tr -d '\r' < "$OUT/cstep.txt" 2>/dev/null | grep -m1 '^ai ')
leap_fight=$(fight)
if [[ "$(metric leaps "$leap")" -lt 1 || "$(health "$leap_fight")" -ge 90 ]]; then
    echo "FAIL: dog leap made no contact: [$leap] [$leap_fight]" >&2
    fail=1
fi

[[ $fail -eq 0 ]] && echo "ok: live dog stopped at y=$live_y; dead dog passed at y=$dead_y"
grep -q '^map_release ok ' "$OUT/cstep.txt" || {
    echo "FAIL: actor-collision run did not release the map" >&2
    exit 1
}
if [[ -s "$OUT/ERROR.LOG" ]]; then
    echo "FAIL: actor-collision run left an error log" >&2
    exit 1
fi
exit $fail
