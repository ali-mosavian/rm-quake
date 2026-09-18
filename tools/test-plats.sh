#!/bin/sh
# A plat trigger column is not support: airborne corpses stay independent,
# grounded corpses follow, and a final AABB overlap still needs a hull hit.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
OUT=$(mktemp -d "${TMPDIR:-/tmp}/qcport-plats.XXXXXX")
trap 'rm -rf "$OUT"' EXIT
cc -std=c89 -pedantic -Wall -Wextra -Werror -fpack-struct=2 \
  -DQCPORT_NATIVE_TEST -Dfar= -Dnear= -Dpascal= \
  -I"$ROOT/src/game" -I"$ROOT/src/host" -I"$ROOT/src/model" \
  -I"$ROOT/src/render" -I"$ROOT/src/sound" -I"$ROOT/src/qgl" \
  "$ROOT/tools/test-plats.c" "$ROOT/src/game/plats.c" \
  "$ROOT/src/host/mathlib.c" -o "$OUT/test-plats"
"$OUT/test-plats"
echo "ok: mover transport requires support or a final hull collision"
