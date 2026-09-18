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
if data[:4] != b"QFX2":
    raise SystemExit("FAIL: effects.bin has no QFX2 shared-bank header")
count = struct.unpack_from("<H", data, 4)[0]
names = [struct.unpack_from("<8s", data, 14 + i * 68)[0].rstrip(b"\0").decode()
         for i in range(count)]

wanted = {"spike", "s_spike", "grenade", "missile", "lavaball", "w_spike", "zom_gib"}
if not wanted.issubset(names):
    raise SystemExit(f"FAIL: effect bank contains {names}, wanted projectiles {sorted(wanted)}")
print("ok: shared effect bank carries all seven original projectile models")
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
