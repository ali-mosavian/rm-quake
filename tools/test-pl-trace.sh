#!/bin/sh
# Exploding boxes are exact asymmetric solid BBOXes for movement and shots.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
OUT=$(mktemp -d "${TMPDIR:-/tmp}/qcport-pl-trace.XXXXXX")
trap 'rm -rf "$OUT"' EXIT
cc -std=c89 -pedantic -Wall -Wextra -Werror -fpack-struct=2 \
  -DQCPORT_NATIVE_TEST -Dfar= -Dnear= -Dpascal= \
  -I"$ROOT/src/game" -I"$ROOT/src/host" -I"$ROOT/src/model" \
  -I"$ROOT/src/render" -I"$ROOT/src/sound" -I"$ROOT/src/qgl" \
  "$ROOT/tools/test-pl-trace.c" "$ROOT/src/game/pl_trace.c" \
  -o "$OUT/test-pl-trace"
"$OUT/test-pl-trace"
echo "ok: exploding-box traces use exact bounds and nearest-hit ordering"
