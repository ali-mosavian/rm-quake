#!/bin/bash
# Regression: the map's makestatic flame.mdl/flame2.mdl entities must draw.
# Keeping only FireAmbient left every torch and flame light visibly empty.
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/.." && pwd)"
OUT="$(cd "${1:?usage: test-flames.sh <build-dir>}" && pwd)"

cp "$ROOT/data/maps/start/start.qmp" "$OUT/"

python3 - "$HERE" "$OUT/start.qmp" <<'PY'
import struct
import sys

sys.path.insert(0, sys.argv[1])
import qmapread

data = qmapread.read(sys.argv[2], "air.bin")
air = struct.unpack_from("<4H", data)
at = 8 + sum(air) * 12
counts = struct.unpack_from("<4H", data, at)
if counts != (41, 41, 41, 41):
    raise SystemExit(f"FAIL: start flame counts are {counts}, want four runs of 41")
if len(data) != at + 8 + sum(counts) * 14:
    raise SystemExit("FAIL: air.bin does not hold one 14-byte record per flame")
print("ok: start ships all 41 static flames")

effects = qmapread.read(sys.argv[2], "effects.bin")
if struct.unpack_from("<4sH", effects) != (b"QFX3", 24):
    raise SystemExit("FAIL: effects.bin has no 24-model QFX3 bank")
models = {}
for i in range(24):
    at = 14 + i * 76
    name = effects[at:at + 8].split(b"\0", 1)[0].decode()
    nframe = struct.unpack_from("<H", effects, at + 16)[0]
    nsets = struct.unpack_from("<H", effects, at + 46)[0]
    sets = tuple(effects[at + 48:at + 48 + nsets])
    models[name] = (nframe, sets)
if models.get("flame") != (6, (6,)):
    raise SystemExit(f"FAIL: flame.mdl groups are {models.get('flame')}, want 6")
if models.get("flame2") != (17, (6, 11)):
    raise SystemExit(f"FAIL: flame2.mdl groups are {models.get('flame2')}, want 6+11")
print("ok: QFX3 retains both original grouped flame animations")
PY

run() {
    local view="$1" extra="$2"
    CYCLES=max TIMEOUT="${TIMEOUT:-120}" "${RUN_SH:-$HERE/run.sh}" "$OUT" \
        "start.qmp -lm -nostats -nosound -noai -nostyles $view -ticks 2 $extra" \
        >/dev/null
}

wall_view="-at 401 737 -39 -yaw 254 -pitch 0"
run "$wall_view" ""
cp "$OUT/BENCH.BMP" "$OUT/flames-on.bmp"
grep -q '^d_alias_selftest 1$' "$OUT/cstep.txt" || {
    echo "FAIL: flame animation group wrapping failed d_alias_selftest" >&2
    exit 1
}
line=$(tr -d '\r' < "$OUT/cstep.txt" | grep '^flames live=' | tail -1 || true)
live=$(sed -n 's/^flames live=\([0-9][0-9]*\).*/\1/p' <<< "$line")
drawn=$(sed -n 's/.* drawn=\([0-9][0-9]*\).*/\1/p' <<< "$line")
if [[ "${live:-0}" -ne 41 || "${drawn:-0}" -lt 1 ]]; then
    echo "FAIL: the visible wall torch did not draw [$line]" >&2
    exit 1
fi

run "$wall_view" "-noflame"
cmp -s "$OUT/flames-on.bmp" "$OUT/BENCH.BMP" && {
    echo "FAIL: removing flames changed no pixels" >&2
    exit 1
}
echo "ok: $drawn flame triangles change the verified start view"

large_view="-at 81 513 41 -yaw 344 -pitch 137"
run "$large_view" ""
cp "$OUT/BENCH.BMP" "$OUT/flame2-on.bmp"
line=$(tr -d '\r' < "$OUT/cstep.txt" | grep '^flames live=' | tail -1 || true)
drawn=$(sed -n 's/.* drawn=\([0-9][0-9]*\).*/\1/p' <<< "$line")
run "$large_view" "-noflame"
cmp -s "$OUT/flame2-on.bmp" "$OUT/BENCH.BMP" && {
    echo "FAIL: the visible large flame2 changed no pixels" >&2
    exit 1
}
if [[ "${drawn:-0}" -lt 1 ]]; then
    echo "FAIL: the visible large flame2 did not draw [$line]" >&2
    exit 1
fi
echo "ok: $drawn flame2 triangles change the verified large-flame view"
