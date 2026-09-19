#!/bin/bash
# Regression test: a save restores the moved player and releases both map loads.
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
OUT="$(cd "${1:?usage: test-savegame.sh <build-dir>}" && pwd)"

work="$(mktemp -d "$OUT/savegame-test.XXXXXX")"
trap 'rm -rf "$work"' EXIT
cp "$OUT/QCPORT.EXE" "$OUT/stuff.ini" "$OUT/default.cfg" \
  "$HERE/../data/maps/e1m6/e1m6.qmp" "$work/"

TIMEOUT="${TIMEOUT:-60}" "$HERE/run.sh" "$work" \
  "e1m6.qmp -nostats -nosound -noai -ticks 100 -walk +wait;wait;wait;wait;save s0;wait;load s0;toggleconsole;quit" >/dev/null
saved="$(sed -n 's/^savegame wrote .* pos //p' "$work/cstep.txt")"
loaded="$(sed -n 's/^savegame loaded .* pos //p' "$work/cstep.txt")"
if [[ -z "$saved" || "$saved" != "$loaded" ]]; then
  echo "save/load position mismatch: saved '$saved', loaded '$loaded'" >&2
  exit 1
fi
if [[ "$(grep -c '^map_release ok ' "$work/cstep.txt")" -ne 2 ]]; then
  echo "save/load did not release both maps" >&2
  exit 1
fi
grep -Eq '^stack_untouched [1-9][0-9]* of ' "$work/cstep.txt" || {
  echo "save/load exhausted the stack probe" >&2
  exit 1
}
if [[ ! -s "$work/s0.sav" ]]; then
  echo "save file was not written" >&2
  exit 1
fi

# Version 5 stored `leapt` as one signed short at bytes 90..91. A landed
# monster therefore has ff in the byte version 6 now uses for anim_set.
# Wizard hover14 and shambler magic12 also exceed the corrected QuakeC
# sequences; both valid old ordinals must migrate rather than reject.
python3 - "$work/s0.sav" <<'PY'
import struct
import sys

path = sys.argv[1]
data = bytearray(open(path, "rb").read())
struct.pack_into("<h", data, 4, 5)
# SaveHead is byte-packed by the 16-bit compiler: mon_count starts at 71.
monsters = struct.unpack_from("<h", data, 71)[0]
gibs = data.rfind(b"GIB1")
if monsters < 1 or gibs < monsters * 104:
    raise SystemExit("save has no monster block before GIB1")
first = gibs - monsters * 104
found = set()
for i in range(monsters):
    ent = first + i * 104
    kind = struct.unpack_from("<h", data, ent + 20)[0]
    if kind == 6 and kind not in found:
        struct.pack_into("<h", data, ent + 24, 0)
        struct.pack_into("<h", data, ent + 26, 14)
        struct.pack_into("<h", data, ent + 90, -1)
        found.add(kind)
    elif kind == 7 and kind not in found:
        struct.pack_into("<h", data, ent + 24, 4)
        struct.pack_into("<h", data, ent + 26, 11)
        struct.pack_into("<h", data, ent + 90, -1)
        found.add(kind)
if found != {6, 7}:
    raise SystemExit(f"e1m6 save lacks wizard/shambler records: {found}")
open(path, "wb").write(data)
PY
TIMEOUT="${TIMEOUT:-60}" "$HERE/run.sh" "$work" \
  "e1m6.qmp -nostats -nosound -noai -ticks 1 +load s0;toggleconsole;quit" >/dev/null
grep -q '^savegame loaded ' "$work/cstep.txt" || {
  echo "version-5 monster animation state was not migrated" >&2
  exit 1
}

echo "ok: save/load restored $loaded and migrated version-5 wizard/shambler animation states"
