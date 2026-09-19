#!/bin/bash
# Regression test: e1m4 must keep both sound and lightmapped animated surfaces.
#
# Its 6,120 faces fragmented conventional memory until the 13,248-byte DSP
# block failed, leaving the second attract-demo map silent. Separately,
# d_faces rejected every animated texture from the surface cache, so the six
# animated lightmapped faces were always drawn fullbright. The button below
# was palette index 248 both with and without -lm.
#
#   tools/test-e1m4-av.sh build/cport-e1m4
set -uo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/.." && pwd)"
OUT="${1:?usage: test-e1m4-av.sh <build-dir>}"
RUN="${RUN_SH:-$HERE/run.sh}"

[[ -f "$OUT/e1m4.qmp" ]] || { echo "SKIP: no e1m4.qmp in $OUT"; exit 0; }

# A cold e1m4 has one large free tail and hides the original failure. Cross
# the real title boundary from e1m3 using short clips of both id demos: every
# allocation is map-authentic, but the test does not spend 74 seconds on the
# first camera track.
work="$(mktemp -d "$OUT/e1m4-transition.XXXXXX")"
trap 'rm -rf "$work"' EXIT
cp "$OUT/QCPORT.EXE" "$OUT/stuff.ini" "$OUT/default.cfg" \
   "$ROOT/data/maps/start/start.qmp" "$ROOT/data/maps/e1m3/e1m3.qmp" \
   "$ROOT/data/maps/e1m4/e1m4.qmp" "$work/"
for demo in demo1 demo2; do
    if [[ -f "$OUT/$demo.qdc" ]]; then
        cp "$OUT/$demo.qdc" "$work/$demo-full.qdc"
    else
        python3 "$ROOT/tools/mkidcam.py" \
            "${PAK:-$HOME/dos/QUAKE_SW/ID1/PAK0.PAK}" "$demo.dem" "$work/$demo-full.qdc"
    fi
done
python3 - "$work/demo1-full.qdc" "$work/demo2-full.qdc" \
    "$work/first.qdc" "$work/second.qdc" <<'PY'
import struct
import sys
from pathlib import Path


FRAME = struct.Struct("<6fl12h")
ENTITY = struct.Struct("<5h4f")


def has_sound(record: bytearray) -> bool:
    count = FRAME.unpack_from(record)[-1]
    return any(
        (entity := ENTITY.unpack_from(record, FRAME.size + index * ENTITY.size))[1] == 5
        and entity[0] == 0
        and entity[4] > 0
        for index in range(count)
    )


def clip(path: str, sound: bool = False) -> bytes:
    data = Path(path).read_bytes()
    records = []
    offset = 13
    while offset < len(data):
        frame = FRAME.unpack_from(data, offset)
        size = FRAME.size + frame[-1] * ENTITY.size
        records.append(bytearray(data[offset : offset + size]))
        offset += size
    selected = [bytearray(records[index]) for index in (0, 11, 28)]
    if sound:
        sounded = [bytearray(record) for record in records[1:] if has_sound(record)]
        if len(sounded) < 2:
            raise SystemExit(f"FAIL: {path} has fewer than two sound snapshots")
        selected[1:] = sounded[:2]
    for record, time in zip(selected, (0.0, 0.02, 0.04)):
        struct.pack_into("<f", record, 0, time)
    return data[:13] + b"".join(selected)


Path(sys.argv[3]).write_bytes(clip(sys.argv[1]))
Path(sys.argv[4]).write_bytes(clip(sys.argv[2], sound=True))
PY

TIMEOUT="${TIMEOUT:-120}" "$RUN" "$work" \
    "start.qmp -lm -nostats +startdemos first.qdc second.qdc;wait;wait;wait;wait;wait;toggleconsole;quit" \
    >/dev/null 2>&1 || {
        echo "FAIL: title transition runner failed" >&2
        exit 1
    }
log="$work/cstep.txt"
cp "$log" "$OUT/e1m4-transition.txt"
cp "$work/BENCH.BMP" "$OUT/e1m4-transition.bmp" 2>/dev/null || {
    echo "FAIL: transitioned e1m4 wrote no screenshot" >&2
    exit 1
}
cp "$work/BENCH.TXT" "$OUT/e1m4-transition-bench.txt" 2>/dev/null || {
    echo "FAIL: transitioned e1m4 wrote no benchmark record" >&2
    exit 1
}
rm -f "$OUT/e1m4-transition-ERROR.LOG" "$OUT/e1m4-transition-RUN.OUT" \
      "$OUT/e1m4-transition-ERRMEM.TXT" "$OUT/e1m4-transition-dosbox.log"
for diagnostic in ERROR.LOG RUN.OUT ERRMEM.TXT dosbox.log; do
    [[ -f "$work/$diagnostic" ]] && cp "$work/$diagnostic" "$OUT/e1m4-transition-$diagnostic"
