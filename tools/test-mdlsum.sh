#!/bin/bash
# Regression test: a model's frames reach the draw from the pages mkmdl.py
# laid them in.
#
# A frame read from the wrong page or offset still draws a plausible
# model -- somebody's vertices -- so no picture says which page was read.
# -mdlsum walks every frame of every loaded model through mdl_frame_verts,
# the draw's own fetch, and prints a sum weighted by each byte's place in
# the stream; this asks mkmdl.py's vertex_bytes, from the PAK, what each
# should be. Every monster a map's container ships must print, Chthon on
# e1m7 -- the one model over several pages -- and v_shot, the one view
# model a short run loads: a model that failed to load prints nothing, and
# nothing must not pass. Every configured view model also has to select a
# named frame set: v_axe's frames are `frame*`, not `shot*`.
#
# Then each set, fetched as the draw picks a state's frames through
# mdl_set_frame. The whole-model sum walks frames 0..n and never asks
# which set a frame is: summing the sets from set 1 moved every death,
# pain and attack back a frame and no sum or reference frame changed.
# The expected counts are the PAK's, and the header's are checked against
# them. Which set a state draws -- d_alias.c's switch -- is not tested.
#
# Then the refusals, on one patched copy of e1m1's container: soldier.geo
# with QMD2's magic, dog.geo claiming a page its frames never reach, and
# v_shot.geo with set counts one short of its frames. Each must load no
# model and say so by name on E9 -- run.sh keeps no stderr -- while the run
# still reaches its exit. The page-too-many header loaded before its guard,
# with a zero-length read nobody noticed; two pages too many make the read
# length negative, and one guard refuses both.
#
# Every run is -skill 2. A container ships the kinds of every skill and a
# run loads only the kinds its skill spawns: on easy e1m2 has no wizard and
# e1m3 no shambler, and their missing sums read as models that failed to
# load. Hard spawns every shipped kind on all three maps.
#
#   tools/test-mdlsum.sh <build-dir>      e1m1..e1m3 hold every monster kind, e1m7 Chthon
set -uo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/.." && pwd)"
OUT="${1:?usage: test-mdlsum.sh <build-dir>}"
MAPS="${MDLSUM_MAPS:-e1m1 e1m2 e1m3 e1m7}"
RUN="${RUN_SH:-$HERE/run.sh}"
PAK="${PAK:-$HOME/dos/QUAKE_SW/ID1/PAK0.PAK}"
[[ -f "$PAK" ]] || { echo "SKIP: no PAK at $PAK"; exit 0; }
fail=0
for map in $MAPS; do
    if [[ ! -f "$OUT/$map.qmp" ]]; then echo "SKIP: no $map.qmp in $OUT"; continue; fi
    TIMEOUT="${TIMEOUT:-300}" "$RUN" "$OUT" "$map.qmp -skill 2 -nostats -nosound -noai -mdlsum -ticks 2" >/dev/null 2>&1
    tr -d '\r' < "$OUT/cstep.txt" 2>/dev/null | grep -E '^mdl_(sum|set) ' > "$OUT/mdlsum-$map.txt"
    # a crash prints no sums either: say so, rather than blame every model
    tr -d '\r' < "$OUT/cstep.txt" 2>/dev/null | grep -q '^frames=' ||
        { echo "FAIL: $map: no frames= summary, so the run never reached its exit"; fail=1; }
    python3 - "$ROOT" "$PAK" "$OUT/$map.qmp" "$OUT/mdlsum-$map.txt" "$map" <<'PY' || fail=1
import struct
import sys

root, pak, qmp, got_path, map_name = sys.argv[1:6]
sys.path.insert(0, f"{root}/tools")
import mdlview as mdl
import numpy as np
import mkassets
import mkmdl
import qmapread

blob, entries = mdl.read_pak(pak)

for name in mkassets.VIEW_MDLS:
    model = mdl.parse_mdl(mdl.pak_read(blob, entries, f"progs/{name}.mdl"), name)
    spec = mkassets.VIEW_MODEL_SET.get(name, "shot")
    named = sum(len(mkmdl.kept(model, set_name, take)) for set_name, take in mkmdl.parse_sets(spec))
    if named != len(mkmdl.select_frames(model, mkmdl.parse_sets(spec))):
        raise SystemExit(f"FAIL: {name}: {spec} selected the fallback frame list, not its named frames")

specs = ({name: sets for name, sets in mkassets.MDL_SETS.values()} |
         {v: mkassets.VIEW_MODEL_SET.get(v, "shot") for v in mkassets.VIEW_MDLS}
         | dict([mkassets.BOSS_MDL]))
