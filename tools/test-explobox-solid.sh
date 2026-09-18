#!/bin/bash
# A live misc_explobox is SOLID_BBOX: walking into e1m1's barrel stops at its expanded face.
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
OUT="${1:?usage: test-explobox-solid.sh <build-dir>}"

[[ -f "$OUT/e1m1.qmp" ]] || { echo "SKIP: no e1m1.qmp in $OUT"; exit 0; }
TIMEOUT="${TIMEOUT:-200}" "${RUN_SH:-$HERE/run.sh}" "$OUT" \
  "e1m1.qmp -nostats -noai -nosound -at 88 1980 -184 -yaw 270 -walk -ticks 120" \
  >/dev/null 2>&1

line=$(tr -d '\r' < "$OUT/cstep.txt" | grep -m1 '^frames=')
y=$(sed -n 's/.* pos=[^,]*,[^,]*,\([-0-9]*\).*/\1/p' <<< "$line")
if [[ "${y:-0}" -lt 2039 || "${y:-0}" -gt 2040 ]]; then
    echo "FAIL: player crossed e1m1's live barrel [$line], wanted y 2039..2040" >&2
    exit 1
fi

echo "ok: e1m1's live exploding box stopped the player at y=$y"
