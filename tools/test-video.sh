#!/bin/sh
# Direct presentation must never erase the status-bar strip before redrawing it.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
OUT=$(mktemp -d "${TMPDIR:-/tmp}/qcport-video.XXXXXX")
trap 'rm -rf "$OUT"' EXIT HUP INT TERM
cc -std=c89 -pedantic -Wall -Wextra -Werror -Wno-unused-parameter -Dfar= -Dpascal= -Dnear= \
  -I"$ROOT/src/render" -I"$ROOT/src/qgl" -I"$ROOT/src/host" \
  "$ROOT/tools/test-video.c" "$ROOT/src/render/vid.c" -o "$OUT/test-video"
"$OUT/test-video"
echo "ok: direct present preserves the status-bar strip"
