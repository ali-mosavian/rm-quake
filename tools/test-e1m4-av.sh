#!/bin/bash
# Regression test: e1m4 must keep both sound and lightmapped animated surfaces.
#
# Its 6,120 faces fragmented conventional memory until the 21,440-byte DSP
# block failed, leaving the second attract-demo map silent. Separately,
# d_faces rejected every animated texture from the surface cache, so the six
# animated lightmapped faces were always drawn fullbright. The button below
# was palette index 248 both with and without -lm.
#
#   tools/test-e1m4-av.sh build/cport-e1m4
set -uo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
OUT="${1:?usage: test-e1m4-av.sh <build-dir>}"
RUN="${RUN_SH:-$HERE/run.sh}"

[[ -f "$OUT/e1m4.qmp" ]] || { echo "SKIP: no e1m4.qmp in $OUT"; exit 0; }

run() {
    CYCLES="${CYCLES:-max}" TIMEOUT="${TIMEOUT:-60}" "$RUN" "$OUT" "e1m4.qmp $1" >/dev/null 2>&1
}

fail=0
run "-lm -nostats -noai -fire -ticks 8"
log="$OUT/cstep.txt"
loops=$(tr -d '\r' < "$log" 2>/dev/null | sed -n 's/^snd loops=\([0-9-]*\).*/\1/p' | head -1)
started=$(tr -d '\r' < "$log" 2>/dev/null | sed -n 's/^snd started=\([0-9-]*\).*/\1/p' | head -1)
stack=$(tr -d '\r' < "$log" 2>/dev/null | sed -n 's/^snd seen=.* stack=\([0-9-]*\).*/\1/p' | head -1)
if ! grep -q '^sc_init ok ' "$log" 2>/dev/null; then
    echo "FAIL: e1m4 lost its lightmap cache" >&2
    fail=1
fi
if (( ${loops:-0} < 1 || ${started:-0} < 1 || ${stack:-0} < 1 )); then
    echo "FAIL: e1m4 sound loops=${loops:-none} started=${started:-none} stack=${stack:-none}" >&2
    fail=1
fi

VIEW="-nostyles -nostats -noai -nosound -at -256 1640 1256 -yaw 0 -pitch 90"
run "-lm $VIEW -warp 3 -256 1640 1256 -ticks 4"
cp "$OUT/BENCH.BMP" "$OUT/e1m4-animated-lit.bmp" 2>/dev/null || {
    echo "FAIL: lit arm wrote no frame" >&2
    exit 1
}
run "-lm $VIEW -warp 23 -256 1640 1256 -ticks 24"
cp "$OUT/BENCH.BMP" "$OUT/e1m4-animated-later.bmp" 2>/dev/null || {
    echo "FAIL: later animation arm wrote no frame" >&2
    exit 1
}
run "$VIEW -warp 3 -256 1640 1256 -ticks 4"
cp "$OUT/BENCH.BMP" "$OUT/e1m4-animated-unlit.bmp" 2>/dev/null || {
    echo "FAIL: unlit arm wrote no frame" >&2
    exit 1
}

read -r lit later unlit <<< "$(python3 - "$OUT/e1m4-animated-lit.bmp" "$OUT/e1m4-animated-later.bmp" "$OUT/e1m4-animated-unlit.bmp" <<'PY'
import struct
import sys


def pixel(path: str, x: int, y: int) -> int:
    data = open(path, "rb").read()
    offset = struct.unpack_from("<I", data, 10)[0]
    width, height = struct.unpack_from("<ii", data, 18)
    row = (width + 3) & ~3
    line = abs(height) - 1 - y if height > 0 else y
    return data[offset + line * row + x]


print(*(pixel(path, 168, 96) for path in sys.argv[1:]))
PY
)"
if [[ -z "$lit" || "$lit" == "$unlit" ]]; then
    echo "FAIL: animated button is index ${lit:-missing} with -lm and ${unlit:-missing} without it" >&2
    fail=1
fi
if [[ -z "$later" || "$later" == "$lit" ]]; then
    echo "FAIL: cached animated button froze at index ${lit:-missing}; later frame is ${later:-missing}" >&2
    fail=1
fi

[[ $fail -eq 0 ]] && echo "ok: e1m4 sound loops=$loops started=$started stack=$stack; button unlit=$unlit, animated lit=$lit->$later"
exit $fail
