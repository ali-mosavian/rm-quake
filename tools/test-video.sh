#!/bin/sh
# Direct presentation must preserve the status-bar strip, copy a 1:1 view without
# the pixel scaler, and keep both 320x200 frame surfaces in EMS.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
OUT=$(mktemp -d "${TMPDIR:-/tmp}/qcport-video.XXXXXX")
trap 'rm -rf "$OUT"' EXIT HUP INT TERM
cc -std=c89 -pedantic -Wall -Wextra -Werror -Wno-unused-parameter -Dfar= -Dpascal= -Dnear= \
  -I"$ROOT/src/render" -I"$ROOT/src/qgl" -I"$ROOT/src/host" \
  "$ROOT/tools/test-video.c" "$ROOT/src/render/vid.c" -lm -o "$OUT/test-video"
"$OUT/test-video"
echo "ok: direct 1:1 present bypasses scaling, preserves the status bar, and uses EMS"
