#!/bin/bash
# Regression test: every surface-cache size class needed by e1m1 must get
# its view. The cache metadata once left too little conventional memory for
# the last lazy qglNewView allocation; that face silently drew unlit from the
# unlit shaded atlas, producing bright seams and apparently missing surfaces.
set -uo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
OUT="${1:?usage: test-surface-cache-views.sh <build-dir>}"
WANT="3f2bc5b16455ef3f5813498ece9788c7"

[[ -f "$OUT/e1m1.qmp" ]] || { echo "SKIP: no e1m1.qmp in $OUT"; exit 0; }

rm -f "$OUT/BENCH.BMP"
TIMEOUT="${TIMEOUT:-300}" "${RUN_SH:-$HERE/run.sh}" "$OUT" \
    "e1m1.qmp -lm -nomdl -noitems -noview -noai -nostats -at 20 1912 -183 -yaw 228 -ticks 4" \
    >/dev/null 2>&1

[[ -s "$OUT/BENCH.BMP" ]] || { echo "FAIL: no frame written" >&2; exit 1; }
got=$(md5 -q "$OUT/BENCH.BMP" 2>/dev/null || md5sum "$OUT/BENCH.BMP" | cut -d' ' -f1)
if [[ "$got" != "$WANT" ]]; then
    echo "FAIL: e1m1 cache-view frame $got, wanted $WANT" >&2
    exit 1
fi

echo "ok: every e1m1 surface-cache class has a lit view"
