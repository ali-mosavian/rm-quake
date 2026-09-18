#!/bin/bash
# Regression test: the pickups draw, and standing on one takes it.
#
# Two arms, because one assertion cannot fail both ways. The frame
# against -noitems says a box reached the screen at all -- an item
# loaded and never drawn, or drawn behind the world by a depth test
# that rejects everything, both read 0 here. The kit line says the
# touch happened: 26 items load on e1m1 and the one at (672,-40) is
# 20 shells, so the player starts with 25 and must end with 45.
#
#   tools/test-items.sh build/cport-e1m1
#
# The map must be staged (e1m1.qmp, which is all of it); the test
# SKIPs rather than fails when it is not.
set -uo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
OUT="${1:?usage: test-items.sh <build-dir>}"
MIN="${MIN:-100}"

[[ -f "$OUT/e1m1.qmp" ]] || { echo "SKIP: no e1m1.qmp in $OUT"; exit 0; }

run() { TIMEOUT="${TIMEOUT:-200}" "${RUN_SH:-$HERE/run.sh}" "$OUT" "$@" >/dev/null 2>&1; }

# The megahealth at (944,1008,-272), seen from 108 units short of it.
AT="-at 944 900 -240 -yaw 270 -ticks 4"
run "e1m1.qmp -nostats $AT"          || true
cp "$OUT/BENCH.BMP" "$OUT/items-on.bmp" 2>/dev/null || { echo "FAIL: no frame written" >&2; exit 1; }
run "e1m1.qmp -nostats -noitems $AT" || true

diff_px=$(python3 - "$OUT/items-on.bmp" "$OUT/BENCH.BMP" <<'PY'
import sys
def px(p):
    d = open(p, "rb").read()
    return d[int.from_bytes(d[10:14], "little"):]
a, b = px(sys.argv[1]), px(sys.argv[2])
print(sum(1 for x, y in zip(a, b) if x != y))
PY
)

fail=0
if [[ ${diff_px:-0} -lt $MIN ]]; then
    echo "FAIL: the pickups put $diff_px pixels on screen, wanted at least $MIN" >&2
    fail=1
else
    echo "ok: the pickups draw -- $diff_px pixels against -noitems"
fi

# Standing on item 21, 20 shells at (672,-40): 25 + 20, and one gone.
run "e1m1.qmp -nostats -at 672 -40 80 -ticks 30" || true
kit=$(tr -d '\r' < "$OUT/cstep.txt" 2>/dev/null | grep -m1 '^fight=')
shells=$(sed -n 's/.*shells \([0-9]*\).*/\1/p' <<< "$kit")
took=$(sed -n 's/.*took \([0-9]*\).*/\1/p' <<< "$kit")

if [[ "${shells:-0}" -ne 45 || "${took:-0}" -lt 1 ]]; then
    echo "FAIL: standing on the 20-shell box gave [$kit], wanted shells 45 and took 1" >&2
    fail=1
else
    echo "ok: the shells were taken -- $kit"
fi

# item_info's rows are indexed by kind; a row out of order hands a pickup another kind's take.
if ! tr -d '\r' < "$OUT/cstep.txt" 2>/dev/null | grep -q '^item_selftest 1$'; then
    echo "FAIL: no 'item_selftest 1' in cstep.txt" >&2
    fail=1
else
    echo "ok: item_selftest 1"
fi

