# qcport

Quake for real-mode DOS, in C and assembly: id's episode 1 maps with
lightmaps, light styles, dynamic lights, a two-layer sky, alias models,
the monsters' AI, doors, triggers, weapons, sound and the status bar.
It runs on a 16-bit 386-class machine with EMS, 320x200 or 160x100.

It grew out of qb-qrender, a renderer in Microsoft BASIC; that code is
in this repository's history up to `chore: drop the BASIC renderer`.

## Layout

| | |
|---|---|
| `src/` | the program: `qmain.c`, the BSP walk, surfaces, entities, AI, sound |
| `qgl/` | the graphics, memory, file, timer and input layer, in asm |
| `qgl/test/` | qgl's own suite, one test per file, run natively |
| `tools/` | the build scripts, the asset pipeline and the `test-*.sh` gates |
| `data/` | `stuff.ini`, `base.dat` and the dm3ish test map |
| `docs/` | memory layout, portal culling, open bugs |

## Building

It needs Borland C 2.01 and jwasm/jwlink from `$TOOLCHAINS`
(`~/work/other/d32x/toolchains`), DOSBox-X, Python 3.12 with numpy and
Pillow, and Quake's shareware `PAK0.PAK` at `$PAK`
(`~/dos/QUAKE_SW/ID1/PAK0.PAK`), which is not redistributable.

```bash
make assets maps
```

```bash
make BUILD=$PWD/build/cport -j8
```

```bash
make test
```

`assets` builds dm3ish and the models, sounds and status bar; `maps`
builds `data/maps/<map>/<map>.qmp` for e1m1..e1m8 and start. Every
compile runs in its own DOSBox-X, so `-j8` is safe inside one make;
two makes must not share a `BUILD`.

## Running

```bash
tools/run.sh build/cport "e1m1.qmp -lm -ticks 600"
```

`tools/run.sh` runs headless and leaves `cstep.txt`, `BENCH.BMP` and
`BENCH.TXT` in the build directory; `tools/viz.sh` opens a window. Copy
the map's `.qmp` beside `QCPORT.EXE` first. A level exit writes
`NEXT.BAT`, which the run script calls to load the next map.

W/A/S/D move, the mouse looks, space jumps, 1-6 pick a weapon, F5 takes
a screenshot, F12 shows the stats.

See `CLAUDE.md` for how this project is worked on and what each
subsystem learned the hard way.
