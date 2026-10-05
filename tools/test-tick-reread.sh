#!/bin/bash
# Regression test: the waits on the BIOS tick re-read it each pass.
# Read through a plain pointer, llrm-c inlined bios_ticks() and merged the
# two reads, so `while (bios_ticks() == t0)` spun forever and the run
# stopped in timer calibration (-Os: before ai_visible, combat_nightmare and
# savegame's self-tests; -O2 stalled too).
#
# llrm-c does not always inline bios_ticks into sys_time_init, which hides
# the bug. So the real bios_ticks() is also waited on in a small harness
# where it is inlined: a plain read there compiles to an empty `jmp` loop.
#
#   LLRM=path/to/llrm-c tools/test-tick-reread.sh
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/.." && pwd)"
LLRM="${LLRM:?set LLRM to llrm-c}"
SRC="$ROOT/src/host/sys_time.c"
S="$(mktemp -d)"; trap 'rm -rf "$S"' EXIT

inc=(); for d in host render model game sound ui qgl; do inc+=(-I "$ROOT/src/$d"); done

# usage: loops.py PROC WAITS  -- asm on stdin. A wait is a loop (a backward
# jump to an earlier label) whose body reads the tick, inline or by the call.
cat > "$S/loops.py" <<'PY'
import re, sys
proc, need = sys.argv[1], int(sys.argv[2])
text = sys.stdin.read()
m = re.search(r"^%s proc.*?^%s endp" % (proc, proc), text, re.S | re.M)
lines = m.group(0).splitlines()
at = {l.rstrip(":"): i for i, l in enumerate(lines) if re.fullmatch(r"\w+:", l)}
spans = [lines[at[m.group(1)]:i] for i, l in enumerate(lines)
         if (m := re.match(r"\s+j\w+ (\w+)$", l)) and at.get(m.group(1), i + 1) <= i]
waits = sum(any("es:[" in l or "call _bios_ticks" in l for l in span) for span in spans)
if waits < need:
    sys.exit("%d of the %d waits in %s re-read the tick" % (waits, need, proc))
PY

{ awk '/^static unsigned long bios_ticks/,/^}/' "$SRC"
  echo 'unsigned long out;
void wait_edge(void) { unsigned long t0 = bios_ticks(); while (bios_ticks() == t0) ; out = t0; }'
} > "$S/h.c"
grep -q 'bios_ticks( void )' "$S/h.c" || { echo "FAIL: bios_ticks not found in sys_time.c"; exit 1; }

rc=0
for opt in -Os -O2; do
    "$LLRM" $opt "$S/h.c" -S -o "$S/h.asm"
    python3 "$S/loops.py" _wait_edge 1 < "$S/h.asm" || { echo "FAIL $opt harness"; rc=1; }
    "$LLRM" $opt "${inc[@]}" ${QCINC:+-I "$QCINC"} "$SRC" -S -o "$S/t.asm"
    python3 "$S/loops.py" _sys_time_init 2 < "$S/t.asm" || { echo "FAIL $opt sys_time_init"; rc=1; }
done
[ $rc = 0 ] && echo "ok: the tick waits re-read it at -Os and -O2"
exit $rc
