#!/bin/bash
# Regression test: Chthon's electrodes and a shambler attack must submit
# their original bolt alias models. Both attacks dealt damage while drawing
# nothing, so sound and health assertions could pass on an empty scene.
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/.." && pwd)"
OUT="${1:?usage: test-lightning.sh <build-dir>}"
RUN="${RUN_SH:-$HERE/run.sh}"

cp "$ROOT/data/maps/e1m7/e1m7.qmp" "$OUT/"

run() {
    CYCLES=max TIMEOUT="${TIMEOUT:-45}" "$RUN" "$OUT" "$* -nostats -bench 4000" >/dev/null 2>&1
}
field() {
    tr -d '\r' < "$OUT/cstep.txt" 2>/dev/null |
        sed -n "s/^beams .*$1=\([0-9][0-9]*\).*/\1/p" | head -1
}

# The first two plates raise the electrodes; touching the third at tick 330
# fires bolt3.mdl. Five ticks later it is still inside id's 0.2 s lifetime.
run "e1m7.qmp -noai -at 8 64 8 -warp 30 352 376 208 -warp 60 352 -248 208 -warp 330 824 64 160 -yaw 180 -ticks 345"
boss=$(field boss)
live=$(field live)
drawn=$(field drawn)
segments=$(field seg)
startz=$(field startz)
endz=$(field endz)
[[ "${boss:-0}" -gt 0 && "${live:-0}" -eq 1 && "${drawn:-0}" -gt 0 && "${segments:-0}" -ge 10 ]] || {
    echo "FAIL: Chthon beam boss=${boss:-none} live=${live:-none} drawn=${drawn:-none} seg=${segments:-none}" >&2
    exit 1
}
[[ "$startz" == 111 && "$endz" == 111 ]] || {
    echo "FAIL: Chthon beam z=${startz:-none},${endz:-none}; wanted 111,111" >&2
    exit 1
}
cp "$OUT/BENCH.BMP" "$OUT/lightning-chthon.bmp"

# The player is 300 units in front of e1m7's first always-spawned shambler:
# outside melee range and inside CastLightning's 600-unit trace.
run "e1m7.qmp -at 1544 100 24 -yaw 270 -ticks 60"
sham=$(field sham)
[[ "${sham:-0}" -gt 0 ]] || {
    echo "FAIL: shambler submitted ${sham:-no} bolt.mdl triangles" >&2
    exit 1
}

cp "$OUT/BENCH.BMP" "$OUT/lightning-shambler.bmp"

echo "ok: lightning Chthon=$boss triangles/$segments segments, shambler=$sham triangles"
