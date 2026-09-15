#!/bin/bash
# Regression test: a model's frames reach the draw from the pages mkmdl.py
# laid them in.
#
# A frame read from the wrong page or offset still draws a plausible
# model -- somebody's vertices -- so no picture says which page was read.
# -mdlsum walks every frame of every loaded model through mdl_frame_verts,
# the draw's own fetch, and prints a sum weighted by each byte's place in
# the stream; this asks mkmdl.py's vertex_bytes, from the PAK, what each
# should be. Every monster a map's container ships must print, and v_shot,
# the one view model a short run loads: a model that failed to load prints
# nothing, and nothing must not pass.
#
# Then the refusals, on one patched copy of e1m1's container: soldier.geo
# with the 48-byte header's magic, and dog.geo claiming a page its frames
# never reach. Each must load no model and say so by name on E9 -- run.sh
# keeps no stderr -- while v_shot still loads. The page-too-many header
# loaded before its guard, with a zero-length read nobody noticed; two
# pages too many make the read length negative, and one guard refuses both.
#
# Every run is -skill 2. A container ships the kinds of every skill and a
# run loads only the kinds its skill spawns: on easy e1m2 has no wizard and
# e1m3 no shambler, and their missing sums read as models that failed to
# load. Hard spawns every shipped kind on all three maps.
#
#   tools/test-mdlsum.sh <build-dir>      e1m1, e1m2 and e1m3 hold every monster kind
set -uo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/.." && pwd)"
OUT="${1:?usage: test-mdlsum.sh <build-dir>}"
RUN="${RUN_SH:-$HERE/run.sh}"
PAK="${PAK:-$HOME/dos/QUAKE_SW/ID1/PAK0.PAK}"
[[ -f "$PAK" ]] || { echo "SKIP: no PAK at $PAK"; exit 0; }
fail=0
for map in e1m1 e1m2 e1m3; do
    if [[ ! -f "$OUT/$map.qmp" ]]; then echo "SKIP: no $map.qmp in $OUT"; continue; fi
    TIMEOUT="${TIMEOUT:-300}" "$RUN" "$OUT" "$map.qmp -skill 2 -nostats -nosound -noai -mdlsum -ticks 2" >/dev/null 2>&1
    tr -d '\r' < "$OUT/cstep.txt" 2>/dev/null | grep '^mdl_sum ' > "$OUT/mdlsum-$map.txt"
    python3 - "$ROOT" "$PAK" "$OUT/$map.qmp" "$OUT/mdlsum-$map.txt" "$map" <<'PY' || fail=1
import struct
import sys

root, pak, qmp, got_path, map_name = sys.argv[1:6]
sys.path.insert(0, f"{root}/tools")
import numpy as np
import mkassets
import mkmdl
import qmapread

specs = {name: sets for name, sets in mkassets.MDL_SETS.values()} | {v: "shot" for v in mkassets.VIEW_MDLS}
monsters = {name for name, _ in mkassets.MDL_SETS.values()}
shipped = {m[:-4] for m in qmapread.members(qmp) if m.endswith(".geo")}
want = (shipped & monsters) | {"v_shot"}
got = {}
for line in open(got_path):
    _, name, frames, pages, s = line.split()
    got[name] = (int(frames), int(pages), int(s, 16))

bad = 0
for name in sorted(want - set(got)):
    print(f"FAIL: {map_name}: {name} printed no sum -- it did not load, or -mdlsum skipped it")
    bad += 1
for name in sorted(want & set(got)):
    vtx = np.frombuffer(mkmdl.vertex_bytes(pak, name, specs[name]), np.uint8).astype(np.uint64)
    nvert = struct.unpack_from("<h", qmapread.read(qmp, f"{name}.geo"), 6)[0]
    frames = len(vtx) // (nvert * 3)
    pages = -(-frames // (16384 // (nvert * 3)))
    weight = np.arange(len(vtx), dtype=np.uint64) % 251 + 1
    want_sum = int((vtx * weight).sum() % (1 << 32))
    if got[name] != (frames, pages, want_sum):
        g = got[name]
        print(f"FAIL: {map_name}: {name} fetched {g[0]} frames, {g[1]} pages, {g[2]:08X};"
              f" the PAK says {frames}, {pages}, {want_sum:08X}")
        bad += 1
if not bad:
    print(f"ok: {map_name}: " + ", ".join(f"{n} {got[n][0]}f/{got[n][1]}p" for n in sorted(want))
          + " fetched as mkmdl laid them")
sys.exit(1 if bad else 0)
PY
done

if [[ -f "$OUT/e1m1.qmp" ]]; then
    python3 - "$ROOT" "$OUT/e1m1.qmp" "$OUT/e1m1bad.qmp" <<'PY'
import struct
import sys

root, src, dst = sys.argv[1:4]
sys.path.insert(0, f"{root}/tools")
import qmapread

d = bytearray(open(src, "rb").read())
m = qmapread.members(src)
so = m["soldier.geo"][0]
d[so:so + 4] = b"QMDL"
do = m["dog.geo"][0]
struct.pack_into("<H", d, do + 48, struct.unpack_from("<H", d, do + 48)[0] + 1)
open(dst, "wb").write(d)
PY
    TIMEOUT="${TIMEOUT:-300}" "$RUN" "$OUT" "e1m1bad.qmp -nostats -nosound -noai -mdlsum -ticks 2" >/dev/null 2>&1
    log=$(tr -d '\r' < "$OUT/cstep.txt" 2>/dev/null)
    rm -f "$OUT/e1m1bad.qmp"
    why=""
    grep -q "^soldier.geo: not a paged .geo" <<< "$log" || why="$why; soldier.geo's old magic said nothing on E9"
    grep -qE "^dog.geo: .* -- will not fit" <<< "$log" || why="$why; dog.geo's page too many said nothing on E9"
    grep -q "^mdl_sum soldier " <<< "$log" && why="$why; soldier loaded from a 48-byte header"
    grep -q "^mdl_sum dog " <<< "$log" && why="$why; dog loaded with a page its frames never reach"
    grep -q "^mdl_sum v_shot " <<< "$log" || why="$why; v_shot printed no sum, so the run never reached its exit"
    if [[ -z "$why" ]]; then
        echo "ok: refusals: an old header and a page too many are each refused by name, and v_shot still loads"
    else
        echo "FAIL: refusals${why}"
        fail=1
    fi
fi
exit $fail
