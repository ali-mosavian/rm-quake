#!/bin/bash
# Regression test: func_train runs. The plat tick walked the whole plat array,
# trains included, and set every entry's state to plat UP or DOWN each tick, so a
# train never reached its wait and never left its first corner. e1m5's four
# untargeted trains read one brush checksum for ten seconds. Ten seconds must now
# show at least three distinct `br` values on the state checksum lines.
#
#   tools/test-trains.sh build/cport-x
set -uo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/.." && pwd)"
OUT="${1:?usage: test-trains.sh <build-dir>}"
RUN="${RUN_SH:-$HERE/run.sh}"
log="$OUT/cstep.txt"

[[ -f "$OUT/e1m5.qmp" ]] || cp "$ROOT/data/maps/e1m5/e1m5.qmp" "$OUT/"
cp "$ROOT/data/stuff.ini" "$OUT/" 2>/dev/null

TIMEOUT="${TIMEOUT:-600}" "$RUN" "$OUT" "e1m5.qmp -noai -nostats -bench 4000 -ticks 600" >/dev/null 2>&1
n=$(tr -d '\r' < "$log" 2>/dev/null | sed -n 's/^sum .* br \([-0-9]*\) .*/\1/p' | sort -u | wc -l | tr -d ' ')
if [[ "${n:-0}" -lt 3 ]]; then
    echo "FAIL: e1m5's trains gave ${n:-no} distinct brush checksums in 10 s, want 3 or more" >&2
    exit 1
fi
echo "ok: e1m5's trains gave $n distinct brush checksums in 10 s"
