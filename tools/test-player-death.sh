#!/bin/bash
# Regression: single-player respawn is a whole-map restart using the level's
# saved entry parms, not the inventory at death. It also requires release then
# a later attack/use/jump press; the held shot that kills nothing cannot skip it.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="$(cd "${1:?usage: test-player-death.sh <build-dir>}" && pwd)"
WORK="$(mktemp -d "$BUILD/player-death.XXXXXX")"
trap 'rm -rf "$WORK"' EXIT

cp "$BUILD/QCPORT.EXE" "$BUILD/stuff.ini" "$BUILD/default.cfg" \
   "$ROOT/data/maps/e1m7/e1m7.qmp" "$WORK/"

# The level-entry kit has seven rockets. Fire one, save the six-rocket live
# state, reload it, die in lava, then restart: the new map must have seven.
python3 - "$WORK/CARRY.BIN" <<'PY'
import struct
import sys

open(sys.argv[1], "wb").write(struct.pack(
    "<lhhfhhhhhhh", 1 | 128 | 4096, 100, 0, 0.0, 25, 0, 7, 128, 0, 0, 0
))
PY

{
    for _ in $(seq 1 2); do echo wait; done
    echo 'save s0'
    echo quit
} > "$WORK/save.cfg"

KEEP_CARRY=1 CYCLES=max TIMEOUT="${TIMEOUT:-180}" "$ROOT/tools/run.sh" "$WORK" \
    "e1m7.qmp -lm -nostats -noai -nosound -carry -fire -at 404 64 -44 +exec save.cfg" \
    >/dev/null
saved=$(tr -d '\r' < "$WORK/cstep.txt" | grep '^fight=' | tail -1 || true)

{
    for _ in $(seq 1 140); do echo wait; done
    echo '-attack'
    echo wait
    echo wait
    echo '+attack'
    echo wait
    echo '-attack'
    for _ in $(seq 1 8); do echo wait; done
    echo quit
} > "$WORK/death.cfg"

KEEP_CARRY=1 CYCLES=max TIMEOUT="${TIMEOUT:-180}" "$ROOT/tools/run.sh" "$WORK" \
    "e1m7.qmp -lm -nostats -noai -nosound -at 404 64 -44 +load s0;exec death.cfg" \
    >/dev/null

reports=$(tr -d '\r' < "$WORK/cstep.txt" | grep '^fight=' || true)
middle=$(sed -n '2p' <<< "$reports")
final=$(sed -n '3p' <<< "$reports")
death=$(tr -d '\r' < "$WORK/cstep.txt" | grep '^death set=' | sed -n '2p')
rockets_before=$(sed -n 's/.* rockets \([0-9][0-9]*\).*/\1/p' <<< "$middle")
health_before=$(sed -n 's/^fight=health \([-0-9][0-9]*\).*/\1/p' <<< "$middle")
rockets_after=$(sed -n 's/.* rockets \([0-9][0-9]*\).*/\1/p' <<< "$final")
health_after=$(sed -n 's/^fight=health \([-0-9][0-9]*\).*/\1/p' <<< "$final")
releases=$(grep -c '^map_release ok ' "$WORK/cstep.txt" || true)
selftest=$(grep -c '^pl_death_selftest 1' "$WORK/cstep.txt" || true)
gibtest=$(grep -c '^gib_selftest 1' "$WORK/cstep.txt" || true)

if [[ "$saved" != *' rockets 6 '* || "$rockets_before" != 6 || "${health_before:-1}" -ge 1 ||
      "$rockets_after" != 7 || "$health_after" != 100 ||
      "$death" != *'phase=2' || "$releases" -ne 3 ||
      "$selftest" -ne 1 || "$gibtest" -ne 1 ||
      $(grep -c '^savegame loaded ' "$WORK/cstep.txt" || true) -ne 1 ]]; then
    echo "FAIL: save/death/restart reported [$saved] [$middle] [$death] [$final], releases $releases" >&2
    exit 1
fi

echo "ok: save kept six live rockets; death restarted the map with its seven-rocket entry kit"
