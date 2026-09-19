#!/bin/bash
# Regression test: entering dm3ish's underwater tunnel at low frame rate must
# rebuild the PVS and finish, not recurse into a corrupt return and freeze.
#
# The failing build reaches tick 126, then dies while marking the new leaf's
# visible BSP subtrees; DOSBox-X reports an illegal dynrec opcode afterwards.
set -uo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
OUT="${1:?usage: test-pvs-transition.sh <build-dir>}"

if ! CYCLES=120000 CORE=dynamic TIMEOUT="${TIMEOUT:-30}" \
    "${RUN_SH:-$HERE/run.sh}" "$OUT" \
    "dm3ish.qmp -lm -nosound -noitems -at 487 254 -103 -yaw 248 -walk -ticks 140" \
    >/dev/null 2>&1; then
    echo "FAIL: DOSBox-X runner failed" >&2
    exit 1
fi

log="$OUT/cstep.txt"
clean_log() { tr -d '\r' < "$log" 2>/dev/null; }

if ! clean_log | grep -q '^frames=.* ticks=140 '; then
    echo "FAIL: underwater PVS transition did not complete 140 ticks" >&2
    exit 1
fi
if ! clean_log | grep -q '^map_release ok '; then
    echo "FAIL: map memory was not restored" >&2
    exit 1
fi
if ! clean_log | grep -q '^restored$'; then
    echo "FAIL: runtime did not shut down cleanly" >&2
    exit 1
fi
if [[ -s "$OUT/error.log" ]]; then
    echo "FAIL: $(tr -d '\r' < "$OUT/error.log")" >&2
    exit 1
fi

echo "ok: underwater PVS transition completed 140 ticks"
