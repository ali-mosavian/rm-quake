#!/bin/bash
# Regression test: the map's monsters draw.
#
# Two things went wrong here and the first hid the second. d_draw_models
# passed `(BspVec3 *) &e->pos` -- world->mon is FAR, and a far-to-near
# cast drops the segment silently, so the frustum test read the box out
# of DS and rejected all ten monsters: 0 of 10 past the frustum with the
# soldier 100 units in front of the camera. And rdr.no_mdl was never
# assigned from args.no_mdl, so -nomdl drew the monsters too and the A/B
# that should have caught it reported IDENTICAL either way.
#
# A third failure reused the EMS vertex pointer after qglRsPoly mapped the
# destination over its slot, so every triangle after the first took its
# lighting normal from framebuffer bytes. The frame hash catches that too.
#
# Hence both assertions: mdl= says triangles were submitted, the frame
# against -nomdl says they reached the screen, and the -nomdl arm's
# mdl=0 is what keeps the second from being vacuous.
#
#   tools/test-models.sh build/cport-e1m1
set -uo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
OUT="${1:?usage: test-models.sh <build-dir>}"
MIN="${MIN:-100}"

[[ -f "$OUT/e1m1.qmp" ]] || { echo "SKIP: no e1m1.qmp in $OUT"; exit 0; }

run() { TIMEOUT="${TIMEOUT:-200}" "${RUN_SH:-$HERE/run.sh}" "$OUT" "$@" >/dev/null 2>&1; }
mdl() { tr -d '\r' < "$OUT/cstep.txt" 2>/dev/null | sed -n 's/^frames=.* mdl=\([0-9]*\).*/\1/p' | head -1; }

# The soldier at (8,1520,-200) and the dog at (88,1520,-200), 100 units
# ahead of the camera: -yaw is the map's angle mirrored, so +y is 270.
# -noai: the monsters must stand still, or the two arms render
# different motion and the pixel count stops meaning the models.
AT="-at 88 1420 -190 -yaw 270 -noai -ticks 4"
run "e1m1.qmp -nostats $AT"        || true
cp "$OUT/BENCH.BMP" "$OUT/mdl-on.bmp" 2>/dev/null || { echo "FAIL: no frame written" >&2; exit 1; }
frame_md5=$(md5 -q "$OUT/mdl-on.bmp" 2>/dev/null || md5sum "$OUT/mdl-on.bmp" | cut -d' ' -f1)
on=$(mdl)
run "e1m1.qmp -nostats -nomdl $AT" || true
off=$(mdl)

diff_px=$(python3 - "$OUT/mdl-on.bmp" "$OUT/BENCH.BMP" <<'PY'
import sys
def px(p):
    d = open(p, "rb").read()
    return d[int.from_bytes(d[10:14], "little"):]
a, b = px(sys.argv[1]), px(sys.argv[2])
print(sum(1 for x, y in zip(a, b) if x != y))
PY
)

fail=0
if [[ "$frame_md5" != 90f20ed0ce82586bac6e28ab0ed8afb1 ]]; then
    echo "FAIL: model frame is $frame_md5; alias projection or lighting changed" >&2
    fail=1
fi

# The first view is wholly inside the frustum. These two exercise the
# same id winding through side-plane and near-plane clipping.
for arm in \
    "side|-at -85 1420 -190 -yaw 270|d86e6e442a7a79729497273969c0b52d" \
    "near|-at 8 1500 -190 -yaw 270|c52eb84131e815c7acc475b6314f4c79"
do
    IFS='|' read -r name at expected <<< "$arm"
    run "e1m1.qmp -nostats -noai -nosound $at -ticks 4" || true
    got=$(md5 -q "$OUT/BENCH.BMP" 2>/dev/null || md5sum "$OUT/BENCH.BMP" | cut -d' ' -f1)
    if [[ "$got" != "$expected" ]]; then
        echo "FAIL: $name-clipped alias frame is $got; winding or clipping changed" >&2
        fail=1
    fi
done
if [[ "${on:-0}" -lt 1 ]]; then
    echo "FAIL: mdl=$on triangles submitted with two monsters in front of the camera" >&2
    fail=1
fi
if [[ "${off:-1}" -ne 0 ]]; then
    echo "FAIL: -nomdl still submitted mdl=$off triangles, so the frame A/B proves nothing" >&2
    fail=1
fi
if [[ ${diff_px:-0} -lt $MIN ]]; then
    echo "FAIL: the monsters put $diff_px pixels on screen, wanted at least $MIN" >&2
    fail=1
fi

[[ $fail -eq 0 ]] && echo "ok: monsters draw -- mdl=$on tris, $diff_px pixels against -nomdl (mdl=$off)"
exit $fail
