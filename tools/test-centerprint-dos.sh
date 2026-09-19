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
  python3 "$ROOT/tools/check-centerprint.py" "$@"
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
  "e1m1.qmp -nostats -nosound -noai -at 1312 904 -392 -record SEED.QCR +wait;toggleconsole;wait;toggleconsole;wait;toggleconsole;quit" >/dev/null
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
  "e1m1.qmp -nostats -nosound -noai -at 1312 904 -392 -play HIDE.QCR +wait;menu_quit;screenshot;wait;toggleconsole;quit" >/dev/null
check_pixels "$E1M1" "$OUT/SCRN0.BMP" 320 200 70 8 menu_absent "$secret"

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
