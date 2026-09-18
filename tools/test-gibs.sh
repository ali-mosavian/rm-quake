#!/bin/bash
# Regression: an overkilled monster vanished into its original head and three
# original gib models; the old renderer left the normal corpse in its place.
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
OUT="${1:?usage: test-gibs.sh <build-dir>}"

[[ -f "$OUT/e1m1.qmp" ]] || { echo "SKIP: no e1m1.qmp in $OUT"; exit 0; }

python3 - "$HERE" "$OUT/e1m1.qmp" <<'PY'
import struct
import sys

sys.path.insert(0, sys.argv[1])
import qmapread

data = qmapread.read(sys.argv[2], "effects.bin")
if data[:4] != b"QFX2":
    raise SystemExit("FAIL: effects.bin is not the shared QFX2 alias bank")
count = struct.unpack_from("<H", data, 4)[0]
names = [struct.unpack_from("<8s", data, 14 + i * 68)[0].rstrip(b"\0").decode() for i in range(count)]
wanted = [
    "spike", "s_spike", "grenade", "missile", "lavaball", "w_spike", "zom_gib",
    "gib1", "gib2", "gib3", "h_player", "h_guard", "h_knight", "h_dog",
    "h_ogre", "h_demon", "h_zombie", "h_wizard", "h_shams", "bolt", "bolt3",
]
if names != wanted:
    raise SystemExit(f"FAIL: effect bank contains {names}, wanted {wanted}")
for i, name in enumerate(names):
    base = 14 + i * 68
    head = struct.unpack_from("<4s5h6fh8B2h", data, base + 8)
    skin_w, skin_h = head[4], head[5]
    skin_ofs = struct.unpack_from("<I", data, base + 64)[0]
    if skin_ofs % 16384 + skin_w * skin_h > 16384:
        raise SystemExit(f"FAIL: {name} skin crosses an EMS page")
print("ok: shared effect bank carries the projectiles, gibs and heads")
PY

# Start with the rocket launcher. One direct rocket overkills the 25-health dog
# 100 units ahead, below dog_die's -35 gib threshold.
python3 - "$OUT/CARRY.BIN" <<'PY'
import struct
import sys

open(sys.argv[1], "wb").write(struct.pack(
    "<lhhfhhhhhhh", 1 | 128 | 4096, 100, 0, 0.0, 25, 0, 10, 128, 0, 0, 0
))
PY

rm -f "$OUT/s11.sav" "$OUT/s11.tmp" "$OUT/s11.old"
trap 'rm -f "$OUT/s11.sav" "$OUT/s11.tmp" "$OUT/s11.old"' EXIT
KEEP_CARRY=1 CYCLES=max TIMEOUT="${TIMEOUT:-200}" "${RUN_SH:-$HERE/run.sh}" "$OUT" \
    "e1m1.qmp -nostats -noai -carry -at 88 1420 -190 -yaw 270 -fire +wait;wait;wait;wait;wait;save s11;load s11;quit" \
    >/dev/null 2>&1 || true

reports=$(tr -d '\r' < "$OUT/cstep.txt" 2>/dev/null | grep '^gib mdl=' || true)
first=$(sed -n '1p' <<< "$reports")
second=$(sed -n '2p' <<< "$reports")
drawn=$(sed -n 's/^gib mdl=\([0-9][0-9]*\).*/\1/p' <<< "$first")
live_before=$(sed -n 's/.* live=\([0-9][0-9]*\).*/\1/p' <<< "$first")
live_after=$(sed -n 's/.* live=\([0-9][0-9]*\).*/\1/p' <<< "$second")
if [[ "${drawn:-0}" -lt 1 || "${live_before:-0}" -lt 1 || "$live_before" != "$live_after" ]]; then
    echo "FAIL: overkill/save/load gib reports were [$reports]" >&2
    exit 1
fi
if [[ "$(grep -c '^map_release ok ' "$OUT/cstep.txt")" -ne 2 ]]; then
    echo "FAIL: gib save/load did not release both map loads" >&2
    exit 1
fi

echo "ok: an overkilled dog drew $drawn gib/head alias triangles; save/load kept $live_after records"
