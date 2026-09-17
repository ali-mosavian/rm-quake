#!/bin/sh
# id's walking camera bobs vertically and rolls proportionally to lateral speed.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
OUT=$(mktemp -d "${TMPDIR:-/tmp}/qcport-viewfx.XXXXXX")
trap 'rm -rf "$OUT"' EXIT
cc -std=c89 -pedantic -Wall -Wextra -Werror -Dfar= -Dpascal= \
  -I"$ROOT/src/render" -I"$ROOT/src/qgl" \
  "$ROOT/tools/test-viewfx.c" "$ROOT/src/render/viewfx.c" -lm -o "$OUT/test-viewfx"
"$OUT/test-viewfx"
echo "ok: id walk bob and lateral view roll"
