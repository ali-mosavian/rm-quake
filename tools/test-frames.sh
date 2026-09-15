#!/bin/bash
# test-frames.sh -- a refactor must not move a pixel, the player or the game state.
#
#   tools/test-frames.sh <build-dir>          compare with tools/ref/frames.txt
#   tools/test-frames.sh <build-dir> --save   write the reference from this build
#
# Each case is one bounded run in its own directory, all in parallel.
# Compared: BENCH.BMP's md5, the player's pos=, the gs_state line, the
# hash of every host_state_sum line and the set of bench.txt's keys.
# Not frames or polys: a faster build renders more frames over the same
# ticks (test-cdefs.sh), which is a refactor working, not failing.
# The reference is real output from a named commit, never generated.
#
# The replay cases play tools/ref/<case>.rec, a -record of the live run
# saved with the reference: the recorded input and per-frame dt drive the
# run instead of this machine's clock, and a replay that leaves the
# recorded camera path fails on its DIVERGED line.
set -uo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
B="$(cd "${1:?usage: test-frames.sh <build-dir> [--save]}" && pwd)"
REF="$ROOT/tools/ref/frames.txt"
SAVE=$([[ "${2:-}" == --save ]] && echo 1)
BASE="-lm -nostats -nosound"
STILL="-noai -ticks 120"
# the live cases: AI on, walking and firing, so the monsters, the movers,
# the pickups and the kit all move -- compared through host_state_sum's
# E9 lines as well as the last frame
LIVE="-ticks 600 -walk -fire"

CASES=(
    "dm3ish-spawn|dm3ish|$STILL"
    "e1m1-spawn|e1m1|$STILL"
    "e1m1-door|e1m1|$STILL -at 96 569 24 -yaw 177"
    "e1m1-styles|e1m1|$STILL -at 398 2130 -93 -yaw 180"
    "e1m3-spawn|e1m3|$STILL"
    "e1m3-trigger|e1m3|$STILL -at -194 -1724 112 -yaw 275"
    "e1m4-spawn|e1m4|$STILL"
    "e1m7-spawn|e1m7|$STILL"
    "e1m1-live|e1m1|$LIVE"
    "e1m3-live|e1m3|$LIVE"
)
REPLAY=(
    "e1m1-replay|e1m1|$LIVE"
    "e1m3-replay|e1m3|$LIVE"
)

qmp() { [[ $1 == dm3ish ]] && echo "$B/dm3ish.qmp" || echo "$ROOT/data/maps/$1/$1.qmp"; }

run_case() {   # name map flags [rec | play <file>]
    local name=$1 map=$2 extra=$3 mode=${4:-} rec=${5:-} d="$B/frames-$1"
    rm -rf "$d"; mkdir -p "$d"
    cp "$B/QCPORT.EXE" "$B/stuff.ini" "$(qmp "$map")" "$d/"
    case $mode in
        rec)  extra+=" -record R.BIN" ;;
        play) cp "$rec" "$d/R.BIN"; extra+=" -play R.BIN" ;;
    esac
    TIMEOUT="${TIMEOUT:-600}" "$ROOT/tools/run.sh" "$d" "$map.qmp $BASE $extra" > /dev/null 2>&1
}

# The recording is staged in $B and reaches tools/ref only with a saved
# reference. No recording leaves no frame, so --save refuses on MISSING
# rather than saving the recording run's row as a replay.
record_then_play() {   # name map flags
    local d="$B/frames-$1" rec="$B/$1.rec"
    rm -f "$rec"
    run_case "$1" "$2" "$3" rec
    [[ -s "$d/R.BIN" ]] || { rm -f "$d/BENCH.BMP"; return; }
    cp "$d/R.BIN" "$rec"
    run_case "$1" "$2" "$3" play "$rec"
}

if [[ -z $SAVE ]]; then
    for c in "${REPLAY[@]}"; do
        IFS='|' read -r n _ _ <<< "$c"
        [[ -s "$ROOT/tools/ref/$n.rec" ]] || { echo "no tools/ref/$n.rec: run with --save"; exit 1; }
    done
fi

for c in "${CASES[@]}"; do IFS='|' read -r n m x <<< "$c"; run_case "$n" "$m" "$x" & done
for c in "${REPLAY[@]}"; do
    IFS='|' read -r n m x <<< "$c"
    if [[ -n $SAVE ]]; then
        record_then_play "$n" "$m" "$x" &
    else
        run_case "$n" "$m" "$x" play "$ROOT/tools/ref/$n.rec" &
    fi
done
wait

got=$(for c in "${CASES[@]}" "${REPLAY[@]}"; do
    IFS='|' read -r n _ _ <<< "$c"; d="$B/frames-$n"
    md5=$( [[ -s "$d/BENCH.BMP" ]] && md5 -q "$d/BENCH.BMP" || echo MISSING )
    pos=$(tr -d '\r' < "$d/cstep.txt" 2>/dev/null | grep -o 'pos=[^ ]*' | tail -1)
    gs=$(tr -d '\r' < "$d/cstep.txt" 2>/dev/null | grep '^gs_state' | tail -1 | tr ' ' '_')
    sums=$(tr -d '\r' < "$d/cstep.txt" 2>/dev/null | grep '^sum ')
    sumk="sums=$(grep -c . <<< "$sums"):$(md5 -q -s "$sums")"
    # bench.txt's key set, not its values: timings differ run to run
    keys="keys=$(tr -d '\r' < "$d/BENCH.TXT" 2>/dev/null | awk '{print $1}' | sort -u | md5 -q)"
    div=""
    [[ $n == *-replay ]] && div=" div=$(tr -d '\r' < "$d/cstep.txt" 2>/dev/null | grep -c DIVERGED)"
    echo "$n $md5 ${pos:-nopos} ${gs:-nogs} $sumk $keys$div"
done)

if [[ -n $SAVE ]]; then
    grep -q MISSING <<< "$got" && { echo "refusing to save: a case rendered nothing"; echo "$got"; exit 1; }
    grep -q "div=[1-9]" <<< "$got" && { echo "refusing to save: a replay diverged from its own recording"; echo "$got"; exit 1; }
    mkdir -p "$(dirname "$REF")"
    dirty=$(git -C "$ROOT" diff --quiet HEAD -- src qgl || echo "+uncommitted")
    { echo "# from $(git -C "$ROOT" rev-parse --short HEAD)$dirty, $BASE"; echo "$got"; } > "$REF" || exit 1
    for c in "${REPLAY[@]}"; do IFS='|' read -r n _ _ <<< "$c"; cp "$B/$n.rec" "$ROOT/tools/ref/$n.rec" || exit 1; done
    echo "saved $(wc -l <<< "$got" | tr -d ' ') cases to $REF"; exit 0
fi

[[ -s "$REF" ]] || { echo "no reference: run with --save on a known-good build"; exit 1; }
bad=$(diff <(grep -v '^#' "$REF") <(echo "$got"))
if [[ -z "$bad" ]]; then echo "ok: $(wc -l <<< "$got" | tr -d ' ') cases identical to $(head -1 "$REF")"; exit 0; fi
echo "FAIL:"; echo "$bad"; exit 1
