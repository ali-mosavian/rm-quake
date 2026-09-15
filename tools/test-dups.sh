#!/usr/bin/env bash
# test-dups.sh -- a name in two subsystems must stop the build.
#
# Objects land flat in BUILD and bcc.sh compiles against every header in one
# DOS directory, so a second rng.c or pal.h would silently build or include
# one of the two. The first guard read the module list after $(sort), which
# drops a repeat, and let two rng.c through.
#
# Works on a copy of the Makefile and src/, never the tree itself. make's
# output goes to a file before it is searched: `make | grep -q` under
# pipefail fails on the broken pipe when grep matches early.
set -uo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
T="$(mktemp -d)"
trap 'rm -rf "$T"' EXIT
fail=0

tree () {
    rm -rf "$T/tree"; mkdir -p "$T/tree"
    cp "$ROOT/Makefile" "$T/tree/"; cp -R "$ROOT/src" "$T/tree/"
}
plan () { make -n -C "$T/tree" "$T/b/QCPORT.EXE" BUILD="$T/b" > "$T/out.txt" 2>&1; }

stops () {   # what, file to copy, where
    tree
    cp "$T/tree/$2" "$T/tree/$3"
    plan
    if grep -q 'one name in two subsystems' "$T/out.txt"; then
        echo "ok: $1 in two subsystems stops the build"
    else
        echo "FAIL: $1 in two subsystems builds"; fail=1
    fi
}

stops "a C module" src/host/rng.c src/ui/rng.c
stops "an asm module" src/render/r_gvcpy.asm src/game/r_gvcpy.asm
stops "a header" src/render/pal.h src/ui/pal.h
# DOS and a case-insensitive disk see one file where make sees two
stops "a header differing in case" src/render/pal.h src/ui/Pal.h

tree
if plan && grep -q 'bcc\.sh' "$T/out.txt"; then
    echo "ok: the tree as it is builds"
else
    echo "FAIL: the tree as it is does not plan a build: $(grep -m1 '\*\*\*' "$T/out.txt")"; fail=1
fi
exit $fail
