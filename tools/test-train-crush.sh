#!/bin/bash
# Regression test: e1m3's 75-point ceiling train crushes instead of passing through the player.
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/.." && pwd)"
OUT="$(cd "${1:?usage: test-train-crush.sh <build-dir>}" && pwd)"
WORK="$(mktemp -d "$OUT/train-crush-test.XXXXXX")"
trap 'rm -rf "$WORK"' EXIT

train_state() {
  python3 - "$@" <<'PY'
import struct
import sys

mode, path = sys.argv[1:3]
data = bytearray(open(path, "rb").read())
want_time = float(sys.argv[3])
want_damage = int(sys.argv[4])
hits = []
for off in range(len(data) - 52):
    model, kind, damage = struct.unpack_from("<h34xh12xh", data, off)
    speed = struct.unpack_from("<f", data, off + 6)[0]
    state = struct.unpack_from("<h", data, off + 10)[0]
    if 0 < model < 256 and kind == 1 and damage == 75 and 0 < speed < 1000 and 2 <= state <= 4:
        hits.append(off)
if mode == "patch":
    if not hits:
        raise SystemExit("could not locate e1m3's 75-point train in save")
    struct.pack_into("<h", data, 4, int(sys.argv[5]))
    for off in hits:
        struct.pack_into("<f", data, off + 2, want_time)
        struct.pack_into("<h", data, off + 50, want_damage)
    open(path, "wb").write(data)
elif mode == "verify":
    values = [struct.unpack_from("<f", data, off + 2)[0] for off in hits]
    if not values or want_damage != 75 or any(value != want_time for value in values):
        raise SystemExit(f"train state did not round-trip: {values}")
else:
    raise SystemExit(f"unknown train-state operation: {mode}")
PY
}

require_load() {
  grep -q '^savegame loaded s0 ' "$WORK/cstep.txt" || {
    grep '^savegame ' "$WORK/cstep.txt" >&2 || true
    echo "train-state save was not loaded" >&2
    exit 1
  }
  ! grep -q '^savegame load failed' "$WORK/cstep.txt"
}

cp "$OUT/QCPORT.EXE" "$OUT/stuff.ini" "$OUT/default.cfg" \
  "$ROOT/data/maps/e1m3/e1m3.qmp" "$WORK/"
TIMEOUT="${TIMEOUT:-120}" "$HERE/run.sh" "$WORK" \
  "e1m3.qmp -noai -nosound -nostats -at -708 -1242 88 -yaw 0 -ticks 780" >/dev/null

grep -q '^fight=health -50 ' "$WORK/cstep.txt" || {
  grep '^fight=' "$WORK/cstep.txt" >&2 || true
  echo "e1m3's 75-point train did not crush twice at its stock half-second cadence" >&2
  exit 1
}

TIMEOUT="${TIMEOUT:-120}" "$HERE/run.sh" "$WORK" \
    "e1m3.qmp -noai -nosound -nostats -at -708 -1242 88 -ticks 1 +save s0;toggleconsole;quit" >/dev/null
train_state patch "$WORK/s0.sav" 123.5 75 9
TIMEOUT="${TIMEOUT:-120}" "$HERE/run.sh" "$WORK" \
    "e1m3.qmp -noai -nosound -nostats -ticks 1 +load s0;save s1;toggleconsole;quit" >/dev/null
require_load
train_state verify "$WORK/s1.sav" 123.5 75
train_state patch "$WORK/s0.sav" 123.5 0 8
rm -f "$WORK/s1.sav"
TIMEOUT="${TIMEOUT:-120}" "$HERE/run.sh" "$WORK" \
    "e1m3.qmp -noai -nosound -nostats -ticks 1 +load s0;save s1;toggleconsole;quit" >/dev/null
require_load
train_state verify "$WORK/s1.sav" 0 75

echo "ok: e1m3 train crushed for 75, v9 state round-tripped, and v8 state migrated"