monsters = {name for name, _ in mkassets.MDL_SETS.values()} | {mkassets.BOSS_MDL[0]}
shipped = {m[:-4] for m in qmapread.members(qmp) if m.endswith(".geo")}
want = (shipped & monsters) | {"v_shot"}
got = {}
got_set = {}
for line in open(got_path):
    kind, name, a, b, s = line.split()
    if kind == "mdl_sum":
        got[name] = (int(a), int(b), int(s, 16))
    else:
        got_set[(name, int(a))] = (int(b), int(s, 16))


def weighted(v: np.ndarray) -> int:
    return int((v * (np.arange(len(v), dtype=np.uint64) % 251 + 1)).sum() % (1 << 32))


bad = 0
for name in sorted(want - set(got)):
    print(f"FAIL: {map_name}: {name} printed no sum -- it did not load, or -mdlsum skipped it")
    bad += 1
for name in sorted(want & set(got)):
    vtx = np.frombuffer(mkmdl.vertex_bytes(pak, name, specs[name]), np.uint8).astype(np.uint64)
    geo = qmapread.read(qmp, f"{name}.geo")
    nb = struct.unpack_from("<h", geo, 6)[0] * 3
    frames = len(vtx) // nb
    pages = -(-frames // (16384 // nb))
    if got[name] != (frames, pages, weighted(vtx)):
        g = got[name]
        print(f"FAIL: {map_name}: {name} fetched {g[0]} frames, {g[1]} pages, {g[2]:08X};"
              f" the PAK says {frames}, {pages}, {weighted(vtx):08X}")
        bad += 1
    # each set as the draw picks it: a frame from the wrong set is still some frame of the model.
    # The counts come from the PAK, not the header under test.
    model = mdl.parse_mdl(mdl.pak_read(blob, entries, f"progs/{name}.mdl"), name)
    sets = mkmdl.parse_sets(specs[name])
    counts = [len(mkmdl.kept(model, s, n)) for s, n in sets]
    if list(geo[40:48]) != (counts + [0] * 8)[:8]:
        print(f"FAIL: {map_name}: {name}.geo's set counts {list(geo[40:48])}; the PAK's are {counts}")
        bad += 1
    printed = {s for n, s in got_set if n == name}
    if printed != {s for s, n in enumerate(counts) if n}:
        print(f"FAIL: {map_name}: {name} printed sets {sorted(printed)}; the PAK's sets with frames are"
              f" {[s for s, n in enumerate(counts) if n]}")
        bad += 1
    first = 0
    for s, ((set_name, _), n) in enumerate(zip(sets, counts)):
        if not n:
            continue
        want_set = (n, weighted(vtx[first * nb:(first + n) * nb]))
        first += n
        if (g := got_set.get((name, s))) and g != want_set:
            print(f"FAIL: {map_name}: {name} set {s} ({set_name}) fetched {g[0]} frames, {g[1]:08X};"
                  f" the PAK says {n}, {want_set[1]:08X}")
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
d[so:so + 4] = b"QMD2"
do = m["dog.geo"][0]
struct.pack_into("<H", d, do + 48, struct.unpack_from("<H", d, do + 48)[0] + 1)
vo = m["v_shot.geo"][0]
d[vo + 40] -= 1
open(dst, "wb").write(d)
PY
    TIMEOUT="${TIMEOUT:-300}" "$RUN" "$OUT" "e1m1bad.qmp -nostats -nosound -noai -mdlsum -ticks 2" >/dev/null 2>&1
    log=$(tr -d '\r' < "$OUT/cstep.txt" 2>/dev/null)
    rm -f "$OUT/e1m1bad.qmp"
    why=""
    grep -q "^soldier.geo: not a QMD3 .geo" <<< "$log" || why="$why; soldier.geo's old magic said nothing on E9"
    grep -qE "^dog.geo: .* -- will not fit" <<< "$log" || why="$why; dog.geo's page too many said nothing on E9"
    grep -qE "^v_shot.geo: .* needed, or not its" <<< "$log" || why="$why; v_shot.geo's short set count said nothing on E9"
    grep -q "^mdl_sum soldier " <<< "$log" && why="$why; soldier loaded from a QMD2 header"
    grep -q "^mdl_sum dog " <<< "$log" && why="$why; dog loaded with a page its frames never reach"
    grep -q "^mdl_sum v_shot " <<< "$log" && why="$why; v_shot loaded with counts short of its frames"
    grep -q "^frames=" <<< "$log" || why="$why; no frames= summary, so the run never reached its exit"
    if [[ -z "$why" ]]; then
        echo "ok: refusals: an old header, a page too many and short set counts are each refused by name"
    else
        echo "FAIL: refusals${why}"
        fail=1
    fi
fi
exit $fail