# armor_touch: e1m1's green (100 at 0.3, 688 480 80) and yellow (150 at 0.6, 1312 1048 -432).
# Green then yellow takes both; yellow then green refuses the green, 30 against 90.
# The yellow stands over slime, whose bites eat armor: arm 1 leaves at tick 40, before the first.
for arm in "688 480 80 1312 1048 -432 2 -warp 40 688 480 104" "1312 1048 -432 688 480 80 1"; do
    read -r ax ay az bx by bz want out <<< "$arm"
    run "e1m1.qmp -nostats -noai -nosound -at $ax $ay $az -warp 30 $bx $by $bz ${out:-} -ticks 60" || true
    kit=$(tr -d '\r' < "$OUT/cstep.txt" 2>/dev/null | grep -m1 '^fight=')
    armor=$(sed -n 's/.*armor \([0-9]*\).*/\1/p' <<< "$kit")
    took=$(sed -n 's/.*took \([0-9]*\).*/\1/p' <<< "$kit")
    if [[ "${armor:-0}" -ne 150 || "${took:-0}" -ne "$want" ]]; then
        echo "FAIL: armor at $ax,$ay then $bx,$by gave [$kit], wanted armor 150 and took $want" >&2
        fail=1
    else
        echo "ok: armor at $ax,$ay then $bx,$by -- armor 150, took $want"
    fi
done

# item_armorInv: dm3ish's red armor, 200 at 0.8. mkassets once dropped the entity, so armor read 0.
# Then the yellow at 448,-592 is refused, 90 against 160.
if [[ -f "$OUT/dm3ish.qmp" ]]; then
    for arm in "-ticks 30|200|took 1" "-warp 30 448 -592 120 -ticks 60|200|took 1"; do
        IFS='|' read -r extra want what <<< "$arm"
        run "dm3ish.qmp -nostats -noai -nosound -at -272 -560 280 $extra" || true
        kit=$(tr -d '\r' < "$OUT/cstep.txt" 2>/dev/null | grep -m1 '^fight=')
        armor=$(sed -n 's/.*armor \([0-9]*\).*/\1/p' <<< "$kit")
        took=$(sed -n 's/.*took \([0-9]*\).*/\1/p' <<< "$kit")
        if [[ "${armor:-0}" -ne "$want" || "${took:-0}" -ne 1 ]]; then
            echo "FAIL: red armor, $extra gave [$kit], wanted armor $want and $what" >&2
            fail=1
        else
            echo "ok: red armor, $extra -- armor $want, $what"
        fi
    done
    # item_cells: 6, or 12 with WEAPON_BIG2. mkassets once dropped the entity, so cells read 0.
    # The pool box drops to the floor at -128; from -104 the player is in reach at once.
    for arm in "395 -227 -104|6" "-773 -11 160|12"; do
        IFS='|' read -r at want <<< "$arm"
        run "dm3ish.qmp -nostats -noai -nosound -at $at -ticks 30" || true
        kit=$(tr -d '\r' < "$OUT/cstep.txt" 2>/dev/null | grep -m1 '^fight=')
        cells=$(sed -n 's/.* cells \([0-9]*\).*/\1/p' <<< "$kit")
        if [[ "${cells:-0}" -ne "$want" ]]; then
            echo "FAIL: cells at $at gave [$kit], wanted cells $want" >&2
            fail=1
        else
            echo "ok: cells at $at -- $want"
        fi
    done

    # Ammo boxes use items.qc's asymmetric 0..32 trigger bounds. The player
    # at x=435 overlaps the box at x=395 by eight units; treating the map
    # origin as the box centre leaves a false 8-unit gap and misses it.
    run "dm3ish.qmp -nostats -noai -nosound -at 435 -227 -104 -ticks 2" || true
    kit=$(tr -d '\r' < "$OUT/cstep.txt" 2>/dev/null | grep -m1 '^fight=')
    cells=$(sed -n 's/.* cells \([0-9]*\).*/\1/p' <<< "$kit")
    if [[ "${cells:-0}" -ne 6 ]]; then
        echo "FAIL: overlapping the positive edge of a cell box gave [$kit], wanted 6 cells" >&2
        fail=1
    else
        echo "ok: the asymmetric cell-box trigger takes at its positive edge"
    fi

    # ammo_touch only changes weapons when the current weapon was the old
    # W_BestWeapon. With an owned SNG and no nails, the shotgun is best;
    # taking nails promotes the SNG. A deliberately selected axe stays put.
    for arm in "1|64|SNG selected" "4096|4096|axe retained"; do
        IFS='|' read -r held want what <<< "$arm"
        python3 - "$OUT/CARRY.BIN" "$held" <<'PY'
