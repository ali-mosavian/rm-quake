#!/bin/bash
# Real map text must keep id's display-space layout. An active message is
# hidden by the console and expires on real frame time while simulation is
# paused there. Centerprint is captured from the final 320x200 display.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="$(cd "${1:?usage: test-centerprint-dos.sh <build-dir>}" && pwd)"
START="$OUT/start.qmp"
E1M1="$OUT/e1m1.qmp"

[[ -f "$START" ]] || { echo "SKIP: no start.qmp in $OUT"; exit 0; }

check_pixels() {
  python3 - "$1" "$2" "$3" "$4" "$5" "$6" "$7" "$8" <<'PY'
import struct
import sys


def member(qmp: bytes, wanted: bytes) -> bytes:
    count, directory = struct.unpack_from("<Hl", qmp, 8)
    for index in range(count):
        name, offset, size = struct.unpack_from("<16sll", qmp, directory + index * 24)
        if name.split(b"\0")[0] == wanted:
            return qmp[offset : offset + size]
    raise AssertionError(f"missing {wanted.decode()}")


qmp = open(sys.argv[1], "rb").read()
font = member(qmp, b"font.fnt")[4:]
palette = member(qmp, b"pal.raw")
bmp = open(sys.argv[2], "rb").read()
want_width, want_height, first_y, step = map(int, sys.argv[3:7])
mode = sys.argv[7]
pixel_offset = struct.unpack_from("<I", bmp, 10)[0]
width, height = struct.unpack_from("<ii", bmp, 18)
assert (width, height) == (want_width, want_height)
stride = (width + 3) & ~3


def bestfit(red: int, green: int, blue: int) -> int:
    return min(
        range(256),
        key=lambda index: sum(
            (palette[index * 3 + channel] - value) ** 2
            for channel, value in enumerate((red, green, blue))
        ),
    )


def pixel(x: int, y: int) -> int:
    return bmp[pixel_offset + (height - 1 - y) * stride + x]


message = sys.argv[8]
expected = set()
for row, line in enumerate(message.split("\n")):
    x0 = (width - len(line) * 4) // 2
    y0 = first_y + row * step
    for column, character in enumerate(line):
        glyph = font[ord(character) * 8 : ord(character) * 8 + 8]
        words = struct.unpack("<4H", glyph)
        for gy in range(8):
            for gx in range(8):
                bit = gy * 8 + gx
                if words[bit >> 4] & (1 << (15 - (bit & 15))):
                    expected.add((x0 + column * 4 + gx, y0 + gy))

colour = bestfit(240, 232, 216)
matching = sum(pixel(x, y) == colour for x, y in expected)
if mode == "present" and matching != len(expected):
    raise SystemExit(f"FAIL: {len(expected) - matching} of {len(expected)} centerprint pixels are missing")
if mode == "absent" and matching > len(expected) // 5:
    raise SystemExit(f"FAIL: {matching} of {len(expected)} hidden centerprint pixels remain")
print(f"ok: centerprint {mode}, {matching}/{len(expected)} matching pixels")
PY
}

episode='This is the second episode:
The Realm of Black Magic

Ancient castles and
strange beasts ahead...'

TIMEOUT="${TIMEOUT:-300}" "$ROOT/tools/run.sh" "$OUT" \
  "start.qmp -nostats -nosound -noai -at 416 2304 104 -ticks 2 +wait;screenshot;wait" >/dev/null
check_pixels "$START" "$OUT/SCRN0.BMP" 320 200 48 8 present "$episode"

[[ -f "$E1M1" ]] || { echo "SKIP console arms: no e1m1.qmp in $OUT"; exit 0; }

