#!/bin/bash
# Regression: button frame 1 selects +a in demos, gameplay, and restored saves.
#
# What went wrong: converted brush frames were discarded and live buttons never
# selected their alternate chain. Pressed switches therefore kept +0shoot.
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/.." && pwd)"
OUT="$(cd "${1:?usage: test-alt-textures.sh <build-dir>}" && pwd)"
RUN="${RUN_SH:-$HERE/run.sh}"
WORK="$(mktemp -d "$OUT/alt-textures.XXXXXX")"
trap 'rm -rf "$WORK"' EXIT

[[ -f "$OUT/demo3.qdc" ]] || python3 "$ROOT/tools/mkidcam.py" \
    "${PAK:-$HOME/dos/QUAKE_SW/ID1/PAK0.PAK}" demo3.dem "$OUT/demo3.qdc"

for arm in normal pressed cached; do
    mkdir "$WORK/$arm"
    cp "$OUT/QCPORT.EXE" "$OUT/stuff.ini" "$OUT/default.cfg" \
       "$ROOT/data/maps/e1m6/e1m6.qmp" "$WORK/$arm/"
done

# demo3's first pressed *38 frame puts the switch directly in front of the
# camera. Hold that real snapshot for five seconds and vary only its frame.
python3 - "$OUT/demo3.qdc" "$WORK/normal/normal.qdc" "$WORK/pressed/pressed.qdc" \
    "$WORK/cached/cached.qdc" <<'PY'
import struct
import sys
from pathlib import Path


FRAME = struct.Struct("<6fl12h")
ENTITY = struct.Struct("<5h4f")


def pressed_record(data: bytes) -> bytearray:
    offset = 13
    while offset < len(data):
        frame = FRAME.unpack_from(data, offset)
        size = FRAME.size + frame[-1] * ENTITY.size
        record = bytearray(data[offset : offset + size])
        if any(
            entity[1] == 1 and entity[2] == 38 and entity[4] == 1
            for entity in (
                ENTITY.unpack_from(record, FRAME.size + i * ENTITY.size)
                for i in range(frame[-1])
            )
        ):
            return record
        offset += size
    raise SystemExit("FAIL: demo3 has no pressed *38 brush frame")


def set_frame(record: bytearray, value: int) -> None:
    count = FRAME.unpack_from(record, 0)[-1]
    hits = 0
    for i in range(count):
        offset = FRAME.size + i * ENTITY.size
        entity = list(ENTITY.unpack_from(record, offset))
        if entity[1] == 1 and entity[2] == 38:
            entity[4] = value
            ENTITY.pack_into(record, offset, *entity)
            hits += 1
    if hits != 1:
        raise SystemExit(f"FAIL: demo3 frame has {hits} copies of brush *38")


source = Path(sys.argv[1]).read_bytes()
for path, value in ((Path(sys.argv[2]), 0), (Path(sys.argv[3]), 1)):
    first = pressed_record(source)
    set_frame(first, value)
    second = bytearray(first)
    struct.pack_into("<f", first, 0, 0.0)
    struct.pack_into("<f", second, 0, 5.0)
    path.write_bytes(source[:13] + first + second)

normal = pressed_record(source)
pressed = pressed_record(source)
held = pressed_record(source)
set_frame(normal, 0)
set_frame(pressed, 1)
set_frame(held, 1)
struct.pack_into("<f", normal, 0, 0.0)
struct.pack_into("<f", pressed, 0, 1.0)
struct.pack_into("<f", held, 0, 5.0)
Path(sys.argv[4]).write_bytes(source[:13] + normal + pressed + held)
PY

for arm in normal pressed; do
    TIMEOUT="${TIMEOUT:-120}" "$RUN" "$WORK/$arm" \
        "e1m6.qmp -nostats -nosound -noai -play $arm.qdc +screenshot;wait;toggleconsole;quit" \
        >/dev/null
    [[ -s "$WORK/$arm/SCRN0.BMP" ]] || { echo "FAIL: $arm demo wrote no screenshot" >&2; exit 1; }
    grep -q '^ent_trig_selftest 1$' "$WORK/$arm/cstep.txt" || {
        echo "FAIL: button frame did not clear on return" >&2
        exit 1
    }
done

{
    echo screenshot
    for _ in $(seq 1 60); do echo wait; done
    echo screenshot
    echo wait
    echo toggleconsole
    echo quit
} > "$WORK/cached/cache.cfg"
TIMEOUT="${TIMEOUT:-120}" "$RUN" "$WORK/cached" \
    "e1m6.qmp -lm -nostyles -nodlight -nostats -nosound -noai -play cached.qdc +exec cache.cfg" \
    >/dev/null
[[ -s "$WORK/cached/SCRN0.BMP" && -s "$WORK/cached/SCRN1.BMP" ]] || {
    echo "FAIL: cached demo wrote no before/after screenshots" >&2
    exit 1
}

python3 - "$WORK/normal/SCRN0.BMP" "$WORK/pressed/SCRN0.BMP" \
    "$WORK/cached/SCRN0.BMP" "$WORK/cached/SCRN1.BMP" <<'PY'
import struct
import sys
from pathlib import Path


