#!/bin/bash
# Regression test: waking Chthon must draw boss.mdl. He was only an unseen
# trigger, so the fight ran and completed in an empty arena.
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/.." && pwd)"
OUT="${1:?usage: test-boss.sh <build-dir>}"

cp "$ROOT/data/maps/e1m7/e1m7.qmp" "$OUT/"
TIMEOUT="${TIMEOUT:-600}" "$HERE/run.sh" "$OUT" \
  "e1m7.qmp -noai -at 8 64 8 -warp 30 352 376 208 -warp 60 352 -248 208 -warp 90 600 64 0 -yaw 180 -ticks 120 -nostats -bench 4000" \
  >/dev/null

drawn=$(sed -n 's/^boss drawn=\([0-9][0-9]*\).*/\1/p' "$OUT/cstep.txt" | head -1)
[[ -n "$drawn" && "$drawn" -gt 0 ]] || {
  echo "FAIL: awakened Chthon drew ${drawn:-no} boss.mdl triangles" >&2
  exit 1
}
live=$(sed -n 's/^particles live=\([0-9][0-9]*\).*/\1/p' "$OUT/cstep.txt" | head -1)
[[ "${live:-0}" -gt 0 ]] || {
  echo "FAIL: boss_awake emitted no lava splash" >&2
  exit 1
}
echo "ok: awakened Chthon drew $drawn boss.mdl triangles with $live lava particles"
