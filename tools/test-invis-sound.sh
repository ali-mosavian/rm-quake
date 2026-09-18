#!/bin/bash
# Regression test: the Ring of Shadows idle whisper enters the mixer at id's half volume.
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/.." && pwd)"
OUT="$(cd "${1:?usage: test-invis-sound.sh <build-dir>}" && pwd)"

[[ -f "$OUT/e1m3.qmp" ]] || cp "$ROOT/data/maps/e1m3/e1m3.qmp" "$OUT/"
TIMEOUT="${TIMEOUT:-60}" "$HERE/run.sh" "$OUT" \
  "e1m3.qmp -nostats -noai -at -1736 -552 -336 -ticks 1" >/dev/null

grep -q '^snd seen=.* invvol=127\( \|$\)' "$OUT/cstep.txt" || {
  echo "Ring of Shadows whisper did not play at half volume" >&2
  grep '^snd seen=' "$OUT/cstep.txt" >&2 || true
  exit 1
}

echo "ok: Ring of Shadows whisper volume 127/255"
