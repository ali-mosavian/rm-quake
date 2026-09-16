#!/bin/bash
# Regression test: changelevel unloads and loads inside one process.
#
# A process-per-map implementation prints two starts and can leak every
# map allocation without consequence. One process must open both maps,
# restore its conventional and EMS baselines between them, and carry the
# player's kit.
#
#   tools/test-mapcycle.sh build/cport-mapcycle
set -uo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
OUT="${1:?usage: test-mapcycle.sh <build-dir>}"
RUN="${RUN_SH:-$HERE/run.sh}"

[[ -f "$OUT/e1m1.qmp" ]] || { echo "SKIP: no e1m1.qmp in $OUT"; exit 0; }
[[ -f "$OUT/e1m2.qmp" ]] || { echo "SKIP: no e1m2.qmp in $OUT"; exit 0; }

WALK="-at 1312 660 -200 -yaw 90 -walk -jump"
TIMEOUT="${TIMEOUT:-600}" "$RUN" "$OUT" \
    "e1m1.qmp -nostats -noai $WALK -fire -ticks 300" >/dev/null 2>&1

log="$OUT/cstep.txt"
clean="$(mktemp)"
trap 'rm -f "$clean"' EXIT
tr -d '\r' < "$log" > "$clean"

starts=$(grep -c '^start$' "$clean")
maps=$(grep -c '^asset_map ok ' "$clean")
releases=$(grep -c '^map_release ok ' "$clean")
map2=$(grep '^asset_map ok ' "$clean" | tail -1 | awk '{print $3}')

[[ "$starts" -eq 1 ]] || { echo "FAIL: changelevel used $starts processes, wanted one" >&2; exit 1; }
[[ "$maps" -ge 2 && "$map2" == "e1m2.qmp" ]] || {
    echo "FAIL: loaded $maps maps; the last was ${map2:-nothing}, wanted e1m2.qmp" >&2
    exit 1
}
[[ "$releases" -ge 2 ]] || { echo "FAIL: only $releases of 2 maps restored the memory baseline" >&2; exit 1; }
grep -q '^carry health ' "$clean" || { echo "FAIL: e1m2 did not receive the carried kit" >&2; exit 1; }
grep -q '^restored$' "$clean" || { echo "FAIL: the session did not shut down cleanly" >&2; exit 1; }
[[ ! -s "$OUT/error.log" ]] || { echo "FAIL: $(<"$OUT/error.log")" >&2; exit 1; }

echo "ok: e1m1 released at baseline and e1m2 loaded in the same process"
