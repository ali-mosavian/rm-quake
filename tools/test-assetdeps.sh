#!/bin/bash
# Regression test: make build copied data/assets/dm3ish.qmp without asking whether mkassets.py
# had changed since it was made. An ents.bin header change shipped the old dm3ish.qmp, the
# reader took its blank map name for 8224 messages, and test-exit's first run hung in its
# first frame. With mkassets.py newer and an up-to-date copy in the build, make must plan
# the asset run and then the copy.
#
# And every tool mkassets.py runs or imports, and what those import from tools/: mkmdl.py
# and mdlview.py were not prerequisites, so a .geo header change left every map's container
# holding models the loader refuses, and make said everything was up to date.
#
#   tools/test-assetdeps.sh
set -uo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
mkdir -p "$ROOT/build"
T="$(mktemp -d "$ROOT/build/assetdeps.XXXXXX")" || { echo "FAIL: assetdeps: no temporary build dir" >&2; exit 1; }
trap 'rm -rf "$T"' EXIT
cp "$ROOT/data/assets/dm3ish.qmp" "$T/" && touch "$T/dm3ish.qmp"
fail=0

# captured first: under pipefail, grep -q closing the pipe early fails make with SIGPIPE
plan=$(make -C "$ROOT" -n -W tools/mkassets.py BUILD="$T" "$T/dm3ish.qmp" 2>/dev/null)
mk=$(grep -n "mkassets.py data/dm3ish.bsp" <<< "$plan" | cut -d: -f1 | head -1)
cp=$(grep -n "cp .*dm3ish.qmp $T/dm3ish.qmp" <<< "$plan" | cut -d: -f1 | head -1)
if [[ -n "$mk" && -n "$cp" && "$mk" -lt "$cp" ]]; then
    echo "ok: assetdeps: a newer mkassets.py rebuilds dm3ish.qmp, then the build copies it"
else
    echo "FAIL: assetdeps: with mkassets.py newer, make plans mkassets at ${mk:-never} and the copy at ${cp:-never}" >&2
    fail=1
fi

if [[ ! -f "$ROOT/data/e1m1.bsp" ]]; then
    echo "SKIP: assetdeps: no data/e1m1.bsp, so no map container to plan"
    exit $fail
fi
tools=$(python3 - "$ROOT/tools" <<'PY'
import re
import sys
from pathlib import Path

d = Path(sys.argv[1])
seen, todo = set(), ["mkassets.py"]
while todo:
    t = todo.pop()
    if t in seen or not (d / t).exists():
        continue
    seen.add(t)
    src = (d / t).read_text()
    todo += re.findall(r"['\"]([a-z_0-9]+\.py)['\"]", src)
    todo += [m + ".py" for m in re.findall(r"^\s*(?:import|from) (\w+)", src, re.M)]
print(" ".join(sorted(seen)))
PY
)
missed=""
for t in $tools; do
    plan=$(make -C "$ROOT" -n -W "tools/$t" data/maps/e1m1/assets.zip 2>/dev/null)
    grep -q "mkassets.py data/e1m1.bsp" <<< "$plan" || missed="$missed $t"
done
if [[ -z "$missed" ]]; then
    echo "ok: assetdeps: a newer copy of any of $tools rebuilds e1m1's container"
else
    echo "FAIL: assetdeps: with$missed newer, make leaves e1m1's container as it is" >&2
    fail=1
fi
exit $fail
