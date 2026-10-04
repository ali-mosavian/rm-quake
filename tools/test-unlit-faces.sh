#!/bin/bash
# Regression test: a face with no lightmap is dark, not full bright.
#
# id's compiler writes lightofs -1 for a face no light reached -- 12 to 18%
# of the faces of e1m1..e1m8, none of dm3ish's. d_faces.c gave only faces
# with a lightmap a cache surface, so those drew as plain textures at
# colormap row 0, and the attract demo showed door jambs and torch
# brackets as bright yellow-green slabs in dark halls.
#
# sc_unlit counts lit-eligible faces drawn without a surface; it must be 0.
# sc_builds > 0 keeps a run with no cache from passing. The symptom is
# checked too: two attract-demo poses (demo1 at 37 s and 50 s) where the
# jamb faces 4229, 4184 and 1603 filled a region at luminance 24 and 62;
# lit by nothing they must be under 15. -pitch is 90 plus degrees down.
#
#   tools/test-unlit-faces.sh build/cport
set -uo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/.." && pwd)"
OUT="${1:?usage: test-unlit-faces.sh <build-dir>}"

field() { tr -d '\r' < "$OUT/BENCH.TXT" 2>/dev/null | awk -v k="$1" '$1==k{print $2}'; }

fail=""
for map in e1m3 e1m4; do
    cp "$ROOT/data/maps/$map/$map.qmp" "$OUT/"
    TIMEOUT="${TIMEOUT:-300}" "${RUN_SH:-$HERE/run.sh}" "$OUT" \
        "$map.qmp -lm -nostats -nosound -noai -noitems -ticks 30" >/dev/null 2>&1
    unlit=$(field sc_unlit); builds=$(field sc_builds)
    [[ -n "$unlit" && "${builds:-0}" -gt 0 ]] || { fail+=" $map: no cache run (builds=${builds:-none});"; continue; }
    [[ "$unlit" == 0 ]] || fail+=" $map: $unlit faces drawn full bright;"
done
lum() { python3 - "$OUT/BENCH.BMP" "$1" <<'PY'
import sys
from PIL import Image
px = list(Image.open(sys.argv[1]).convert("RGB").crop(tuple(map(int, sys.argv[2].split(",")))).getdata())
print(int(sum(0.3 * r + 0.59 * g + 0.11 * b for r, g, b in px) / len(px)))
PY
}
for pose in "-1151 -730 -71 -yaw 224 -pitch 72|150,45,185,125" "-1303 -138 -71 -yaw 340 -pitch 87|0,25,40,110"; do
    TIMEOUT="${TIMEOUT:-300}" "${RUN_SH:-$HERE/run.sh}" "$OUT" \
        "e1m3.qmp -lm -nostats -nosound -noai -noitems -at ${pose%%|*} -ticks 30" >/dev/null 2>&1
    l=$(lum "${pose##*|}")
    [[ -n "$l" && "$l" -lt 15 ]] || fail+=" pose ${pose%%|*}: jamb region luminance ${l:-none}, want < 15;"
done
[[ -z "$fail" ]] || { echo "FAIL:$fail"; exit 1; }
echo "ok: no lightmap-less face drawn full bright on e1m3, e1m4; demo jambs dark"
