#!/bin/bash
# Regression: in-flight projectiles must use Quake's alias models, not flat boxes.
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
OUT="${1:?usage: test-projectile-models.sh <build-dir>}"

[[ -f "$OUT/e1m1.qmp" ]] || { echo "SKIP: no e1m1.qmp in $OUT"; exit 0; }

python3 - "$HERE" "$OUT/e1m1.qmp" <<'PY'
import struct
import sys

sys.path.insert(0, sys.argv[1])
import qmapread

members = qmapread.members(sys.argv[2])
if "effects.bin" not in members:
    raise SystemExit("FAIL: map has no effects.bin projectile bundle")
data = qmapread.read(sys.argv[2], "effects.bin")
if data[:4] != b"QFX1":
    raise SystemExit("FAIL: effects.bin has no QFX1 header")
count = struct.unpack_from("<H", data, 4)[0]
if count != 7:
    raise SystemExit(f"FAIL: effects.bin carries {count} projectile models, wanted 7")

names = []
pos = 6
for _ in range(count):
    name, size = struct.unpack_from("<8sL", data, pos)
    pos += 12
    names.append(name.rstrip(b"\0").decode())
    if data[pos:pos + 4] != b"QMD3":
        raise SystemExit(f"FAIL: {names[-1]} is not a bundled alias model")
    pos += size

wanted = {"spike", "s_spike", "grenade", "missile", "lavaball", "w_spike", "zom_gib"}
if set(names) != wanted or pos != len(data):
    raise SystemExit(f"FAIL: projectile bundle contains {names}, wanted {sorted(wanted)}")
print("ok: projectile bundle carries all seven original models")
PY

TIMEOUT="${TIMEOUT:-200}" "${RUN_SH:-$HERE/run.sh}" "$OUT" \
    "e1m1.qmp -nostats -noai -at 112 2352 16 -yaw 270 -fire -ticks 14" >/dev/null 2>&1 || true

line=$(tr -d '\r' < "$OUT/cstep.txt" 2>/dev/null | grep -m1 '^projectile mdl=' || true)
drawn=$(sed -n 's/^projectile mdl=\([0-9][0-9]*\).*/\1/p' <<< "$line")
if [[ "${drawn:-0}" -lt 1 ]]; then
    echo "FAIL: the nail in flight drew no alias-model triangles [$line]" >&2
    exit 1
fi

echo "ok: the nail in flight drew $drawn alias-model triangles"
