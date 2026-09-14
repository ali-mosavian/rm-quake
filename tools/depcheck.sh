#!/usr/bin/env bash
# depcheck.sh -- an edit to a header must rebuild what includes it.
#
# qgl.inc carries the surface kinds and the struct layouts; src/*.h carry
# every struct the C modules share. Change one with the objects built and
# a rule missing the header has nothing to notice: the sources are
# untouched, the stale objects stay, the link is happy, and QCPORT.EXE
# runs old constants against new code. Nothing in the build says so.
#
# It builds no code. It fabricates a built tree, asserts make would
# compile nothing -- without that the test could pass for the wrong
# reason -- then makes each header newer and asserts every object of
# its kind would be rebuilt.
set -uo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT" || exit 1

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
OUT="$TMP/o"
mkdir -p "$OUT"

var () { make BUILD="$OUT" -p -n "$OUT/QCPORT.EXE" 2>/dev/null | sed -n "s/^$1 := //p" | head -1; }
c_mods=$(var C_MODS)
qgl_mods=$(var QGL_MODS)
asm_mods=$(var ASM_MODS)
[[ -n "$c_mods" && -n "$qgl_mods" ]] || { echo "depcheck: cannot read the module lists"; exit 1; }
nc=$(wc -w <<< "$c_mods"); nq=$(wc -w <<< "$qgl_mods")

built () {
    for m in $c_mods $qgl_mods $asm_mods; do : > "$OUT/$m.obj"; done
    : > "$OUT/QCPORT.EXE"
}
# the compiles make would run; qglh's generator check runs every time, by design
compiles () { make BUILD="$OUT" -n "$OUT/QCPORT.EXE" 2>/dev/null | grep -cE "$1"; }

fail=0
say () { printf '   %-4s %s\n' "$1" "$2"; }

built
n=$(compiles 'bcc\.sh|jwasm')
if (( n == 0 )); then say ok "a built tree compiles nothing"; else say FAIL "a built tree compiles $n objects"; fail=$((fail + 1)); fi

check () {   # header, pattern of its kind's compile, how many there are
    # make compares mtimes at one-second resolution: a header touched in
    # the same second as the objects reads as no newer
    sleep 1
    touch "$1"
    n=$(compiles "$2")
    if (( n == $3 )); then say ok "$1 rebuilds all $3"; else say FAIL "$1 rebuilds $n of $3"; fail=$((fail + 1)); fi
    sleep 1
    built
}

for h in qgl/*.inc; do check "$h" "jwasm .* -I$ROOT/qgl " "$nq"; done
for h in src/world.h src/qgl.h; do check "$h" 'bcc\.sh' "$nc"; done

if (( fail )); then echo "RESULT FAIL ($fail)"; exit 1; fi
echo "RESULT PASS"
