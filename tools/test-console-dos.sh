#!/bin/bash
# Console cvars must affect the engine, survive a map command, and an open
# console must pause simulation without stopping frames or accumulating ticks.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
B="$(cd "${1:?usage: test-console-dos.sh <build-dir>}" && pwd)"

pause="$B/console-pause"
rm -rf "$pause"; mkdir -p "$pause"
cp "$B/QCPORT.EXE" "$B/stuff.ini" "$ROOT/data/assets/dm3ish.qmp" "$pause/"
TIMEOUT="${TIMEOUT:-120}" "$ROOT/tools/run.sh" "$pause" \
  "dm3ish.qmp -nosound -noai -ticks 5 +toggleconsole;wait;wait;toggleconsole" >/dev/null
grep -q '^console paused=2$' "$pause/cstep.txt" || {
    echo "FAIL: console did not pause exactly two rendered frames"; exit 1;
}

cycle="$B/console-cycle"
rm -rf "$cycle"; mkdir -p "$cycle"
cp "$B/QCPORT.EXE" "$B/stuff.ini" "$ROOT/data/maps/e1m1/e1m1.qmp" \
   "$ROOT/data/maps/start/start.qmp" "$cycle/"
TIMEOUT="${TIMEOUT:-240}" "$ROOT/tools/run.sh" "$cycle" \
  "e1m1.qmp -nosound -noai -sflags 1 -ticks 3 +fov 110 +skill 2 +wait +map start.qmp" >/dev/null
[[ $(grep '^mod_load_world ok' "$cycle/cstep.txt" | head -1) == *"mon=42"* ]] || {
    echo "FAIL: startup +skill did not select the first map's hard entities"; exit 1;
}
[[ $(grep -c '^cvars skill=2 fov=110$' "$cycle/cstep.txt") -eq 2 ]] || {
    echo "FAIL: cvars did not survive map command"; exit 1;
}
[[ $(grep -c '^map_release ok ' "$cycle/cstep.txt") -eq 2 ]] || {
    echo "FAIL: map lifecycle baseline was not restored"; exit 1;
}
grep -q '^asset_map ok start.qmp$' "$cycle/cstep.txt" || {
    echo "FAIL: map start.qmp was truncated"; exit 1;
}
[[ $(grep '^sflags ' "$cycle/cstep.txt" | tr '\n' ' ') == "sflags 1 sflags 0 " ]] || {
    echo "FAIL: map command carried serverflags into a fresh game"; exit 1;
}

quit="$B/console-quit"
rm -rf "$quit"; mkdir -p "$quit"
cp "$B/QCPORT.EXE" "$B/stuff.ini" "$ROOT/data/assets/dm3ish.qmp" "$quit/"
TIMEOUT="${TIMEOUT:-120}" "$ROOT/tools/run.sh" "$quit" \
  "dm3ish.qmp -nosound -noai +wait;wait;wait;toggleconsole;quit" >/dev/null
grep -q '^frames=3 ' "$quit/cstep.txt" || {
    echo "FAIL: quit command advanced or rendered one old-world frame"; exit 1;
}

fov="$B/console-fov"
rm -rf "$fov"; mkdir -p "$fov"
cp "$B/QCPORT.EXE" "$ROOT/data/assets/dm3ish.qmp" "$fov/"
sed 's/world.camera.fov = 73.74/world.camera.fov = 60/' "$B/stuff.ini" > "$fov/stuff.ini"
TIMEOUT="${TIMEOUT:-120}" "$ROOT/tools/run.sh" "$fov" \
  "dm3ish.qmp -nosound -noai -ticks 1" >/dev/null
grep -q '^cvars skill=0 fov=75$' "$fov/cstep.txt" || {
    echo "FAIL: console fov did not inherit world.camera.fov"; exit 1;
}

precedence="$B/console-quit-precedence"
rm -rf "$precedence"; mkdir -p "$precedence"
cp "$B/QCPORT.EXE" "$B/stuff.ini" "$ROOT/data/assets/dm3ish.qmp" "$precedence/"
TIMEOUT="${TIMEOUT:-120}" "$ROOT/tools/run.sh" "$precedence" \
  "dm3ish.qmp -nosound -noai +toggleconsole;quit +map missing" >/dev/null
grep -q '^frames=0 ' "$precedence/cstep.txt" || {
    echo "FAIL: command dispatch continued after quit"; exit 1;
}
grep -q '^gs_state 1 map dm3ish.qmp next  ' "$precedence/cstep.txt" || {
    echo "FAIL: map command executed after quit"; exit 1;
}

many="$B/console-many-startup"
rm -rf "$many"; mkdir -p "$many"
cp "$B/QCPORT.EXE" "$B/stuff.ini" "$ROOT/data/assets/dm3ish.qmp" "$many/"
TIMEOUT="${TIMEOUT:-120}" "$ROOT/tools/run.sh" "$many" \
  "dm3ish.qmp -nosound -noai +noclip +noclip +noclip +noclip +noclip +noclip +noclip +noclip +toggleconsole;quit" >/dev/null
tr -d '\r' < "$many/ERROR.LOG" | grep -qi '^startup command queue overflow$' || {
    echo "FAIL: deferred startup overflow was silent"; exit 1;
}
echo "ok: console pause, cvar consumers and map persistence"
