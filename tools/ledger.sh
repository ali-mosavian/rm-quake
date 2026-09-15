#!/bin/bash
# ledger.sh -- what a build costs: code, near memory, far memory, frame time.
#
#   tools/ledger.sh <build-dir> <label>        appends to docs/ledger.tsv
#
# Rows are `label commit scope key value`. scope is "build", a module's
# segment name, or a map. Compare two labels with
#   awk -F'\t' '$1=="a"||$1=="b"' docs/ledger.tsv
#
# Code is the MAP's segment sizes, not the EXE's bytes: the EXE carries
# CodeView symbols, which grow with every function a split adds.
# Timings run on CORE=normal, one map a directory, in parallel; one run
# a map is a ledger row, not a measurement -- a delta past ft_mean's
# 2.3 ms spread still wants six interleaved runs an arm.
set -uo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
B="$(cd "${1:?usage: ledger.sh <build-dir> <label>}" && pwd)"
LABEL="${2:?usage: ledger.sh <build-dir> <label>}"
MAPS="${MAPS:-e1m1 e1m3 e1m4 e1m7}"
TICKS="${TICKS:-300}"
LEDGER="$ROOT/docs/ledger.tsv"
MAP="$B/QCPORT.MAP"
[[ -s "$MAP" ]] || { echo "ledger: no $MAP" >&2; exit 1; }

commit="$(git -C "$ROOT" rev-parse --short HEAD)$(git -C "$ROOT" diff --quiet HEAD -- src ':(exclude)src/qvm' ':(exclude)src/qgl/test' || echo +)"
rows=()
row() { rows+=("$LABEL	$commit	$1	$2	$3"); }

# segments: name class ... size (hex, last column)
c_mods=" $(make -C "$ROOT" -p -n "$ROOT/build/ledger-none/QCPORT.EXE" 2>/dev/null | sed -n 's/^C_MODS := //p' | head -1 | tr 'a-z' 'A-Z') "
[[ -n "${c_mods// }" ]] || { echo "ledger: cannot read the C modules from make" >&2; exit 1; }
code_c=0; code_qgl=0; far_data=0
while read -r name class size; do
    n=$((16#$size))
    case "$class" in
        CODE)
            row "$name" size "$n"
            if [[ "$c_mods" == *" ${name%_TEXT} "* ]]; then code_c=$((code_c + n)); else code_qgl=$((code_qgl + n)); fi ;;
        FAR_DATA) far_data=$((far_data + n)) ;;
    esac
done < <(awk '$2=="CODE"||$2=="FAR_DATA"{print $1, $2, $NF}' "$MAP")
row build code_c "$code_c"
row build code_qgl "$code_qgl"
row build far_data "$far_data"
row build dgroup "$((16#$(awk '$1=="DGROUP"{print $3; exit}' "$MAP")))"
stk=$(sed -n 's/^unsigned _stklen = \([0-9]*\).*/\1/p' "$ROOT/src/host/qmain.c")
heap=$("$ROOT/tools/dgroup-check.sh" "$MAP" "$stk" 2>&1 | sed -n 's/.*stack [0-9]*, \([0-9]*\) bytes of near heap.*/\1/p')
row build near_heap "${heap:-?}"

run_map() {   # map -> its own directory, so the runs cannot share a BENCH.TXT
    local m=$1 d="$B/ledger-$1"
    rm -rf "$d"; mkdir -p "$d"
    cp "$B/QCPORT.EXE" "$B/stuff.ini" "$ROOT/data/maps/$m/$m.qmp" "$d/"
    CORE=normal TIMEOUT="${TIMEOUT:-600}" "$ROOT/tools/run.sh" "$d" \
        "$m.qmp -lm -nostats -noai -nosound -ticks $TICKS" > /dev/null 2>&1
}
for m in $MAPS; do run_map "$m" & done
wait

for m in $MAPS; do
    t="$B/ledger-$m/BENCH.TXT"
    [[ -s "$t" ]] || { row "$m" missing 1; continue; }
    for k in mem_far_largest ft_mean pt_frame_mean pt_draw_mean pt_alias_mean frames; do
        v=$(tr -d '\r' < "$t" | awk -v k="$k" '$1==k{print $2}')
        [[ -n "$v" ]] && row "$m" "$k" "$v"
    done
    v=$(tr -d '\r' < "$B/ledger-$m/cstep.txt" 2>/dev/null | awk '$1=="stack_untouched"{print $2}')
    [[ -n "$v" ]] && row "$m" stack_untouched "$v"
done

[[ -s "$LEDGER" ]] || printf 'label\tcommit\tscope\tkey\tvalue\n' > "$LEDGER"
printf '%s\n' "${rows[@]}" >> "$LEDGER"
printf '%s\n' "${rows[@]}" | awk -F'\t' '$3=="build" || $3 ~ /^e1m|^start|^dm3/' | cut -f3-
