#!/bin/bash
# Pixel-exact regression for id's two-row single-player status bar.
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
OUT="$(cd "${1:?usage: test-sbar.sh <build-dir>}" && pwd)"
PAK="${PAK:-$HOME/dos/QUAKE_SW/ID1/PAK0.PAK}"

[[ -f "$OUT/dm3ish.qmp" ]] || { echo "SKIP: no dm3ish.qmp in $OUT"; exit 0; }
[[ -f "$PAK" ]] || { echo "SKIP: no PAK0.PAK at $PAK"; exit 0; }

work="$(mktemp -d "$OUT/sbar-test.XXXXXX")"
trap 'rm -rf "$work"' EXIT
cp "$OUT/QCPORT.EXE" "$OUT/dm3ish.qmp" "$OUT/stuff.ini" "$OUT/default.cfg" "$work/"

run() {
    local ticks="${2:-2}"
    TIMEOUT="${TIMEOUT:-300}" "$HERE/run.sh" "$work" \
        "dm3ish.qmp -nostats -nosound -noai -ticks $ticks $1" >/dev/null
}

run "+wait;screenshot;wait"
python3 "$HERE/check-sbar.py" "$PAK" "$work/SCRN0.BMP" normal

cp "$HERE/fixtures/showscores.cfg" "$work/autoexec.cfg"
run "+wait;screenshot;wait"
python3 "$HERE/check-sbar.py" "$PAK" "$work/SCRN0.BMP" scores

mv "$work/autoexec.cfg" "$work/showscores.cfg"
run "-sflags 15 +wait;screenshot;wait"
python3 "$HERE/check-sbar.py" "$PAK" "$work/SCRN0.BMP" normal 15

run "+wait;viewsize 50;wait;screenshot;wait" 4
python3 "$HERE/check-sbar.py" "$PAK" "$work/SCRN0.BMP" normal
python3 - "$work/SCRN0.BMP" <<'PY'
import struct
import sys

data = open(sys.argv[1], "rb").read()
offset = struct.unpack_from("<I", data, 10)[0]
width, height = struct.unpack_from("<ii", data, 18)
stride = (width + 3) & ~3


def pixel(x: int, y: int) -> int:
    return data[offset + (height - 1 - y) * stride + x]


outside = [
    pixel(x, y)
    for y in range(152)
    for x in range(320)
    if not (80 <= x < 240 and 26 <= y < 126)
]
inside = [pixel(x, y) for y in range(26, 126) for x in range(80, 240)]
if any(outside) or sum(value != 0 for value in inside) < 100:
    raise SystemExit("FAIL: viewsize 50 is not a centered 160x100 view above the bar")
print("ok: viewsize 50 is a centered 160x100 view above the bar")
PY
grep -q '^map_release ok ' "$work/cstep.txt"
grep -q '^restored$' "$work/cstep.txt"
[[ ! -s "$work/error.log" ]]

cp "$HERE/fixtures/fullscreen-scores.cfg" "$work/autoexec.cfg"
run "+wait;screenshot;wait"
python3 "$HERE/check-sbar.py" "$PAK" "$work/SCRN0.BMP" scores24

echo "ok: score hold and all four rune icons"
