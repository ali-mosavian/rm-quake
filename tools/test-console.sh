#!/bin/sh
# Console command parsing must preserve quoting, split command streams,
# reject invalid cvar values and fail closed at every fixed buffer boundary.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
OUT=$(mktemp -d "${TMPDIR:-/tmp}/qcport-console.XXXXXX")
trap 'rm -rf "$OUT"' EXIT HUP INT TERM

cc -std=c89 -pedantic -Wall -Wextra -Werror \
  -I"$ROOT/src/host" -I"$ROOT/src/render" "$ROOT/tools/test-console.c" \
  "$ROOT/src/host/cmd.c" "$ROOT/src/render/fov.c" -lm -o "$OUT/test-console"
"$OUT/test-console"
echo "ok: console parser and cvar registry"
