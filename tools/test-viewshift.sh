#!/bin/sh
# Damage, pickup, contents, and powerup shifts reproduce id's palette blend and fades.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
OUT=$(mktemp -d "${TMPDIR:-/tmp}/qcport-viewshift.XXXXXX")
trap 'rm -rf "$OUT"' EXIT
cc -std=c89 -pedantic -Wall -Wextra -Werror -fpack-struct=2 -DQCPORT_NATIVE_TEST -Dfar= -Dpascal= \
  -I"$ROOT/src/game" -I"$ROOT/src/model" -I"$ROOT/src/render" -I"$ROOT/src/qgl" -I"$ROOT/src/host" \
  "$ROOT/tools/test-viewshift.c" "$ROOT/src/render/vshift.c" -o "$OUT/test-viewshift"
"$OUT/test-viewshift"
echo "ok: id damage, pickup, contents, and powerup palette shifts"