# Make the stimulus through the recorder, then change only its three frame
# times. The long middle frame is deliberate: console pause stops simulation,
# not the centerprint's real-time expiry.
rm -f "$OUT/SEED.QCR" "$OUT/ACTIVE.QCR" "$OUT/HIDE.QCR" "$OUT/EXPIRE.QCR"
TIMEOUT="${TIMEOUT:-300}" "$ROOT/tools/run.sh" "$OUT" \
  "e1m1.qmp -nostats -nosound -noai -at 1312 904 -392 -record SEED.QCR +wait;toggleconsole;wait;toggleconsole;wait;quit" >/dev/null
python3 - "$OUT/SEED.QCR" "$OUT" <<'PY'
import struct
import sys

source = open(sys.argv[1], "rb").read()
record = struct.Struct("<9h5f")
assert source[:4] == b"QCR2" and (len(source) - 4) // record.size >= 3


def write(name: str, times: list[float]) -> None:
    data = bytearray(source[: 4 + len(times) * record.size])
    for index, frame_time in enumerate(times):
        struct.pack_into("<f", data, 4 + index * record.size + 18, frame_time)
    open(f"{sys.argv[2]}/{name}", "wb").write(data)


write("ACTIVE.QCR", [0.02, 0.02])
write("HIDE.QCR", [0.02, 0.02])
write("EXPIRE.QCR", [0.02, 2.1, 0.2])
PY

secret='You found a secret area!'
rm -f "$OUT/SCRN0.BMP"
TIMEOUT="${TIMEOUT:-300}" "$ROOT/tools/run.sh" "$OUT" \
  "e1m1.qmp -nostats -nosound -noai -at 1312 904 -392 -play ACTIVE.QCR +wait;screenshot;wait" >/dev/null
check_pixels "$E1M1" "$OUT/SCRN0.BMP" 320 200 70 8 present "$secret"

rm -f "$OUT/SCRN0.BMP"
TIMEOUT="${TIMEOUT:-300}" "$ROOT/tools/run.sh" "$OUT" \
  "e1m1.qmp -nostats -nosound -noai -at 1312 904 -392 -play HIDE.QCR +wait;toggleconsole;screenshot;wait" >/dev/null
check_pixels "$E1M1" "$OUT/SCRN0.BMP" 320 200 70 8 absent "$secret"

rm -f "$OUT/SCRN0.BMP"
TIMEOUT="${TIMEOUT:-300}" "$ROOT/tools/run.sh" "$OUT" \
  "e1m1.qmp -nostats -nosound -noai -at 1312 904 -392 -play EXPIRE.QCR +wait;toggleconsole;wait;toggleconsole;screenshot;wait" >/dev/null
check_pixels "$E1M1" "$OUT/SCRN0.BMP" 320 200 70 8 absent "$secret"
center=$(tr -d '\r' < "$OUT/cstep.txt" | grep '^center ' | tail -1)
[[ "$center" == "center msg=0 ms=0" ]] || {
  echo "FAIL: paused centerprint did not expire: ${center:-no state}" >&2
  exit 1
}

# A five-line message reaches above a short, centered viewport. Its pixels
# must leave the static border when the message expires.
letterbox=$(mktemp -d "$OUT/center-letterbox.XXXXXX")
trap 'rm -rf "$letterbox"' EXIT
cp "$OUT/QCPORT.EXE" "$OUT/default.cfg" "$START" "$OUT/EXPIRE.QCR" "$letterbox/"
sed 's/render.yres = 100/render.yres = 40/' "$OUT/stuff.ini" > "$letterbox/stuff.ini"
TIMEOUT="${TIMEOUT:-300}" "$ROOT/tools/run.sh" "$letterbox" \
  "start.qmp -nostats -nosound -noai -at 416 2304 104 -play EXPIRE.QCR +wait;toggleconsole;wait;toggleconsole;screenshot;wait" >/dev/null
check_pixels "$letterbox/start.qmp" "$letterbox/SCRN0.BMP" 320 200 48 8 absent "$episode"
rm -rf "$letterbox"
trap - EXIT

echo "ok: console hiding and paused real-time expiry"
