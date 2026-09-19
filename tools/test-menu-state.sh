#!/bin/sh
# Help navigation and the quit modal must preserve id's menu state and EMS ownership.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
OUT=$(mktemp -d "${TMPDIR:-/tmp}/qcport-menu-state.XXXXXX")
trap 'rm -rf "$OUT"' EXIT HUP INT TERM
cc -std=c89 -pedantic -Wall -Wextra -Werror -fpack-struct=2 -DQCPORT_NATIVE_TEST \
  -Dfar= -Dpascal= -Dnear= -D_fmemset=memset \
  -I"$ROOT/src/ui" -I"$ROOT/src/host" -I"$ROOT/src/qgl" \
  -I"$ROOT/src/render" -I"$ROOT/src/model" -I"$ROOT/src/game" -I"$ROOT/src/sound" \
  "$ROOT/tools/test-menu-state.c" "$ROOT/src/ui/menu.c" -o "$OUT/test-menu-state"
"$OUT/test-menu-state"
echo "ok: help navigation and quit confirmation preserve menu state and EMS ownership"
