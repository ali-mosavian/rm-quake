#!/bin/bash
# Regression test: the swapped-in front end draws id's main menu and frees its EMS.
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
OUT="$(cd "${1:?usage: test-menu.sh <build-dir>}" && pwd)"
PAK="${PAK:-$HOME/dos/QUAKE_SW/ID1/PAK0.PAK}"

[[ -f "$PAK" ]] || { echo "SKIP: no PAK0.PAK at $PAK"; exit 0; }

work="$(mktemp -d "$OUT/menu-test.XXXXXX")"
trap 'rm -rf "$work"' EXIT
for arm in main help; do
  mkdir "$work/$arm"
  cp "$OUT/QCPORT.EXE" "$OUT/stuff.ini" "$OUT/default.cfg" "$OUT/dm3ish.qmp" "$work/$arm/"
done
cp "$HERE/fixtures/help-menu.cfg" "$work/help/mhelp.cfg"

TIMEOUT="${TIMEOUT:-60}" "$HERE/run.sh" "$work/main" \
  "dm3ish.qmp -nostats -nosound -noai -ticks 3 +wait;menu_main;wait;screenshot;wait;togglemenu;wait" >/dev/null
TIMEOUT="${TIMEOUT:-60}" "$HERE/run.sh" "$work/help" \
  "dm3ish.qmp -nostats -nosound -noai -ticks 30 +wait;exec mhelp.cfg" >/dev/null
for arm in main help; do
  grep -q '^scr_screenshot ok$' "$work/$arm/cstep.txt"
  grep -q '^map_release ok ' "$work/$arm/cstep.txt"
  grep -q '^restored$' "$work/$arm/cstep.txt"
done
python3 "$HERE/check-menu.py" "$PAK" "$work/main/SCRN0.BMP" "$work/help/SCRN0.BMP"
head -c 54 "$work/help/SCRN0.BMP" > "$work/help-short.bmp"
if python3 "$HERE/check-menu.py" "$PAK" "$work/main/SCRN0.BMP" \
     "$work/help-short.bmp" >/dev/null 2>&1; then
  echo "FAIL: a Help BMP with no pixel data passed" >&2
  exit 1
fi
