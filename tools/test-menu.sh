#!/bin/bash
# Regression test: the swapped-in front end draws id's main menu and frees its EMS.
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
OUT="$(cd "${1:?usage: test-menu.sh <build-dir>}" && pwd)"
PAK="${PAK:-$HOME/dos/QUAKE_SW/ID1/PAK0.PAK}"

[[ -f "$PAK" ]] || { echo "SKIP: no PAK0.PAK at $PAK"; exit 0; }

work="$(mktemp -d "$OUT/menu-test.XXXXXX")"
trap 'rm -rf "$work"' EXIT
cp "$OUT/QCPORT.EXE" "$OUT/stuff.ini" "$OUT/default.cfg" "$OUT/dm3ish.qmp" "$work/"

TIMEOUT="${TIMEOUT:-60}" "$HERE/run.sh" "$work" \
  "dm3ish.qmp -nostats -nosound -noai -ticks 3 +wait;menu_main;wait;screenshot;wait;togglemenu;wait" >/dev/null
grep -q '^scr_screenshot ok$' "$work/cstep.txt"
grep -q '^map_release ok ' "$work/cstep.txt"
grep -q '^restored$' "$work/cstep.txt"
python3 "$HERE/check-menu.py" "$PAK" "$work/SCRN0.BMP"
