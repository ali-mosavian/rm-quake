#!/bin/bash
# Stage qvm_test for Borland C++ 3.1 -mm under DOSBox-X, and print the conf.
#
#   qvm/test/dosconf.sh [build dir]      then dosbox_launch the conf it prints
#
# The run compiles with -w into cct.txt, which must hold no Warning, then
# runs qvm_test on id's progs.dat and its qchash.py rewrite through INT 67h.
# Every line also goes out port E9 into qvmt.log beside the conf: the run
# passed when that log says "qvm_test: PASSED".
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
B="${1:-$ROOT/build/qvm-test}"
TOOLCHAINS="${TOOLCHAINS:-$HOME/work/other/d32x/toolchains}"
D="$B/dos"

make --no-print-directory -C "$HERE" B="$B" "$B/PROGS.DAT" "$B/PROGSH.DAT" >&2
rm -rf "$D"
mkdir -p "$D"
cp "$ROOT"/qvm/qvm.c "$ROOT"/qvm/qvm.h "$ROOT"/qvm/qtypes.h "$D"/
cp "$HERE"/qvm_test.c "$HERE"/qimg.c "$HERE"/qimg.h "$HERE"/ems.h "$HERE"/ems_dos.c "$D"/
cp "$B/PROGS.DAT" "$B/PROGSH.DAT" "$D"/

cat > "$D/qvmt.conf" <<EOF
[sdl]
autolock=false
[dosbox]
memsize=32
startbanner=false
quit warning=false
bochs debug port e9=true
[log]
logfile=$D/qvmt.log
[cpu]
core=normal
cycles=max
[dos]
xms=true
ems=true
[autoexec]
mount w $D
mount b $TOOLCHAINS/bcpp31
path b:\\bin
w:
bcc -3 -f87 -mm -Ox -w -I. -IB:\\INCLUDE -LB:\\LIB qvm_test.c qvm.c qimg.c ems_dos.c > cct.txt
type cct.txt
qvm_test.exe PROGS.DAT PROGSH.DAT
echo ==== qvmt done
EOF
echo "$D/qvmt.conf"
