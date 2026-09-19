#!/bin/sh
# The quit modal owns grave/OEM102; they must not close it and open the console.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
OUT=$(mktemp -d "${TMPDIR:-/tmp}/qcport-input-route.XXXXXX")
trap 'rm -rf "$OUT"' EXIT HUP INT TERM
cc -std=c89 -pedantic -Wall -Wextra -Werror -fpack-struct=2 \
  -DQCPORT_NATIVE_TEST -Dfar= -Dpascal= -Dnear= -D_fmemset=memset \
  -I"$ROOT/src/host" -I"$ROOT/src/qgl" -I"$ROOT/src/render" \
  -I"$ROOT/src/model" -I"$ROOT/src/game" -I"$ROOT/src/ui" -I"$ROOT/src/sound" \
  "$ROOT/tools/test-input-route.c" "$ROOT/src/host/input.c" \
  -o "$OUT/test-input-route"
"$OUT/test-input-route"
echo "ok: menu input owns grave and OEM102 before console routing"
