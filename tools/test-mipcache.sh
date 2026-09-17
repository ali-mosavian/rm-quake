#!/bin/bash
# Regression test: a cached surface must use the mip selected at the
# current viewpoint, not the mip most recently built for that face.
#
# The warm arm first sees the floor at (-584,-633,120), then warps to
# (-503,-352,120). The cold arm starts at the final viewpoint. With the
# old sticky-mip policy the warm arm kept mip 1 where the cold arm chose
# mip 0, changing 7,196 of 12,160 pixels. Static-world flags make cache
# history the only difference between the final frames.
#
#   tools/test-mipcache.sh build/cport
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
OUT="${1:?usage: test-mipcache.sh <build-dir>}"
COMMON="-lm -yaw 284 -ticks 90 -noview -nomdl -noents -noai -nodlight -nostyles"

run() { # <name> <position-and-warps>
    local name="$1" args="$2" qargs
    qargs="dm3ish.qmp $args $COMMON"
    if (( ${#qargs} > 127 )); then
        echo "FAIL: DOS command tail is ${#qargs} bytes, maximum is 127" >&2
        exit 1
    fi
    TIMEOUT="${TIMEOUT:-180}" "${RUN_SH:-$HERE/run.sh}" "$OUT" \
        "$qargs" >/dev/null 2>&1
    cp "$OUT/BENCH.BMP" "$OUT/mipcache-$name.bmp"
}

run cold "-at -503 -352 120"
run warm "-at -584 -633 120 -warp 60 -503 -352 120"

if cmp -s "$OUT/mipcache-cold.bmp" "$OUT/mipcache-warm.bmp"; then
    echo "ok: cached surface matches the cold mip at the final viewpoint"
    exit 0
fi

echo "FAIL: cached surface depends on the viewpoint that first built it" >&2
python3 "$HERE/imgdiff.py" "$OUT/mipcache-cold.bmp" "$OUT/mipcache-warm.bmp" >&2 || true
exit 1
