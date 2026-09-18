#!/bin/bash
# Regression: alias-model pickups must reach the renderer, not the old flat-box fallback.
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
OUT="${1:?usage: test-item-models.sh <build-dir>}"

[[ -f "$OUT/e1m1.qmp" ]] || { echo "SKIP: no e1m1.qmp in $OUT"; exit 0; }

python3 - "$HERE" "$OUT/e1m1.qmp" <<'PY'
import struct
import sys

sys.path.insert(0, sys.argv[1])
import qmapread

path = sys.argv[2]
members = qmapread.members(path)
expected = {"armor.itm", "backpack.itm", "g_nail.itm", "g_shot.itm", "quaddama.itm", "suit.itm"}
missing = expected - members.keys()
if missing:
    raise SystemExit(f"FAIL: e1m1 lacks pickup model members {sorted(missing)}")

armor = qmapread.read(path, "armor.itm")
_magic, ntri, nvert, nframe, sw, sh = struct.unpack_from("<4s5H", armor)
skin_at = 60 + ntri * 18 + nvert * nframe * 3
size = sw * sh
skins = [armor[skin_at + i * size:skin_at + (i + 1) * size] for i in range(3)]
if any(len(s) != size for s in skins) or len(set(skins)) != 3:
    raise SystemExit("FAIL: armor.itm does not carry three distinct complete skins")
print("ok: e1m1 ships its six pickup models and three armor skins")
PY

TIMEOUT="${TIMEOUT:-200}" "${RUN_SH:-$HERE/run.sh}" "$OUT" \
    "e1m1.qmp -nostats -noai -nosound -at 688 544 104 -yaw 90 -ticks 4" >/dev/null 2>&1 || true

line=$(tr -d '\r' < "$OUT/cstep.txt" 2>/dev/null | grep -m1 '^pickup mdl=' || true)
drawn=$(sed -n 's/^pickup mdl=\([0-9][0-9]*\).*/\1/p' <<< "$line")
if [[ "${drawn:-0}" -lt 1 ]]; then
    echo "FAIL: the visible green armor drew no alias-model triangles [$line]" >&2
    exit 1
fi

echo "ok: the visible pickup drew $drawn alias-model triangles"
