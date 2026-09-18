#!/bin/bash
# Regression test: the build must refuse an EXE that cannot start.
#
# What went wrong: Borland's medium-model startup puts near data, the
# stack and the near heap in one 64K DGROUP, and over 64K it calls
# abort() BEFORE main -- "Abnormal program termination", on stderr,
# which DOS does not redirect. The build was 22 bytes inside the limit
# with _stklen 32768; two shorts added to a struct crossed it, and the
# run then produced an empty redirect, no cstep.txt and a nonzero exit
# with no message anywhere. Nothing reported the cause, and it read as
# a broken build for a session.
#
#   tools/test-dgroup.sh build/cport-e1m1
#
# Two arms: the build that is here must have near-heap room, and the
# guard must reject a map that does not. CHECK_SH overrides the guard,
# which is how this was mutation-checked.
set -uo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
OUT="${1:?usage: test-dgroup.sh <build-dir>}"
CHECK="${CHECK_SH:-$HERE/dgroup-check.sh}"
MAP="$OUT/QCPORT.MAP"

fail=0
[[ -s "$MAP" ]] || { echo "FAIL: no $MAP" >&2; exit 1; }

STKLEN=$(sed -n 's/^unsigned _stklen = \([0-9]*\)U*;.*/\1/p' "$HERE/../src/host/qmain.c")
if [[ -z "$STKLEN" ]]; then
    echo "FAIL: no _stklen in src/host/qmain.c -- the guard has nothing to check" >&2
    fail=1
elif ! "$CHECK" "$MAP" "$STKLEN" >/dev/null 2>&1; then
    echo "FAIL: this build has no near-heap room: $("$CHECK" "$MAP" "$STKLEN" 2>&1 | head -2)" >&2
    fail=1
else
    echo "ok: $("$CHECK" "$MAP" "$STKLEN")"
fi

# And the budget. sc_selftest_short's 256 hog pointers went in as a
# static, 1,024 bytes of DGROUP for a table used once at startup: near
# heap 7,404 to 6,380. Move this floor with a reason, never to go green.
NEAR_MIN=7000
if [[ -n "$STKLEN" ]] && ! "$CHECK" "$MAP" "$STKLEN" "$NEAR_MIN" >/dev/null 2>&1; then
    echo "FAIL: near heap under $NEAR_MIN: $("$CHECK" "$MAP" "$STKLEN" 2>&1 | head -1)" >&2
    fail=1
elif [[ -n "$STKLEN" ]]; then
    echo "ok: near heap at least $NEAR_MIN"
fi

# Synthesize a map that leaves one byte less than the guard's minimum.
# A historical fixed size stopped being invalid when _stklen was reduced.
TMP="$(mktemp -d)"; trap 'rm -rf "$TMP"' EXIT
OVER=$((65536 - STKLEN - 4095))
printf 'Group                           Address              Size\n=====\nDGROUP                          288b:0000            %08x\n' "$OVER" > "$TMP/over.map"
if "$CHECK" "$TMP/over.map" "$STKLEN" >/dev/null 2>&1; then
    echo "FAIL: the guard passed DGROUP $OVER + stack $STKLEN with under 4096 bytes free" >&2
    fail=1
else
    echo "ok: the guard rejects a DGROUP with under 4096 bytes free"
fi

exit $fail