done
grep -q '^snd_report_selftest 1$' "$log" || {
    echo "FAIL: maximum-width sound report crossed its 160-byte buffer" >&2
    exit 1
}
grep -q '^sc_selftest 1$' "$log" && grep -q '^sc_selftest_leak 0$' "$log" || {
    echo "FAIL: fragmented surface-cache allocation self-test failed or leaked" >&2
    exit 1
}
e1m4_sc=$(awk '/^asset_map ok e1m4.qmp$/{map=1; next} map && /^sc_init /{print; exit}' "$log")
[[ "$e1m4_sc" == sc_init\ ok* ]] || {
    echo "FAIL: e1m3 -> e1m4 transition reported ${e1m4_sc:-no surface cache result}" >&2
    exit 1
}
transition_builds=$(tr -d '\r' < "$OUT/e1m4-transition-bench.txt" | awk '$1 == "sc_builds"{print $2}')
transition_live=$(tr -d '\r' < "$OUT/e1m4-transition-bench.txt" | awk '$1 == "sc_live"{print $2}')
if [[ ! "$transition_builds" =~ ^[0-9]+$ || ! "$transition_live" =~ ^[0-9]+$ ]]; then
    echo "FAIL: transitioned e1m4 cache counters are malformed" >&2
    exit 1
fi
if (( transition_builds < 1 || transition_live < 1 )); then
    echo "FAIL: transitioned e1m4 cache built=${transition_builds:-none} live=${transition_live:-none}" >&2
    exit 1
fi
e1m3_frames=$(awk '/^frames=/{frames=$0} /^gs_state .* map e1m3.qmp /{print frames; exit}' "$log")
[[ "$e1m3_frames" =~ ^frames=[1-9][0-9]*\ .*\ mdl=[1-9][0-9]*\ ticks=0\  ]] || {
    echo "FAIL: e1m3 title clip did not render a model frame" >&2
    exit 1
}
e1m4_sound=$(awk '/^snd started=/{sound=$0} /^gs_state .* map e1m4.qmp /{print sound; exit}' "$log")
e1m4_seen=$(awk '/^snd seen=/{seen=$0} /^gs_state .* map e1m4.qmp /{print seen; exit}' "$log")
transition_started=$(sed -n 's/^snd started=\([0-9][0-9]*\).*/\1/p' <<< "$e1m4_sound")
transition_loops=$(sed -n 's/.* loops=\([0-9][0-9]*\).*/\1/p' <<< "$e1m4_sound")
transition_stack=$(sed -n 's/.* stack=\([0-9][0-9]*\).*/\1/p' <<< "$e1m4_seen")
transition_paints=$(sed -n 's/.* paints=\([0-9][0-9]*\).*/\1/p' <<< "$e1m4_sound")
transition_seen=$(sed -n 's/^snd seen=\([0-9A-F][0-9A-F]*\).*/\1/p' <<< "$e1m4_seen")
seen_byte=${transition_seen:2:2}
if (( ${transition_started:-0} < 1 || ${transition_loops:-0} != 72 ||
      ${transition_paints:-0} < 1 || ${transition_stack:-0} < 1 || ${transition_stack:-0} >= 512 ||
      (16#${seen_byte:-00} & 2) == 0 )); then
    echo "FAIL: transitioned e1m4 sound started=${transition_started:-none} loops=${transition_loops:-none} paints=${transition_paints:-none} stack=${transition_stack:-none}" >&2
    exit 1
fi
[[ $(grep -c '^map_release ok ' "$log") -ge 3 ]] || {
    echo "FAIL: title transition did not release all three loaded maps" >&2
    exit 1
}
grep -q '^restored$' "$log" || { echo "FAIL: title transition did not shut down cleanly" >&2; exit 1; }

run() {
    CYCLES="${CYCLES:-max}" TIMEOUT="${TIMEOUT:-60}" "$RUN" "$OUT" "e1m4.qmp $1" >/dev/null 2>&1 || {
        echo "FAIL: e1m4 runner failed: $1" >&2
        exit 1
    }
    grep -q '^restored$' "$OUT/cstep.txt" || {
        echo "FAIL: e1m4 run did not shut down cleanly: $1" >&2
        exit 1
    }
}

fail=0
run "-lm -nostats -noai -fire -ticks 8"
log="$OUT/cstep.txt"
loops=$(tr -d '\r' < "$log" 2>/dev/null | sed -n 's/^snd loops=\([0-9-]*\).*/\1/p' | head -1)
started=$(tr -d '\r' < "$log" 2>/dev/null | sed -n 's/^snd started=\([0-9-]*\).*/\1/p' | head -1)
stack=$(tr -d '\r' < "$log" 2>/dev/null | sed -n 's/^snd seen=.* stack=\([0-9-]*\).*/\1/p' | head -1)
cold_paints=$(tr -d '\r' < "$log" 2>/dev/null | sed -n 's/^snd started=.* paints=\([0-9-]*\).*/\1/p' | head -1)
if ! grep -q '^sc_init ok ' "$log" 2>/dev/null; then
    echo "FAIL: e1m4 lost its lightmap cache" >&2
    fail=1
fi
if (( ${loops:-0} < 1 || ${started:-0} < 1 || ${cold_paints:-0} < 1 || ${stack:-0} < 1 )); then
    echo "FAIL: e1m4 sound loops=${loops:-none} started=${started:-none} paints=${cold_paints:-none} stack=${stack:-none}" >&2
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

[[ $fail -eq 0 ]] && echo "ok: transition e1m4 started=$transition_started loops=$transition_loops paints=$transition_paints stack=$transition_stack; cold paints=$cold_paints; button unlit=$unlit, animated lit=$lit->$later"
exit $fail
