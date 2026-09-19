#!/bin/bash
# Regression test: the slipgate, the intermission, and the next map.
#
# What it is written against, each arm mutation-checked:
#
#   0. Nothing restored INT 8 or INT 9 on the way out. DOS does not do
#      it for a program that ends, so another run in a session
#      installed its timer over the first run's dead handler and
#      chained to it: the BIOS tick stopped advancing and
#      sys_time_init spun in its tick-edge wait for ever, two marks
#      into the load. Invisible while one run was the whole session.
#   1. ENT_TRIG_EXIT had no case in ent_move_trigs at all -- a touch on
#      e1m1's changelevel volume did nothing, so the level never ended.
#      Without the case the run reads gs_state 1 and the camera stays
#      where it walked; with it, gs_state 4 at the map's own
#      info_intermission, (-112,704).
#   2. changelevel releases e1m1 and loads e1m2 in the same process.
#      The resource baseline catches leaks before the second load.
#   3. PlayerCarry carries the kit in memory. The first leg runs
#      WITH the AI, so e1m1's soldiers take health off before the exit
#      -- a fresh kit is exactly 100, so a carry that does nothing is
#      visible. That is the whole reason this arm does not use -noai.
#
#   tools/test-exit.sh build/cport-e1m1
#
# Needs e1m1.qmp AND e1m2.qmp staged; skips without either.
set -uo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
OUT="${1:?usage: test-exit.sh <build-dir>}"
RUN="${RUN_SH:-$HERE/run.sh}"

[[ -f "$OUT/e1m1.qmp" ]] || { echo "SKIP: no e1m1.qmp in $OUT"; exit 0; }
[[ -f "$OUT/e1m2.qmp" ]] || { echo "SKIP: no e1m2.qmp in $OUT"; exit 0; }

log="$OUT/cstep.txt"
fail=0

# 0: a run after a run. dm3ish because it loads in seconds and this
# arm is about the machine the first run left behind, not the map.
QARGS2="dm3ish.qmp -nostats -noai -ticks 5" \
TIMEOUT="${TIMEOUT:-300}" "$RUN" "$OUT" "dm3ish.qmp -nostats -noai -ticks 5" >/dev/null 2>&1
runs=$(tr -d '\r' < "$log" 2>/dev/null | grep -c '^frames=')
if [[ "$runs" -lt 2 ]]; then
    echo "FAIL: $runs of 2 runs in one session reached a frame -- the ISRs were left installed" >&2
    fail=1
fi

# The slipgate is 32 units up, past the 18 a step climbs, so the walk
# jumps into it -- check.sh's own e1m1 arm, same viewpoint.
WALK="-at 1312 660 -200 -yaw 90 -walk -jump"

# 1: the intermission. No -fire, so nothing goes on from it.
TIMEOUT="${TIMEOUT:-600}" "$RUN" "$OUT" "e1m1.qmp -nostats -noai $WALK -ticks 240" >/dev/null 2>&1
gs=$(tr -d '\r' < "$log" 2>/dev/null | grep -m1 '^gs_state ' | awk '{print $2}')
pos=$(tr -d '\r' < "$log" 2>/dev/null | grep -m1 '^frames=' | sed 's/.*pos=//')
IFS=, read px py pz <<< "${pos:-0,0,0}"
if [[ "$gs" != "4" ]]; then
    echo "FAIL: walked into the slipgate and gs_state is ${gs:-none}, not 4 (GS_EXIT)" >&2
    fail=1
elif [[ $(( (px+112)*(px+112) + (pz-704)*(pz-704) )) -gt 400 ]]; then
    echo "FAIL: GS_EXIT but the camera is at $pos, not e1m1's intermission (-112,*,704)" >&2
    fail=1
fi

# 2 and 3: the changelevel, with the AI on so the carry has something
# to say. cstep.txt holds both maps under one start marker.
TIMEOUT="${TIMEOUT:-1200}" "$RUN" "$OUT" "e1m1.qmp -nostats $WALK -fire -ticks 300" >/dev/null 2>&1
starts=$(tr -d '\r' < "$log" 2>/dev/null | grep -c '^start$')
legs=$(tr -d '\r' < "$log" 2>/dev/null | grep -c '^asset_map ok ')
if [[ "$starts" -ne 1 || "$legs" -lt 2 ]]; then
    echo "FAIL: changelevel used $starts process(es) and loaded $legs map(s); wanted one and two" >&2
    fail=1
else
    # the second leg's OWN load mark, not the last gs_state line: what
    # is under test is the changelevel, and e1m2 running its carried
    # -ticks to the end is a much longer run than that claim needs.
    map2=$(tr -d '\r' < "$log" | grep '^asset_map ok ' | tail -1 | awk '{print $3}')
    [[ "$map2" == "e1m2.qmp" ]] || { echo "FAIL: the second leg opened ${map2:-nothing}, not e1m2.qmp" >&2; fail=1; }
    grep -q '^map_release ok ' "$log" || { echo "FAIL: e1m1 did not return to the session memory baseline" >&2; fail=1; }

    carry=$(tr -d '\r' < "$log" | grep -m1 '^carry health ')
    [[ -n "$carry" ]] || { echo "FAIL: e1m2 was started without -carry" >&2; fail=1; }
    ch=$(echo "$carry" | awk '{print $3}')
    left=$(tr -d '\r' < "$log" | grep -m1 '^fight=' | sed 's/.*health \([0-9-]*\).*/\1/')
    if [[ -n "$ch" && "$ch" -ge 100 ]]; then
        echo "FAIL: e1m1 ended on $left health and e1m2 carried $ch -- a fresh kit, not the carry" >&2
        fail=1
    fi
    if [[ -n "$ch" && "$ch" -lt 50 ]]; then
        echo "FAIL: carried $ch health; SetChangeParms floors it at 50" >&2
        fail=1
    fi
