#!/bin/bash
# Regression test: no sprintf may overrun its char array, and tools/bufcheck.py must see one.
#
# The old main wrote its run summary into buf[96]: the fight= line needs
# 145 bytes at 16-bit widths, and the models line appended a kind at a
# time into the same 96. Nothing failed; the stack under the frame took
# it. The second arm is that source, from 5f8c8a1: a bufcheck that stops
# reporting its fight= line is blind.
#
#   tools/test-bufcheck.sh
set -uo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
fail=0

out=$(python3 "$ROOT/tools/bufcheck.py" "$ROOT"/src/*.c)
if [[ $? -ne 0 ]]; then
    grep '^OVERRUN' <<< "$out" >&2
    echo "FAIL: a sprintf in src/ can overrun its array" >&2
    fail=1
fi

old="$(mktemp -d)/qmain.c"
git -C "$ROOT" show 5f8c8a1:src/qmain.c > "$old"
# bufcheck exits 1 on the overruns it is meant to find, so read its output, not its status
oldout=$(python3 "$ROOT/tools/bufcheck.py" "$old")
if ! grep -q '^OVERRUN .*fight=health.*needs 145 of buf\[96\]' <<< "$oldout"; then
    echo "FAIL: bufcheck no longer sees 5f8c8a1's fight= line overrun buf[96]" >&2
    fail=1
fi
rm -rf "$(dirname "$old")"

[[ $fail == 0 ]] && echo "ok: $(tail -1 <<< "$out")"
exit $fail
