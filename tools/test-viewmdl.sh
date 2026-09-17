#!/bin/sh
# Weapon switches must evict the old view model before EMS fills.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
OUT=$(mktemp -d "${TMPDIR:-/tmp}/qcport-viewmdl.XXXXXX")
trap 'rm -rf "$OUT"' EXIT HUP INT TERM
cc -std=c89 -pedantic -Wall -Wextra -Werror -Wno-keyword-macro -Dfar= -Dpascal= -Dnear= \
  -I"$ROOT/src/model" -I"$ROOT/src/game" -I"$ROOT/src/host" -I"$ROOT/src/qgl" -I"$ROOT/src/sound" \
  -I"$ROOT/src/render" "$ROOT/tools/test-viewmdl.c" -o "$OUT/test-viewmdl"
"$OUT/test-viewmdl"
echo "ok: one view weapon model remains resident"
