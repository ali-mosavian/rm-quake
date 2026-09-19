#!/bin/bash
# Regression: id's software crosshair must draw the CONCHARS '+' at the view centre and honour its offsets.
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
OUT="$(cd "${1:?usage: test-crosshair.sh <build-dir>}" && pwd)"
PAK="${PAK:-$HOME/dos/QUAKE_SW/ID1/PAK0.PAK}"

[[ -f "$OUT/dm3ish.qmp" ]] || { echo "SKIP: no dm3ish.qmp in $OUT"; exit 0; }
[[ -f "$PAK" ]] || { echo "SKIP: no PAK0.PAK at $PAK"; exit 0; }

work="$(mktemp -d "$OUT/crosshair-test.XXXXXX")"
if [[ -n "${KEEP_CROSSHAIR_TEST:-}" ]]; then
    echo "crosshair evidence: $work"
else
    trap 'rm -rf "$work"' EXIT
fi
cp "$OUT/QCPORT.EXE" "$OUT/dm3ish.qmp" "$OUT/stuff.ini" "$OUT/default.cfg" "$work/"

run() {
    TIMEOUT="${TIMEOUT:-300}" "$HERE/run.sh" "$work" \
        "dm3ish.qmp -nostats -nosound -noai -nodraw -at 232 232 -16 -yaw 270 -ticks 12 $1" >/dev/null
    mv "$work/SCRN0.BMP" "$work/$2.bmp"
}

run "+wait;screenshot;wait" off
printf 'crosshair -1\ncl_crossx -7.5\ncl_crossy -5.5\n' > "$work/autoexec.cfg"
run "+wait;screenshot;wait" on
printf 'crosshair 0.5\ncl_crossx -164.5\ncl_crossy 0\n' > "$work/autoexec.cfg"
run "+wait;screenshot;wait" clipped
printf 'crosshair -1\ncl_crossx 320\ncl_crossy 200\n' > "$work/autoexec.cfg"
run "+wait;screenshot;wait" outside

python3 - "$PAK" "$work/off.bmp" "$work/on.bmp" "$work/clipped.bmp" "$work/outside.bmp" <<'PY'
import struct
import sys


def pak_file(path: str, wanted: bytes) -> bytes:
    data = open(path, "rb").read()
    magic, directory, size = struct.unpack_from("<4sii", data)
    if magic != b"PACK":
        raise SystemExit("FAIL: PAK0.PAK has no PACK header")
    for offset in range(directory, directory + size, 64):
        name, start, length = struct.unpack_from("<56sii", data, offset)
        if name.split(b"\0", 1)[0].lower() == wanted:
            return data[start : start + length]
    raise SystemExit(f"FAIL: {wanted.decode()} absent from PAK0.PAK")


def wad_lump(data: bytes, wanted: bytes) -> bytes:
    magic, count, directory = struct.unpack_from("<4sii", data)
    if magic != b"WAD2":
        raise SystemExit("FAIL: gfx.wad has no WAD2 header")
    for index in range(count):
        offset = directory + index * 32
        start, _disk, size = struct.unpack_from("<iii", data, offset)
        name = data[offset + 16 : offset + 32].split(b"\0", 1)[0].upper()
        if name == wanted:
            return data[start : start + size]
    raise SystemExit(f"FAIL: {wanted.decode()} absent from gfx.wad")


def bmp(path: str) -> tuple[int, int, bytes]:
    data = open(path, "rb").read()
    pixels = struct.unpack_from("<I", data, 10)[0]
    width, height = struct.unpack_from("<ii", data, 18)
    stride = (width + 3) & ~3
    top_down = b"".join(
        data[pixels + (height - 1 - y) * stride : pixels + (height - y) * stride][:width]
        for y in range(height)
    )
    return width, height, top_down


chars = wad_lump(pak_file(sys.argv[1], b"gfx.wad"), b"CONCHARS")
code = ord("+")
glyph = [
    chars[(code // 16 * 8 + y) * 128 + code % 16 * 8 + x]
    for y in range(8)
    for x in range(8)
]
ow, oh, off = bmp(sys.argv[2])
nw, nh, on = bmp(sys.argv[3])
if (ow, oh) != (320, 200) or (nw, nh) != (ow, oh):
    raise SystemExit("FAIL: crosshair evidence is not 320x200")

# Default viewsize 100 leaves a 320x152 view above the 48-line status bar.
# Draw_Character receives the centre as its glyph's top-left. The float
# offsets are added before the final integer conversion: 152.5,70.5 -> 152,70.
x0, y0 = 152, 70
changed = 0
for y in range(8):
    for x in range(8):
        at = (y0 + y) * nw + x0 + x
        pixel = glyph[y * 8 + x]
        if pixel:
            if on[at] != pixel:
                raise SystemExit(f"FAIL: crosshair pixel {x},{y} is {on[at]}, wanted {pixel}")
            changed += on[at] != off[at]
        elif on[at] != off[at]:
            raise SystemExit(f"FAIL: transparent crosshair pixel {x},{y} changed")
if changed < 8:
    raise SystemExit("FAIL: crosshair glyph was not drawn")

cw, ch, clipped = bmp(sys.argv[4])
if (cw, ch) != (ow, oh):
    raise SystemExit("FAIL: clipped crosshair evidence has the wrong size")
# 160 - 164.5 converts at the final Draw_Character coordinate to -4.
for y in range(8):
    for x in range(4):
        at = (76 + y) * cw + x
        pixel = glyph[y * 8 + x + 4]
        if clipped[at] != (pixel or off[at]):
            raise SystemExit(f"FAIL: clipped crosshair pixel {x},{y} is wrong")
for y in range(30, ch):
    for x in range(cw):
        if 0 <= x < 4 and 76 <= y < 84:
            continue
        at = y * cw + x
        if clipped[at] != off[at]:
            raise SystemExit(f"FAIL: clipped crosshair changed {x},{y}")

xw, xh, outside = bmp(sys.argv[5])
if (xw, xh) != (ow, oh) or outside[30 * xw :] != off[30 * ow :]:
    raise SystemExit("FAIL: fully offscreen crosshair changed the frame")
print("ok: CONCHARS crosshair, fractional cvars and clipped offsets")
PY

[[ ! -s "$work/error.log" ]]
grep -q '^map_release ok ' "$work/cstep.txt"
grep -q '^restored$' "$work/cstep.txt"
