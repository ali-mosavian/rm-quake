#!/bin/sh
# Expired console text must retain no EMS; notifications keep only the font,
# while an open console owns the backdrop and font and can release/reopen both.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
OUT=$(mktemp -d "${TMPDIR:-/tmp}/qcport-console-gfx.XXXXXX")
trap 'rm -rf "$OUT"' EXIT HUP INT TERM

cc -std=c89 -pedantic -Wall -Wextra -Werror -fpack-struct=2 -DQCPORT_NATIVE_TEST \
  -Dfar= -Dpascal= -Dnear= -D_fmemset=memset \
  -I"$ROOT/src/ui" -I"$ROOT/src/host" -I"$ROOT/src/qgl" \
  -I"$ROOT/src/render" -I"$ROOT/src/model" -I"$ROOT/src/game" -I"$ROOT/src/sound" \
  "$ROOT/tools/test-console-gfx.c" "$ROOT/src/ui/console.c" -o "$OUT/test-console-gfx"
"$OUT/test-console-gfx"
echo "ok: console EMS follows visible art lifetime"
