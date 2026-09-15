#!/bin/bash
# Regression test: no sprintf may overrun its char array, and tools/bufcheck.py must see one.
#
# The old main wrote its run summary into buf[96]: the fight= line needs
# 145 bytes at 16-bit widths, and the models line appended a kind at a
# time into the same 96. Nothing failed; the stack under the frame took
# it. The second arm is that source, from 5f8c8a1: a bufcheck that stops
# reporting its fight= line is blind.
#
# The third is mdl.c from 8d556a8, whose refusals went into msg[64] with a
# char[24] path in %s: boss.geo's "will not fit" wrote 65. bufcheck read
# only the first array of `char path[24], msg[64];` and bounded no %s, so
# it passed that file with nothing unchecked it could name.
#
# The fourth is ent.c's ent_ents_name, which sprintfs into its parameter
# `name`, under a file-scope `char name[64]` added for the arm: a parameter
# is a pointer whatever an outer array of its name holds, and bufcheck
# judged it against the array. The fifth is the same under d_alias.c's
# mdl_draw_tris, whose signature ends in a comment: bufcheck looked for a
# `)` before the `{`, found the comment, and pushed no parameters.
#
# Last, every sprintf bufcheck cannot bound must be in
# tools/ref/bufcheck-unchecked.txt with its reason, one line a call, and
# every entry there must still exist: a new unbounded sprintf needs a bound
# or a stated reason. The list was a set, so one line covered any number of
# identical calls.
#
#   tools/test-bufcheck.sh
set -uo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
ALLOW="${BUFCHECK_ALLOW:-$ROOT/tools/ref/bufcheck-unchecked.txt}"
fail=0

# the program's modules as make builds them, so a new subsystem is checked too, and the VM's
c_src=$(make -C "$ROOT" -p -n "$ROOT/build/bufcheck-none/QCPORT.EXE" 2>/dev/null | sed -n 's/^C_SRC := //p' | head -1)
[[ -n "$c_src" ]] || { echo "FAIL: cannot read C_SRC from make" >&2; exit 1; }
out=$(cd "$ROOT" && python3 tools/bufcheck.py $c_src src/qvm/*.c)
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

old="$(mktemp -d)/mdl.c"
git -C "$ROOT" show 8d556a8:src/mdl.c > "$old"
oldout=$(python3 "$ROOT/tools/bufcheck.py" "$old")
if ! grep -q '^OVERRUN .*%d verts %d tris.*needs [0-9]* of msg\[64\]' <<< "$oldout"; then
    echo "FAIL: bufcheck no longer sees 8d556a8's model refusal overrun msg[64]" >&2
    fail=1
fi
rm -rf "$(dirname "$old")"

fix="$(mktemp -d)/shadow.c"
python3 - "$ROOT/src/game/ent.c" "$fix" <<'PY'
import re
import sys

src, dst = sys.argv[1:3]
t = open(src, encoding="latin-1").read()
a = re.search(r"^static void near ent_ents_name\(.*?\n\}\n", t, re.M | re.S)
open(dst, "w").write("static char name[64];\n\n" + a[0])
PY
if ! python3 "$ROOT/tools/bufcheck.py" "$fix" | grep -q '^unchecked .*sprintf( name, "ents%d.bin"'; then
    echo "FAIL: bufcheck judged ent_ents_name's parameter against a file-scope name[64]" >&2
    fail=1
fi
rm -rf "$(dirname "$fix")"

fix="$(mktemp -d)/tailcmt.c"
python3 - "$ROOT/src/render/d_alias.c" "$fix" <<'PY'
import re
import sys

src, dst = sys.argv[1:3]
t = open(src, encoding="latin-1").read()
a = re.search(r"^static short mdl_draw_tris\(.*?\*/\n\{\n", t, re.M | re.S)
open(dst, "w").write("static char dst[64];\n\n" + a[0] + "    sprintf( dst, \"%s\", \"x\" );\n}\n")
PY
if ! python3 "$ROOT/tools/bufcheck.py" "$fix" | grep -q '^unchecked .*sprintf( dst, "%s"'; then
    echo "FAIL: a comment after mdl_draw_tris's ) hid its parameter dst behind a file-scope dst[64]" >&2
    fail=1
fi
rm -rf "$(dirname "$fix")"

allow_gate() {
    python3 - "$1" "$2" <<'PY'
import os
import re
import sys
from collections import Counter

out, allow_path = sys.argv[1:3]
seen = Counter(f"{os.path.basename(m[1])} | {m[2]}"
               for line in out.splitlines() if line.startswith("unchecked ")
               if (m := re.match(r"unchecked (.+?):\d+: (.*) -- [^-]*$", line)))
allowed = Counter(line.rstrip("\n").rsplit(" | ", 1)[0]
                  for line in open(allow_path) if line.strip() and not line.startswith("#"))
name = os.path.basename(allow_path)
bad = [f"FAIL: unchecked, and not in {name}: {k}" for k in sorted((seen - allowed).elements())]
bad += [f"FAIL: in {name}, and no longer unchecked: {k}" for k in sorted((allowed - seen).elements())]
print("\n".join(bad), file=sys.stderr, end="\n" if bad else "")
sys.exit(1 if bad else 0)
PY
}

# two unchecked calls with one key are two entries: one line must not cover both
dup="$(mktemp -d)"
printf 'void f( char *p, char *s )\n{\n    sprintf( p, "%%s", s );\n    sprintf( p, "%%s", s );\n}\n' > "$dup/dup.c"
printf 'dup.c | sprintf( p, "%%s" | a pointer\n' > "$dup/allow.txt"
if allow_gate "$(python3 "$ROOT/tools/bufcheck.py" "$dup/dup.c")" "$dup/allow.txt" 2>/dev/null; then
    echo "FAIL: one allowlist line covered two unchecked sprintfs with the same key" >&2
    fail=1
fi
rm -rf "$dup"

allow_gate "$out" "$ALLOW" || fail=1

[[ $fail == 0 ]] && echo "ok: $(tail -1 <<< "$out")"
exit $fail
