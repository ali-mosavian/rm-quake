#!/bin/bash
# Regression test: id's SND_Spatialize sends a source on the listener's
# left to the left channel, then swaps the channels when the view turns 180.
# Mono output produced equal channel volumes and could not pass this.
set -uo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/.." && pwd)"
OUT="${1:?usage: test-stereo.sh <build-dir>}"
RUN="${RUN_SH:-$HERE/run.sh}"
LOG="$OUT/cstep.txt"

cp "$ROOT/data/maps/e1m1/e1m1.qmp" "$OUT/"
cp "$ROOT/data/stuff.ini" "$OUT/"

pan() {
    TIMEOUT="${TIMEOUT:-180}" "$RUN" "$OUT" \
        "e1m1.qmp -noai -at 88 1420 -190 -yaw $1 -ticks 60 -nostats -bench 4000" >/dev/null 2>&1
    tr -d '\r' < "$LOG" | sed -n 's/^snd seen=.* pan=[0-9-]*:\([0-9-]*,[0-9-]*\).*/\1/p'
}

a=$(pan 0)
b=$(pan 180)
[[ -n "$a" && -n "$b" ]] || { echo "FAIL: no placed stereo voice ($a / $b)" >&2; exit 1; }
(( ${a%,*} > ${a#*,} )) || { echo "FAIL: source on the left panned $a" >&2; exit 1; }
(( ${b%,*} < ${b#*,} )) || { echo "FAIL: source on the right panned $b" >&2; exit 1; }
[[ "$b" == "${a#*,},${a%,*}" ]] || { echo "FAIL: $a turned 180 became $b" >&2; exit 1; }
echo "ok: stereo pan $a -> $b"
