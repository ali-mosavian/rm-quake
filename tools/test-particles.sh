#!/bin/sh
# The full id particle pool stays in EMS chunks and batches framebuffer rows.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
OUT=$(mktemp -d "${TMPDIR:-/tmp}/qcport-particles.XXXXXX")
trap 'rm -rf "$OUT"' EXIT HUP INT TERM
cc -std=c89 -pedantic -Wall -Wextra -Werror -Wno-unused-parameter -Dfar= -Dpascal= -Dnear= \
  -I"$ROOT/src/render" -I"$ROOT/src/qgl" \
  "$ROOT/tools/test-particles.c" "$ROOT/src/render/part.c" -lm -o "$OUT/test-particles"
"$OUT/test-particles"
echo "ok: all 2048 particle slots stay in EMS; id-sized effects render in row batches"