import struct
import sys
open(sys.argv[1], "wb").write(struct.pack(
    "<lhhfhhhhhhh", 1 | 64 | 4096, 100, 0, 0.0, 25, 0, 0,
    int(sys.argv[2]), 0, 0, 0
))
PY
        KEEP_CARRY=1 run "dm3ish.qmp -nostats -noai -nosound -carry -at -452 -580 280 -ticks 2" || true
        kit=$(tr -d '\r' < "$OUT/cstep.txt" 2>/dev/null | grep -m1 '^fight=')
        weapon=$(sed -n 's/.* weapon \([0-9]*\).*/\1/p' <<< "$kit")
        if [[ "${weapon:-0}" -ne "$want" ]]; then
            echo "FAIL: nail pickup with weapon $held gave [$kit], wanted $what" >&2
            fail=1
        else
            echo "ok: nail pickup -- $what"
        fi
    done
else
    echo "SKIP: no dm3ish.qmp in $OUT"
fi

# item_artifact_invisibility: e1m3's ring, then a warp beside the ogre. FindTarget refuses an
# invisible player, so nothing hunts; mkassets once dropped the ring, and the ogre hunted.
# The control starts 64 units south, off the ring (-noitems only hides it; it is still taken).
if [[ -f "$OUT/e1m3.qmp" ]]; then
    E1M3="e1m3.qmp -nostats -nosound"
    for arm in "-1736 -552 -336|0|ring" "-1736 -616 -336|1|no ring"; do
        IFS='|' read -r at want what <<< "$arm"
        run "$E1M3 -at $at -warp 30 -1336 -256 -72 -ticks 300" || true
        hunting=$(tr -d '\r' < "$OUT/cstep.txt" 2>/dev/null | sed -n 's/^ai hunting=\([0-9]*\).*/\1/p' | head -1)
        took=$(tr -d '\r' < "$OUT/cstep.txt" 2>/dev/null | grep -m1 '^fight=' | sed -n 's/.*took \([0-9]*\).*/\1/p')
        if [[ -z "$hunting" || "${took:--1}" -ne $(( 1 - want )) || ( $want -eq 0 && $hunting -ne 0 ) || ( $want -eq 1 && $hunting -eq 0 ) ]]; then
            echo "FAIL: $what, beside the ogre: hunting=${hunting:-none} took=${took:-none}" >&2
            fail=1
        else
            echo "ok: $what, beside the ogre -- hunting=$hunting"
        fi
    done
    # R_DrawViewModel draws no gun while invisible: with the ring the frame equals -noview's,
    # and without it they differ, or the gun was never on screen and the equality proves nothing.
    for arm in "-1736 -552 -336|same|no gun while invisible" "-1736 -616 -336|differ|the gun draws without the ring"; do
        IFS='|' read -r at want what <<< "$arm"
        rm -f "$OUT/ring-view.bmp"
        run "$E1M3 -noai -at $at -ticks 60" || true
        cp "$OUT/BENCH.BMP" "$OUT/ring-view.bmp" 2>/dev/null
        run "$E1M3 -noai -noview -at $at -ticks 60" || true
        if [[ ! -f "$OUT/ring-view.bmp" || ! -f "$OUT/BENCH.BMP" ]]; then
            echo "FAIL: $what -- a run wrote no frame" >&2
            fail=1
            continue
        fi
        cmp -s "$OUT/ring-view.bmp" "$OUT/BENCH.BMP" && got=same || got=differ
        if [[ $got != "$want" ]]; then
            echo "FAIL: $what -- the frame and -noview's are $got" >&2
            fail=1
        else
            echo "ok: $what"
        fi
    done
else
    echo "SKIP: no e1m3.qmp in $OUT"
fi

exit $fail
