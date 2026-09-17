#!/bin/bash
# Regression test: startdemos runs QDS1 client snapshots without simulation,
# draws their entity state, advances at EOF, and releases each old map.
set -uo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/.." && pwd)"
OUT="${1:?usage: test-title-demos.sh <build-dir>}"
RUN="${RUN_SH:-$HERE/run.sh}"
LOG="$OUT/cstep.txt"

cp "$ROOT/data/maps/e1m3/e1m3.qmp" "$OUT/"
cp "$ROOT/data/stuff.ini" "$OUT/"
python3 "$ROOT/tools/mkidcam.py" "${PAK:-$HOME/dos/QUAKE_SW/ID1/PAK0.PAK}" demo1.dem "$OUT/title.qdc"
python3 - "$OUT/title.qdc" "$OUT/titlea.qdc" "$OUT/titleb.qdc" <<'PY'
from pathlib import Path
import struct
import sys

source = Path(sys.argv[1]).read_bytes()
frame = struct.Struct("<6fl12h")
entity = struct.Struct("<5h4f")
records = []
offset = 13
while offset < len(source):
    values = frame.unpack_from(source, offset)
    size = frame.size + values[-1] * entity.size
    records.append(bytearray(source[offset:offset + size]))
    offset += size
struct.pack_into("<f", records[0], 0, 0.0)
struct.pack_into("<f", records[11], 0, 0.02)
data = source[:13] + records[0] + records[11]
Path(sys.argv[2]).write_bytes(data)
Path(sys.argv[3]).write_bytes(data)
PY

TIMEOUT="${TIMEOUT:-120}" "$RUN" "$OUT" \
    "e1m3.qmp +startdemos titlea.qdc titleb.qdc;wait;wait;wait;wait;wait;wait;quit" >/dev/null 2>&1

demos=$(tr -d '\r' < "$LOG" | sed -n 's/^attract demo \([^ ]*\) map .*/\1/p' | head -2 | paste -sd ' ' -)
[[ "$demos" == "titlea.qdc titleb.qdc" ]] || { echo "FAIL: demo order ${demos:-none}" >&2; exit 1; }
! grep -q 'DIVERGED' "$LOG" || { grep 'DIVERGED' "$LOG" >&2; exit 1; }
! grep '^frames=' "$LOG" | grep -qv ' ticks=0 ' || { echo "FAIL: camera playback ran game simulation" >&2; exit 1; }
grep -q '^frames=.* ticks=0 pos=-733,110,-1603$' "$LOG" || { echo "FAIL: camera did not reach recorded endpoint" >&2; exit 1; }
grep -Eq '^frames=.* mdl=[1-9][0-9]* ticks=0 ' "$LOG" || { echo "FAIL: snapshot monsters or view weapon were not drawn" >&2; exit 1; }
releases=$(grep -c '^map_release ok ' "$LOG")
[[ "$releases" -ge 3 ]] || { echo "FAIL: $releases map releases, want at least 3" >&2; exit 1; }

TIMEOUT="${TIMEOUT:-120}" "$RUN" "$OUT" \
    "e1m3.qmp +startdemos title.qdc;wait;stopdemo;wait;quit" >/dev/null 2>&1
[[ ! -s "$OUT/error.log" ]] || { cat "$OUT/error.log" >&2; exit 1; }
grep -q '^restored$' "$LOG" || { echo "FAIL: stopdemo did not shut down cleanly" >&2; exit 1; }
stop_releases=$(grep -c '^map_release ok ' "$LOG")
[[ "$stop_releases" -ge 2 ]] || { echo "FAIL: stopdemo resumed the snapshot-mutated map" >&2; exit 1; }
echo "ok: snapshot titlea -> titleb, recorded models, no game ticks, $releases clean map releases"
