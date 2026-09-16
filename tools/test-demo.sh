#!/bin/bash
# test-demo.sh -- a -record file must replay as the run it recorded.
#
#   tools/test-demo.sh <build-dir>
#
# The recorder handed its far buffer to fwrite, which takes a near pointer:
# every R.BIN was DGROUP ("Borland C++ - Copyright 1991" at byte 2), the
# same bytes run to run, and -play of it diverged at frame 0 wanting 0,0,0.
# With that fixed the replay still drifted at frame 15, 4 units: a frame's
# dt was not recorded, so each replayed frame ran the replaying clock's
# ticks. Both arms hold -walk, which the recording does not carry as input.
set -uo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
B="$(cd "${1:?usage: test-demo.sh <build-dir>}" && pwd)"
RUN="e1m1.qmp -lm -nostats -nosound -noai -ticks 120"

for arm in rec play; do
    d="$B/demo-$arm"; rm -rf "$d"; mkdir -p "$d"
    cp "$B/QCPORT.EXE" "$B/stuff.ini" "$ROOT/data/maps/e1m1/e1m1.qmp" \
       "$ROOT/tools/ref/attack.cfg" "$d/"
done
TIMEOUT="${TIMEOUT:-600}" "$ROOT/tools/run.sh" "$B/demo-rec" \
  "$RUN -record R.BIN +exec attack.cfg" > /dev/null 2>&1
[[ -s "$B/demo-rec/R.BIN" ]] || { echo "FAIL: no R.BIN"; exit 1; }
cp "$B/demo-rec/R.BIN" "$B/demo-play/"
TIMEOUT="${TIMEOUT:-600}" "$ROOT/tools/run.sh" "$B/demo-play" "$RUN -play R.BIN" > /dev/null 2>&1

step() { tr -d '\r' < "$B/demo-$1/cstep.txt" 2>/dev/null; }
size=$(wc -c < "$B/demo-rec/R.BIN" | tr -d ' ')
rf=$(step rec | grep -o 'frames=[0-9]*'); pf=$(step play | grep -o 'frames=[0-9]*')
fail=""
(( (size - 4) % 38 == 0 )) || fail+=" size $size is not header plus whole records;"
[[ "$(dd if="$B/demo-rec/R.BIN" bs=1 count=4 2>/dev/null)" == "QCR2" ]] || fail+=" missing QCR2 header;"
attack=$(od -An -td2 -j8 -N2 "$B/demo-rec/R.BIN" | tr -d ' ')
[[ "$attack" == "1" ]] || fail+=" first action record attack=$attack;"
grep -q '^fight=.* shells 2[0-4] ' "$B/demo-rec/cstep.txt" || fail+=" attack did not spend shells;"
grep -q "Borland" "$B/demo-rec/R.BIN" && fail+=" R.BIN holds DGROUP;"
[[ -n "$rf" && "$rf" == "$pf" ]] || fail+=" record ${rf:-noframes} play ${pf:-noframes};"
div=$(step play | grep DIVERGED) && fail+=" $div;"
[[ -z "$fail" ]] || { echo "FAIL:$fail"; exit 1; }
echo "ok: $(((size - 4) / 38)) action records, $rf, attack-only replayed without drift"