def bmp(path: str) -> tuple[list[tuple[int, int, int]], list[int]]:
    data = Path(path).read_bytes()
    offset = struct.unpack_from("<I", data, 10)[0]
    dib_size, width, height = struct.unpack_from("<Iii", data, 14)
    bits = struct.unpack_from("<H", data, 28)[0]
    if (width, abs(height), bits) != (320, 200, 8):
        raise SystemExit(f"FAIL: {path} is not 320x200x8")
    palette_at = 14 + dib_size
    palette = [
        tuple(reversed(data[palette_at + i * 4 : palette_at + i * 4 + 3]))
        for i in range(256)
    ]
    stride = (width + 3) & ~3
    rows = [
        data[offset + row * stride : offset + row * stride + width]
        for row in range(abs(height))
    ]
    if height > 0:
        rows.reverse()
    return palette, [pixel for row in rows for pixel in row]


def compare(first: str, second: str, label: str, minimum_light: int, failure: str) -> None:
    first_palette, first_pixels = bmp(first)
    second_palette, second_pixels = bmp(second)
    world = range(16 * 320, 152 * 320)
    different = sum(first_pixels[i] != second_pixels[i] for i in world)
    first_light = sum(sum(first_palette[first_pixels[i]]) for i in world)
    second_light = sum(sum(second_palette[second_pixels[i]]) for i in world)
    light_delta = first_light - second_light
    print(f"{label} changes {different} world pixels, luminance {light_delta}")
    if different < 15000 or light_delta < minimum_light:
        raise SystemExit(f"FAIL: {failure}")


compare(*sys.argv[1:3], "demo3 pressed switch", 1000000, "pressed demo brush kept its bright normal texture")
compare(*sys.argv[3:5], "cached lightmapped switch", 800000, "cached normal surface was reused for pressed texture")
PY

cp "$WORK/normal/SCRN0.BMP" "$OUT/alt-texture-normal.bmp"
cp "$WORK/pressed/SCRN0.BMP" "$OUT/alt-texture-pressed.bmp"
cp "$WORK/cached/SCRN0.BMP" "$OUT/alt-texture-cache-normal.bmp"
cp "$WORK/cached/SCRN1.BMP" "$OUT/alt-texture-cache-pressed.bmp"

# Touch e1m1's *4 and save while its one-second hold is active. Remove the
# derived draw bit, load, and save again: load must reconstruct it from HELD.
mkdir "$WORK/live" "$WORK/loaded"
for arm in live loaded; do
    cp "$OUT/QCPORT.EXE" "$OUT/stuff.ini" "$OUT/default.cfg" \
       "$ROOT/data/maps/e1m1/e1m1.qmp" "$WORK/$arm/"
done
{
    for _ in $(seq 1 5); do echo wait; done
    echo screenshot
    echo 'save s0'
    echo wait
    echo toggleconsole
    echo quit
} > "$WORK/live/press.cfg"
TIMEOUT="${TIMEOUT:-120}" "$RUN" "$WORK/live" \
    "e1m1.qmp -lm -nostats -noai -nosound -at -80 576 40 -yaw 0 -walk +exec press.cfg" \
    >/dev/null
grep -q '^savegame wrote ' "$WORK/live/cstep.txt" || { echo "FAIL: pressed button was not saved" >&2; exit 1; }

python3 - "$WORK/live/s0.sav" <<'PY'
import struct
import sys
from pathlib import Path


BRUSH_BYTES = 18
MODEL = 4
data = bytearray(Path(sys.argv[1]).read_bytes())
_skill, models, teles, plats, doors, trigs, lights, items, monsters = struct.unpack_from("<9h", data, 55)
gibs = data.find(b"GIB1")
if gibs < 0:
    raise SystemExit("FAIL: save has no GIB1 trailer")
brush_end = gibs - (
    monsters * 104 + items * 28 + lights * 10 + trigs * 78
    + doors * 102 + plats * 52 + teles * 48
)
brush_start = brush_end - models * BRUSH_BYTES
offset = brush_start + MODEL * BRUSH_BYTES
draw = struct.unpack_from("<h", data, offset)[0]
if draw != 3:
    raise SystemExit(f"FAIL: live pressed brush draw is {draw}, want 3")
struct.pack_into("<h", data, offset, 1)
Path(sys.argv[1]).write_bytes(data)
PY

cp "$WORK/live/s0.sav" "$WORK/loaded/"
TIMEOUT="${TIMEOUT:-120}" "$RUN" "$WORK/loaded" \
    "e1m1.qmp -lm -nostats -noai -nosound +load s0;wait;screenshot;save s1;wait;toggleconsole;quit" \
    >/dev/null
grep -q '^savegame loaded ' "$WORK/loaded/cstep.txt" || { echo "FAIL: patched save did not load" >&2; exit 1; }

python3 - "$WORK/loaded/s1.sav" <<'PY'
import struct
import sys
from pathlib import Path


data = Path(sys.argv[1]).read_bytes()
_skill, models, teles, plats, doors, trigs, lights, items, monsters = struct.unpack_from("<9h", data, 55)
gibs = data.find(b"GIB1")
brush_end = gibs - (
    monsters * 104 + items * 28 + lights * 10 + trigs * 78
    + doors * 102 + plats * 52 + teles * 48
)
draw = struct.unpack_from("<h", data, brush_end - models * 18 + 4 * 18)[0]
if draw != 3:
    raise SystemExit(f"FAIL: loaded HELD button reconstructed draw {draw}, want 3")
PY

cp "$WORK/live/SCRN0.BMP" "$OUT/alt-texture-live.bmp"
cp "$WORK/loaded/SCRN0.BMP" "$OUT/alt-texture-loaded.bmp"
echo "ok: alternate texture is visible in demo3 and restored for live/save buttons"
