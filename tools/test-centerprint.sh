#!/bin/sh
# Asset conversion must preserve centerprint newlines, blank rows and Quake's bound.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
if [ -f "$ROOT/data/start.bsp" ]; then
  PYTHONPATH="$ROOT/tools" python3 "$ROOT/tools/test-centerprint.py" "$ROOT/data/start.bsp"
else
  PYTHONPATH="$ROOT/tools" python3 "$ROOT/tools/test-centerprint.py"
fi
OUT=$(mktemp -d "${TMPDIR:-/tmp}/qcport-center.XXXXXX")
trap 'rm -rf "$OUT"' EXIT HUP INT TERM
cc -std=c89 -pedantic -Wall -Wextra -Werror -Dfar= -Dpascal= \
  -I"$ROOT/src/ui" -I"$ROOT/src/game" \
  "$ROOT/tools/test-centerprint.c" "$ROOT/src/ui/center.c" \
  "$ROOT/src/game/enttext.c" -o "$OUT/test-centerprint"
"$OUT/test-centerprint"
echo "ok: centerprint assets, layout and expiry"
