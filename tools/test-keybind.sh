#!/bin/bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT=$(mktemp -d "${TMPDIR:-/tmp}/qcport-keybind.XXXXXX")
trap 'rm -rf "$OUT"' EXIT
cc -std=c89 -Wall -Wextra -Werror -Dnear= -Dfar= -Dpascal= \
  -I"$ROOT/src/host" -I"$ROOT/src/qgl" "$ROOT/tools/test-keybind.c" \
  "$ROOT/src/host/cmd.c" "$ROOT/src/host/keybind.c" -o "$OUT/test-keybind"
(cd "$OUT" && ./test-keybind)
echo "ok: binding ownership, commands, mouse order and config paths"