fi

# 4: each changelevel's own map. mkassets kept one a file, the last read, so
#    all five of start's slipgates led to "end" and e1m4's main exit to e1m8.
#    start's exits carry NO_INTERMISSION: entering one must load its map
#    without a button press or a tally screen.
if [[ -f "$OUT/start.qmp" ]]; then
    for gate in "-64 1628 176 e1m1" "-548 2368 208 e2m1"; do
        read -r gx gy gz want <<< "$gate"
        TIMEOUT="${TIMEOUT:-300}" "$RUN" "$OUT" "start.qmp -nostats -noai -at $gx $gy $gz -ticks 30" >/dev/null 2>&1
        next=$(tr -d '\r' < "$log" 2>/dev/null | grep -m1 '^gs_state ' | sed -n 's/.* next \([^ ]*\) .*/\1/p')
        [[ "$next" == "$want" ]] || { echo "FAIL: start's slipgate at $gx,$gy leads to ${next:-nothing}, not $want" >&2; fail=1; }
        if [[ "$want" == "e1m1" && -f "$OUT/e1m1.qmp" ]]; then
            map2=$(tr -d '\r' < "$log" | grep '^asset_map ok ' | tail -1 | awk '{print $3}')
            [[ "$map2" == "e1m1.qmp" ]] || {
                echo "FAIL: start's NO_INTERMISSION gate stopped at the tally instead of loading e1m1" >&2
                fail=1
            }
        fi
    done
else
    echo "SKIP arm 4: no start.qmp in $OUT"
fi

# 5: e1m7's exit fires t18. Its relay opens the five pairs of named
# monster teleporters over the intermission; without SUB_UseTargets no
# monster reaches the finale camera.
if [[ -f "$OUT/e1m7.qmp" ]]; then
    TIMEOUT="${TIMEOUT:-600}" "$RUN" "$OUT" \
        "e1m7.qmp -nostats -noai -nosound -at 1016 52 -1000 -ticks 450" >/dev/null 2>&1
    teles=$(tr -d '\r' < "$log" 2>/dev/null | sed -n 's/^ai .* teles=\([0-9-]*\) .*/\1/p')
    [[ "${teles:-0}" -gt 0 ]] || {
        echo "FAIL: e1m7's changelevel did not fire its monster-teleport target chain" >&2
        fail=1
    }
else
    echo "SKIP arm 5: no e1m7.qmp in $OUT"
fi

# 6: saves made before exit metadata shipped contain zero in the static
# NO_INTERMISSION slot. Loading one must rebind that field from the map,
# not restore the old intermission bug over the rebuilt entity data.
if [[ -f "$OUT/start.qmp" && -f "$OUT/e1m1.qmp" ]]; then
    savework=$(mktemp -d "$OUT/exit-save-test.XXXXXX")
    cp "$OUT/QCPORT.EXE" "$OUT/stuff.ini" "$OUT/default.cfg" \
       "$OUT/start.qmp" "$OUT/e1m1.qmp" "$savework/"
    TIMEOUT="${TIMEOUT:-300}" "$RUN" "$savework" \
        "start.qmp -nostats -noai -nosound -at -64 1628 176 -ticks 1 +save s0;quit" >/dev/null 2>&1
    python3 - "$savework/s0.sav" <<'PY'
import struct
import sys

path = sys.argv[1]
data = bytearray(open(path, "rb").read())
needle = struct.pack("<6h", 12, 4, 0, 0, 0, 0)
matches = [i for i in range(len(data)) if data.startswith(needle, i)]
if len(matches) != 1:
    raise SystemExit(f"start save has {len(matches)} candidates for exit model 12")
struct.pack_into("<h", data, matches[0] + 40, 0)
open(path, "wb").write(data)
PY
    TIMEOUT="${TIMEOUT:-300}" "$RUN" "$savework" \
        "start.qmp -nostats -noai -nosound -ticks 30 +load s0" >/dev/null 2>&1
    map2=$(tr -d '\r' < "$savework/cstep.txt" | grep '^asset_map ok ' | tail -1 | awk '{print $3}')
    [[ "$map2" == "e1m1.qmp" ]] || {
        echo "FAIL: old save restored stale exit metadata and stopped before e1m1" >&2
        fail=1
    }
    rm -rf "$savework"
fi

# 7: -carry with no CARRY.BIN (run.sh deletes it) loaded nothing, and the log still
#    printed the default kit as "carry health 100 ...". A short file is untested: every
#    -carry run used to follow the save that wrote it.
TIMEOUT="${TIMEOUT:-300}" "$RUN" "$OUT" "e1m1.qmp -nostats -noai -carry -ticks 2" >/dev/null 2>&1
if ! tr -d '\r' < "$log" 2>/dev/null | grep -q '^carry refused' || tr -d '\r' < "$log" | grep -q '^carry health '; then
    echo "FAIL: -carry with no CARRY.BIN is not reported refused" >&2
    fail=1
fi

[[ $fail -eq 0 ]] && echo "ok: the slipgate ends the level, e1m2 follows, and $ch health went with it"
exit $fail
