#!/bin/bash
# Config must run before the first map, explicit diagnostics must still win,
# and the clean-shutdown rewrite must preserve unbound keys.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
B="$(cd "${1:?usage: test-keybind-dos.sh <build-dir>}" && pwd)"
OUT=$(mktemp -d "$B/keybind-test.XXXXXX")
trap 'rm -rf "$OUT"' EXIT

cp "$B/QCPORT.EXE" "$B/stuff.ini" "$B/default.cfg" \
   "$ROOT/data/maps/e1m1/e1m1.qmp" "$OUT/"
cp "$ROOT/tools/ref/custom-config.cfg" "$OUT/config.cfg"
KEEP_CONFIG=1 TIMEOUT="${TIMEOUT:-180}" "$ROOT/tools/run.sh" "$OUT" \
  "e1m1.qmp -nosound -noai -skill 2 -ticks 1" >/dev/null

grep -q '^mod_load_world ok .* mon=42$' "$OUT/cstep.txt" || {
    echo "FAIL: explicit -skill did not win before first-map entity selection"; exit 1;
}
grep -q '^cvars skill=2 fov=100$' "$OUT/cstep.txt" || {
    echo "FAIL: archived cvars or explicit startup precedence is wrong"; exit 1;
}
tr -d '\r' < "$OUT/config.cfg" > "$OUT/config.clean"
grep -q '^unbindall$' "$OUT/config.clean" || { echo "FAIL: config lacks unbindall"; exit 1; }
grep -q '^bind "q" "+forward"$' "$OUT/config.clean" || {
    echo "FAIL: remapped binding was not serialized"; exit 1;
}
if grep -q '^bind "w" ' "$OUT/config.clean"; then
    echo "FAIL: config rewrite resurrected an unbound key"; exit 1
fi
grep -q '^skill "2"$' "$OUT/config.clean" || { echo "FAIL: archived skill was not written"; exit 1; }
grep -q '^fov "100"$' "$OUT/config.clean" || { echo "FAIL: archived fov was not written"; exit 1; }
echo "ok: config startup precedence, binding persistence and cvar archive"
