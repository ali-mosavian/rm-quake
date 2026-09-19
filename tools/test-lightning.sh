#!/bin/bash
# Regression test: all three id lightning paths draw and the player can
# pick up, fire, hear, and drown with the Thunderbolt.
set -uo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/.." && pwd)"
OUT="${1:?usage: test-lightning.sh <build-dir>}"
RUN="${RUN_SH:-$HERE/run.sh}"

[[ -f "$OUT/dm3ish.qmp" && -f "$OUT/e1m1.qmp" ]] || {
    echo "SKIP: dm3ish.qmp or e1m1.qmp is not staged in $OUT"
    exit 0
}
cp "$ROOT/data/maps/e1m7/e1m7.qmp" "$OUT/"

run() { CYCLES=max TIMEOUT="${TIMEOUT:-240}" "$RUN" "$OUT" "$* -nostats -bench 4000" >/dev/null 2>&1; }
kit() { tr -d '\r' < "$OUT/cstep.txt" 2>/dev/null | sed -n 's/^fight=/ /p' | head -1; }
fld() { sed -n "s/.* $1 \([0-9-]*\).*/\1/p" <<< "$2"; }
beamfld() {
    tr -d '\r' < "$OUT/cstep.txt" 2>/dev/null |
        sed -n "s/^beams .*$1=\([0-9][0-9]*\).*/\1/p" | head -1
}
seen() {
    local hex byte
    hex=$(tr -d '\r' < "$OUT/cstep.txt" | sed -n 's/^snd seen=\([0-9A-F]*\).*/\1/p' | head -1)
    byte=${hex:$(( ($1 >> 3) * 2 )):2}
    [[ -n "$byte" ]] && (( 0x$byte & (1 << ($1 & 7)) ))
}
carry() {
    python3 - "$OUT/CARRY.BIN" "$1" <<'PY'
import struct
import sys

open(sys.argv[1], "wb").write(struct.pack(
    "<lhhfhhhhhhh", 1 | 256 | 4096, 100, 0, 0.0, 25, 0, 0,
    256, 0, 0, int(sys.argv[2])
))
PY
}

# weapon_lightning at (632,-136,-88), resting on the pool floor.
run "dm3ish.qmp -nostats -noai -nosound -at 632 -136 -104 -ticks 30"
k=$(kit)
items=$(fld items "$k")
if [[ -z "$items" || $(( items & 256 )) -eq 0 || "$(fld weapon "$k")" -ne 256 ||
      "$(fld cells "$k")" -ne 15 ]]; then
    echo "FAIL: the dm3ish Thunderbolt pickup gave [$k], wanted weapon 256 and 15 cells" >&2
    exit 1
fi

# One 30-point shot kills the 25-health dog. Eight ticks leave the keyed
# bolt2 beam and blood particles alive, and spend two cells at 10 Hz.
carry 3
KEEP_CARRY=1 run "e1m1.qmp -nostats -noai -carry -at 88 1420 -190 -yaw 270 -fire -ticks 8"
k=$(kit)
beam=$(tr -d '\r' < "$OUT/cstep.txt" 2>/dev/null | grep -m1 '^beams live=')
particles=$(tr -d '\r' < "$OUT/cstep.txt" 2>/dev/null | sed -n 's/^particles live=\([0-9]*\).*/\1/p' | head -1)
if [[ "$(fld kills "$k")" -lt 1 || "$(fld cells "$k")" -ne 1 ||
      "${particles:-0}" -lt 1 || "$(beamfld live)" -lt 1 || "$(beamfld drawn)" -lt 1 ]] ||
   ! seen 177 || ! seen 178; then
    echo "FAIL: the 10 Hz Thunderbolt gave [$k] [$beam], wanted a kill, 1 cell and a drawn beam" >&2
    exit 1
fi
carry 3
KEEP_CARRY=1 run "e1m1.qmp -noai -nomdl -nodlight -carry -at 88 1420 -190 -yaw 0 -fire -ticks 8"
cp "$OUT/BENCH.BMP" "$OUT/lightning-on.bmp" 2>/dev/null || {
    echo "FAIL: the lightning run wrote no frame" >&2
    exit 1
}

carry 3
KEEP_CARRY=1 run "e1m1.qmp -noai -nomdl -nobeam -nodlight -carry -at 88 1420 -190 -yaw 0 -fire -ticks 8"
px=$(python3 - "$OUT/lightning-on.bmp" "$OUT/BENCH.BMP" <<'PY'
import sys

def pixels(path):
    data = open(path, "rb").read()
    return data[int.from_bytes(data[10:14], "little"):]

a, b = pixels(sys.argv[1]), pixels(sys.argv[2])
print(sum(x != y for x, y in zip(a, b)))
PY
)
if [[ ${px:-0} -lt 20 ]]; then
    echo "FAIL: the Thunderbolt beam changed $px pixels, wanted at least 20" >&2
    exit 1
fi

# W_FireLightning consumes the whole magazine above waterlevel 1. The
# player center is four units above the origin, so the 70-point blast deals 34.
carry 2
KEEP_CARRY=1 run "dm3ish.qmp -nostats -noai -carry -at 500 0 -60 -fire -ticks 8"
k=$(kit)
if [[ "$(fld cells "$k")" -ne 0 || "$(fld health "$k")" -ne 66 ]]; then
    echo "FAIL: the underwater discharge gave [$k], wanted 0 cells and 66 health" >&2
    exit 1
fi

# Preserve Chthon's bolt3 and the shambler's bolt regressions.
run "e1m7.qmp -noai -at 8 64 8 -warp 30 352 376 208 -warp 60 352 -248 208 -warp 330 824 64 160 -yaw 180 -ticks 345"
boss=$(beamfld boss); segments=$(beamfld seg)
[[ "${boss:-0}" -gt 0 && "$(beamfld live)" -eq 1 && "$(beamfld drawn)" -gt 0 &&
   "${segments:-0}" -ge 10 && "$(beamfld startz)" -eq 111 && "$(beamfld endz)" -eq 111 ]] || {
    echo "FAIL: Chthon bolt3 did not draw at the electrode height" >&2
    exit 1
}
cp "$OUT/BENCH.BMP" "$OUT/lightning-chthon.bmp"

run "e1m7.qmp -at 1544 100 24 -yaw 270 -ticks 120"
sham=$(beamfld sham)
[[ "${sham:-0}" -gt 0 ]] || {
    echo "FAIL: shambler submitted no bolt.mdl triangles" >&2
    exit 1
}
cp "$OUT/BENCH.BMP" "$OUT/lightning-shambler.bmp"

echo "ok: Thunderbolt pickup/beam/sound/damage ($px px), underwater discharge, Chthon=$boss, shambler=$sham"
