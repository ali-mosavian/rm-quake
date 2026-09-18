#!/bin/sh
# An active pool maps its EMS page once and batches framebuffer rows per frame.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
OUT=$(mktemp -d "${TMPDIR:-/tmp}/qcport-particles.XXXXXX")
trap 'rm -rf "$OUT"' EXIT HUP INT TERM
cc -std=c89 -pedantic -Wall -Wextra -Werror -Wno-unused-parameter -Dfar= -Dpascal= -Dnear= \
  -I"$ROOT/src/render" -I"$ROOT/src/qgl" \
  "$ROOT/tools/test-particles.c" "$ROOT/src/render/part.c" -lm -o "$OUT/test-particles"
"$OUT/test-particles"
echo "ok: impacts, trails and teleports use one EMS pool mapping and batched framebuffer rows per frame"
