#!/bin/sh
# Regression test: approaching dm3ish's lift from (32,608,24) used to start
# it at the outer edge, then reverse it as the player crossed the trigger.
# The player remained below the upper doorway instead of riding the lift up.
set -eu

HERE=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
OUT="${1:?usage: test-plat-entry.sh <build-dir>}"

TIMEOUT="${TIMEOUT:-60}" "${RUN_SH:-$HERE/run.sh}" "$OUT" \
  "dm3ish.qmp -lm -nostats -nosound -noai -at 32 608 24 -yaw 163 -walk -ticks 120" \
  >/dev/null 2>&1

line=$(tr -d '\r' < "$OUT/cstep.txt" | grep '^frames=.* ticks=120 ')
pos=${line##* pos=}
IFS=, read -r x eye_z y <<EOF
$pos
EOF

if [ "$x" -gt -100 ] || [ "$eye_z" -lt 250 ]; then
  echo "FAIL: dm3ish lift approach ended at $pos; it did not carry the player toward the upper doorway" >&2
  exit 1
fi

echo "ok: dm3ish lift approach reached $pos while the lift continued upward"
