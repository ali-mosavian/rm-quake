#!/bin/bash
# Regression test: shot-only buttons move, and health triggers can fire brushes.
set -uo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
OUT="${1:?usage: test-shoot-brushes.sh <build-dir>}"
[[ -f "$OUT/e1m1.qmp" && -f "$OUT/e1m2.qmp" ]] || { echo "SKIP: no e1m1/e1m2 qmp in $OUT"; exit 0; }

python3 - "$HERE/.." "$OUT/e1m2.qmp" <<'PY' || exit 1
import struct
import sys

sys.path.insert(0, sys.argv[1] + "/tools")
import qmapread

data = qmapread.read(sys.argv[2], "ents0.bin")
head = struct.unpack_from("<4f3fff13hfhhh3ffh", data)
ntele, nplat, nhide, nitem = head[10:14]
ndoor, ntrig, nmon, ngate = head[14], head[15], head[16], head[30]
offset = 90 + nmon * 24 + ntele * 22 + nplat * 14 + nhide * 2 + ngate * 6 + nitem * 22 + ndoor * 54
kinds = [struct.unpack_from("<hh", data, offset + i * 54) for i in range(ntrig)]
if (56, 14) not in kinds:
    raise SystemExit("FAIL: e1m2 func_button *56 was not packed as shoot-only")
PY

run() {
    TIMEOUT="${TIMEOUT:-180}" "${RUN_SH:-$HERE/run.sh}" "$OUT" \
        "e1m1.qmp -nostats -noai -nosound -at 400 2016 -105 -yaw 0 -ticks 60 $1" >/dev/null 2>&1
}
brush() { tr -d '\r' < "$OUT/cstep.txt" | sed -n 's/^sum 60 .* br \([0-9-]*\) .*/\1/p' | tail -1; }

run ""
off=$(brush)
run "-fire"
on=$(brush)
[[ -n "$off" && -n "$on" && "$off" != "$on" ]] || {
    echo "FAIL: shooting e1m1's health trigger moved no target brush ($off/$on)" >&2
    exit 1
}
grep -q '^combat_brush_selftest 1' "$OUT/cstep.txt" || {
    echo "FAIL: trigger CanDamage did not select NONE/PUSH targets correctly" >&2
    exit 1
}
grep -q '^ent_trig_selftest 1' "$OUT/cstep.txt" || {
    echo "FAIL: a health func_button did not enter its movement state" >&2
    exit 1
}
echo "ok: health trigger fired its brush; shoot-only button and CanDamage branches pass"
