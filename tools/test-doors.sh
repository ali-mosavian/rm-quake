#!/bin/sh
# A secret door's first use clears its message and fires its own target once.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
OUT=$(mktemp -d "${TMPDIR:-/tmp}/qcport-doors.XXXXXX")
trap 'rm -rf "$OUT"' EXIT
cc -std=c89 -pedantic -Wall -Wextra -Werror -fpack-struct=2 \
  -DQCPORT_NATIVE_TEST -Dfar= -Dnear= -Dpascal= \
  -I"$ROOT/src/game" -I"$ROOT/src/host" -I"$ROOT/src/model" \
  -I"$ROOT/src/render" -I"$ROOT/src/sound" -I"$ROOT/src/qgl" \
  "$ROOT/tools/test-doors.c" "$ROOT/src/game/doors.c" "$ROOT/src/game/mover.c" \
  "$ROOT/src/game/pl_move.c" \
  "$ROOT/src/host/mathlib.c" -lm -o "$OUT/test-doors"
"$OUT/test-doors"
echo "ok: doors push, damage, reverse, and share Quake's block/touch clocks"
