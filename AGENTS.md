# Notes for working on this

Hard-won things, mostly the kind that cost an hour before they cost a minute.

## Writing — precedes every other rule

**NO RAMBLING.** Write the minimum needed to understand. This takes precedence over every other rule here and in
any project's instructions: when another convention or habit would make the output longer, this
one wins. Length is never the price of following some other rule.

- **Chat replies**: answer the question asked, then stop. No findings dumps, no summary tables of
  work just done, no "a few more things worth knowing" sections. If you found more than was
  asked, give the headline and offer the rest.
- **Descriptions** (PR, Jira): `TL;DR` line first — outcome and headline number — then the short
  version of the detail.
- **Commit messages**: no `TL;DR` — the subject line is the summary. Keep the body to what the
  diff cannot show; link the ticket instead of restating it.
- **Docs and prose a person reads**: plain sentences, one idea per paragraph. No framing, no
  restating the same fact twice, no caveats section.
- **Code comments**: only for the non-trivial parts — hidden constraints, subtle invariants,
  non-obvious workarounds. Naming and structure carry the rest.
- Cut rhetorical flourishes. Name the thing, state the fact, move on.

## Measurements — the second rule

**Doubt the measurement before the subject.** A result that contradicts what
is known about the thing being measured is evidence the instrument is wrong,
not a finding. Never conclude "there is nothing here" from a number that
common sense says should be large.

- Several measures agreeing on zero is one broken assumption, not several facts.
- A question shaped so the subject's own style cannot answer it always returns
  zero. Ask what the subject actually does, not what the textbook name is.
- Verify against the raw thing -- read the assembly, the bytes, the output --
  before writing a conclusion down. One look beats any amount of analysis.
- Derive a target by hand on both sides. An estimated one flatters whatever
  it measures.
- Cost hidden behind a call, an interrupt or a helper reads as free unless the
  measure is told about it.
- Never explain an implausible result with a story. That is how it survives.

## Regression tests — the third rule

**Every issue found and fixed gets a regression test, in the same commit as
the fix.** Not "should" — the fix is not finished without it.

- **Write it so it fails first.** Revert the fix, watch the test fail, restore
  the fix. A test written after the fix and never seen to fail is evidence of
  nothing.
- **Test the symptom, not the patch.** The wrong output is the fact; which
  branch of which helper returned early is this week's shape of it. Assert on
  observable behaviour where you can.
- **Instruments count.** A broken measurement is an issue like any other and
  gets a test like any other — see the second rule. A scoreboard reading the
  wrong thing will not announce itself twice.
- **Say what it cost.** The test's name or docstring says what went wrong and
  what it produced. Cheapest to write while it is still understood.

## Rules

The short list. Where one of these has a long-form note further down --
most do, because most were learned the expensive way -- it points there
rather than saying it twice.

### Code: principles

These apply whether the file is `.bas` or `.py`.

**DRY: one fact in one place.** A value or a rule that is true twice is
declared once and read twice. **SRP: one module, one subsystem.** Listing
a file's procedure prefixes should collapse to one -- see **Layout** and
**Splitting a module**, where this is checked mechanically. **Open/closed:
a new idiom is a new case, not an edit to the cases already there.** A
`select case` gains a branch; the branches it already has do not change
shape to accommodate it. The ini parser is the worked example of where
this does NOT pay -- see **What was deliberately not done** -- so apply
it where it earns its keep, not everywhere reflexively.

### Code: the tools

`tools/*.py` are standalone scripts run directly -- there is no
`pyproject.toml`, no uv lock, no ruff config and no pytest suite today.
The rules below are for what gets written or touched from here:

- Python 3.13+, and write 3.13: `match`/`case` over `if`/`elif` chains,
  `StrEnum` for string-constant sets (a bare name in `case` captures
  everything, so constants must be dotted), `X | None`, type aliases,
  walrus where it shortens.
- Type annotations on every parameter and return. None of the current
  seventeen tools has one, so this is a rule about new code, not a claim
  about old.
- Functions over classes; a class earns its place only when behaviour and
  state travel together. Data is `@dataclass(slots=True)`, `frozen=True`
  unless it mutates.
- Functions are pure. No module-level code that does work.
- A comprehension where it fits: a `for`+`append` with no real branching
  is a comprehension not yet written as one.
- ruff / ruff-format / isort, line length 120, double quotes, one import
  per line.
- **No function docstrings, and no comments that restate the code** -- a
  comment must carry a fact the code does not. The MODULE docstring
  stays: in this repo it is the tool's usage text, and `imgdiff.py` is
  the model.
- If a Python test suite appears: pytest, no test classes, parametrize
  wherever one assertion runs over a set. The gates today are `make test`
  and `tools/test-*.sh`, not pytest.

### The gates

    make test                    the qgl suite, native; needs `make assets`
    tools/test-<name>.sh <build> one behaviour of QCPORT.EXE, headless

`make test` needs no Borland C, which is what makes it worth running
between edits: a failure there names an assertion instead of producing a
picture that came out wrong for reasons unknown. Each `test-*.sh` says in
its header what went wrong and what it produced; run the ones for the
subsystem a change touches, against a build directory of its own.

The notes below still name `tools/check.sh`, `tools/dosbox.sh` and
`tools/ref/*.bmp`: the BASIC renderer's gates, removed with it. What they
proved is history; `tools/test-*.sh` are the gates now.

### Writing

**Commit messages.** Conventional commits, `type(scope): description`,
plus a body. The subject says what changed; the body says what it was and
why. Both short. No `Co-Authored-By` -- the user is the author.

**Replies in session are held to the same rule, and it is the one most
often missed.** Say the thing and stop. The finding first, no preamble,
and no closing paragraph recommending next steps unless a decision is
actually open. PR bodies, docs and comments likewise.

**Report what was measured, not what was expected.** If six runs were
meant and three ran, say three.

### Working

**Watch DOSBox, do not wait for it -- and check often, not once.** A
launched run gets `dosbox_status`, a screenshot, the text screen and
`dosbox_where` checked repeatedly WHILE it is in flight, not started and
then read once at the end. A picture answers in one glance what a timeout
cannot answer at all: loading screen, black screen, error, or a frame
that rendered and stopped. This is the MUST rule -- see **Drive DOSBox
hands-on**. Every inspection freezes the CPU; resume with
`dosbox_continue` and confirm `cpuRunning` before reading anything from
the next check.

**Measure, do not reason, about cost.** DOSBox charges per instruction and
models no latency, so intuitions about what is expensive are worthless
here. Six runs per arm, interleaved, medians -- see **One run per side is
not a measurement**.

**Round trip before trusting a rewrite.** Read the thing, write it back,
compare bytes, and only then believe the transform. `-dumptex` reading
every atlas cell back through its own view is this rule, and it is what
proved the atlas right while the fault was elsewhere.

**Fixtures are real output, never generated.** Reference images come from
a known-good build; BSP and asset fixtures are real map data. A fixture
you synthesised tests your idea of the format.

**Verify in a build directory you made from clean.** An incremental
`$(BUILD)` accumulates files nothing produces any more, and it keeps
running while a fresh checkout cannot: `data/assets` is generated and
untracked, nothing regenerated `soldier.geo`, and every clean build died
in `mdl_load` with `runtime error 9 at line 0` while the old directory
ran fine on a copy made by hand months earlier. `make assets` builds the
monster too now, and `mdl_load` says which file is missing.

**A fix starts with a failing test.** Reproduce it, watch it fail, then
fix. New code does not need the test first, but the coverage has to be
real. `tools/check.sh --churn` was written this way: it failed on the
pool build before anything was changed.

**Test behaviour, not implementation.** `sc_selftest` checks the
allocator's invariants and passed with both of this session's bugs
present; the test that caught them compares two frames.

**Mutation-check every fix.** Put the bug back and confirm something
notices. Forcing `sc_find` to always miss is the form this takes here --
it made a nondeterministic render byte-identical, which is what named the
hit path.

**When the source measures clean and it still misbehaves, stop measuring
the source and check what was BUILT.** A stale `uglv.lib`, a shared build
directory, an unresolved external that LINK emitted an EXE around
anyway -- see **Build directories: never share one**.

**Consult an Opus-model agent for review at every iteration, not just
the ends of a task.** Before writing code, after writing it, after each
round of changes while iterating, and whenever stuck -- not only at the
start and finish. Resume the same agent across rounds rather than
starting fresh each time, so it keeps the thread of what it already
reviewed. Fall back to Fable if Opus is unavailable or fails.

## Build directories: never share one

**Always point a build at a path unique to your worktree/session.** More
than one agent works in these trees at once. Two processes writing one
`BUILD` interleave their objects, and LINK still emits an EXE around an
unresolved external -- `int 3` at the call site -- so the symptom is a
run that "compiles clean and prints nothing", not a build error. Hours
went into that when the library rules did it.

    BUILD=$PWD/build/vbd-<tag>       renderer objs, the EXE   (make)
    VBD_OUT=$PWD/build/vbd-<tag>     BENCH.BMP, bench.txt     (check.sh)

`VBD_OUT` matters for a subtler reason: `check.sh` compares `BENCH.BMP`
against the reference and reads ticks from `bench.txt`. Two runs sharing
that directory can have the picture from one run and the timing from the
other, and the comparison still "passes".

`make -j8` is safe: one parallel make orders its own rules. The unsafe
thing is two separate `make` PROCESSES in one directory. Anything
reporting TIMINGS sets `CORE=normal` explicitly: the recompiler throws
its translations away when a filler patches immediates into its own
inner loop.

## Drive DOSBox hands-on, do not wait on files

**Watch the emulator while it runs. Do not start a run and wait for a text
file to appear.** Waiting blind cost most of an afternoon here: three
separate "it is hung" conclusions were all a run that simply had not
finished, and a "control" that had died at load with `runtime error 53`
was read as evidence for an hour because nobody looked at the screen.

The MCP DOSBox tools attach to a conf that has a debugger section --
`tools/dosbox.sh viz <map>` emits one:

    tools/dosbox.sh viz dm3ish.bsp        # prints the conf path
    dosbox_launch { conf, headless: true }
    dosbox_screenshot { path }            # then Read the png
    dosbox_text_screen                    # text mode
    dosbox_regs                           # is it executing, or spinning?

What each answers, in seconds rather than minutes:

- **screenshot** -- is the picture right, and is the overlay advancing?
  Two identical captures a few seconds apart mean one frame is taking
  longer than that, NOT that it is stuck. Space them out before deciding.
- **regs** -- sampled twice, do CS:EIP move? Moving means slow, not hung.
  `ES` near the EMS frame (0xE000 + slot*0x400) says it is in a window.

**EVERY inspection FREEZES the CPU and does not resume it.** `dosbox_regs`,
`dosbox_where` and `dosbox_screenshot` all leave `cpuRunning: false,
debuggerFrozen: true` -- check with `dosbox_status`. Resume with
`dosbox_continue` (it will time out waiting for a stop that never comes;
that is fine, the CPU is running again). Forgetting this makes the
emulator look stalled BECAUSE YOU STALLED IT, and any wall-clock number
taken across an inspection is worthless. Sample for WHERE it is, never
for HOW LONG it took.
- **RUN.OUT / ERROR.LOG / ERRMEM.TXT** -- read these FIRST when a run
  produces nothing. An empty RUN.OUT with an ERROR.LOG beside it is a
  runtime error, not a hang.

**A viz window takes keystrokes.** `autolock=true` means a stray key
reaches the guest, and the renderer's toggles are single letters -- `L`
flips lightmaps, `B` culling, `F1`/`F2` mip and perspective. A screenshot
that looks wrong may be a toggle, not a regression: read the status bar
before believing it. `L lm off` with `Hit / built 0/0` is the lightmap
toggle, not broken lightmaps.

**Drive DOSBox hands-on, do not wait on files**

### One run per side is not a measurement

`ft_mean` over a campath run has a run-to-run spread of about **2.3 ms**
-- wider than most changes worth arguing about. It is also quantised:
the same handful of values recur across unrelated builds, so two runs
agreeing to ten decimals means the metric is coarse, not that the builds
are identical.

**Six runs per arm, interleaved A/B/A/B, and compare medians.** Not six
of one then six of the other: the host drifts, and a block design loads
that drift onto whichever arm ran second.

Build the other side in a worktree so both trees stay intact:

    git worktree add -q --detach /tmp/base <commit>
    cp -r data/assets /tmp/base/data/
    make -C /tmp/base BUILD=/tmp/base/build/o
    cp build/vbd-x/campath.bin /tmp/base/build/o/

    for r in 1 2 3 4 5 6; do
      for a in "base:/tmp/base:/tmp/base/build/o" "head:$PWD:$PWD/build/vbd-x"; do
        IFS=: read t rt out <<< "$a"
        VBD_OUT=$out QFLAGS="-lm -campath" $rt/tools/dosbox.sh run > /dev/null 2>&1
        echo "$t $(grep '^ft_mean ' $out/bench.txt | cut -d' ' -f2)"
      done
    done

`git worktree remove --force` each one afterwards.

Three per arm is not enough. A 3v3 here produced a clean-looking +1.95ms
with no overlap between the arms, which did not survive n=6 -- the
medians then differed by 0.04ms. The confirming test is cheap and worth
running whenever a delta looks real: **bench the baseline against
itself**, perturbed only in layout -- add a never-called sub of about
the same code size to the module that grew, and rebuild. If the control
reproduces the "regression", the effect is not in your change. It did.

Before believing any delta, check what actually changed in the hot path:

    grep -E ' (D_POLY|D_SURF|SCREEN|PL_MOVE)_CODE' build/<dir>/QRENDER.MAP

Byte-identical segment sizes for the per-frame code mean no per-frame
work was added, whatever the timings say.

## Player physics

`pl_move.bas`, ported from softquake's `bsp_trace.c` and `pl_move.c`, which
are Quake's `SV_RecursiveHullCheck` and `SV_FlyMove`.

**The hulls are pre-expanded.** The clipnodes lump is a second set of bsp trees
over the same planes, each grown by a bounding box, so the player is traced as
a *point* through hull 1 rather than as a box through the world. That is why
collision needs no box maths at all.

**Two coordinate spaces meet here, and only here.** The renderer is Y-up; the
bsp is Z-up. `mod_find_spawn` already swaps when it reads the spawn origin.
`pl.pos` is Z-up and authoritative, and the last three lines of `pl_move`
convert it back for `cam.pos`. Do not do the swap anywhere else.

**`-walk` holds forward** so the collision response can be tested headlessly.
From the dm3ish spawn, 200 frames of it should travel ~400 units, drift
sideways where it meets a wall, and descend to a floor with `onground` true
and `vz` zero. A run that ends with x and y unchanged means the trace is
reporting solid everywhere; one that ends with a huge negative z means it fell
through the world.

**The simulation runs at a fixed 60 Hz, whatever the renderer manages.**
`host_advance` takes the frame's real elapsed time, adds it to an accumulator,
and spends it in whole `HOST_DT` steps, carrying the remainder. So a frame runs
one step, or two, or none -- but every step is the same length.

That is what makes it deterministic rather than merely framerate-independent.
Time-based updates alone still integrate a walk in a few long steps at 12 fps
and many short ones at 45, and the two drift apart, because a long step
overshoots a wall a short one stops against. `-ticks N` stops after N
simulation steps rather than N frames, which is the test:

    44 fps  220 frames  301 ticks   209.348   -287.9687  184.0313
    12 fps   64 frames  300 ticks   209.4243  -287.9687  184.0313
     4 fps   61 frames  300 ticks   209.4243  -287.9687  184.0313

The 12 and 4 fps runs agree to every digit across an 11x spread. The 44 fps
run differs in x alone because it ran one extra tick, worth about 0.08 units.

**`HOST_MAXSTEPS` caps the steps one frame may run**, or a frame slower than
`HOST_DT` asks for more steps, which make the next frame slower still, and the
accumulator runs away. At the cap the game runs in slow motion, which is
survivable; without it, it stops.

**The frame is `host_tick` then `host_render`.** One changes the world and
draws nothing; the other draws and changes nothing. `host_tick` takes dt as a
parameter rather than reading `scr.frame_time`, so a caller can hand it a
different step -- a fixed one, or a halved one for a sub-tick -- without the
routine knowing.

**Time, not frames.** Everything that moves multiplies by `scr.frame_time`,
measured once at the top of the frame by `sys_frame_time`. Nothing may advance
by a per-frame constant -- noclip flew at 3 units a frame for years, which
means it flew at whatever speed the framerate happened to give it.

**The frame clock calibrates itself, and has to.** Asking uGL for a 1 kHz
timer and dividing the counter by 1000 gave a dt seven times too small: the
physics was frame-rate independent but ran in slow motion, every speed in the
game being units per seven seconds. What mgl delivered was 145.6 Hz: its
`tmrInit` programmed a PIT divisor of 8192 whatever was asked for. The PIT
is qgl's now -- `qgl/tmr.asm` hooks INT 8 at a real 1000 Hz and chains
the BIOS handler each time the divisor accumulates to 65536, so DOS's clock
keeps its 18.2 -- and `sys_time_init` still measures it against DOS's own
TIMER, because the emulator is still underneath it.

That measurement aligns both ends of its window to a TIMER edge, because
TIMER only ticks every 55ms: without the alignment the same binary measured
142.9 Hz on one run and 148.1 on the next, and the game ran 4% faster on one
of them. Aligned, three runs gave 144.0, 145.5, 144.0 on mgl's timer.

**Controls.** W/S forward and back, A/D strafe left and right, mouse looks,
mouse buttons also walk. Space jumps. F1 mips, F2 render mode, F3 birdseye,
F4 noclip, F5 screenshot, F12 stats, B backface culling.

Screenshot is F5 because S walks backwards. That is the only binding WASD
displaced.

**`-at X Y Z`, `-yaw D` and `-nostats` aim a headless run at a thing and get
the overlay out of the picture**, which is what makes a rendering bug
photographable without a keyboard. `-yaw` is mirrored relative to the map's
angle key: aim with `atan2(-dy, dx)`.

**`-jump` holds jump, `-walk` holds forward, `-strafe` holds strafe**, and `peakz` records the highest
point reached, so a jump is provable from a headless run: from the dm3ish
spawn it should peak **47.8** units above the resting height. Not the
45.56 that `v^2/2g` gives for Quake's 270 up and 800 down: a fixed 60Hz
step overshoots the continuous result, and the measurement is the
authority.

**Movement is Quake's, from `sv_user.c`.** Walking is `cl_forwardspeed`
200; 320 is `sv_maxspeed`, what `+speed` gets. `sv_edgefriction` doubles
friction over a drop and is ported. Before this, a flat 500 u/s^2 against
speed-proportional friction settled at `500/4 = 125` u/s -- the 320 cap
it carried was never reached, so "the cap is unchanged" was never the
same as "the speed is unchanged".

**Simulate id's code offline before trusting a movement port.** A Python
transcription of SV_Accelerate/SV_UserFriction predicts the whole curve
in seconds; ours then measured 197.9 and 199.8 u/s against a predicted
200, with a constant 63.9 unit offset that was the spawn fall under
SV_AirAccelerate's 30 u/s cap.

**Water and lava live in hull 0, not the collision hulls.** The clipnodes are
built for a box to move through and carry only EMPTY and SOLID -- on dm3ish,
579 EMPTY and 1077 SOLID children and not one WATER. `pl_point_contents` walks
the render tree instead and reads the leaf's contents, which is why `leaf2`
keeps its `cont` field: an earlier version of the lump conversion dropped it as
unused, and it had to come back for this.

`pl_water_level` samples three heights up the body -- feet, waist, eyes -- for
0..3, which is what makes wading feel different from swimming.

**Quake encodes what a texture does in its name.** A leading `*` is a liquid,
a leading `+N` is one frame of an animation whose other frames share the name
after the digit. `mod_load_textures` classifies them and `mod_link_anims`
groups the chains; `d_draw_faces` applies both once per face, not per vertex.

A liquid is perturbed, not scrolled: each coordinate is displaced by a sine of
the *other* one, from a 256-entry table, which is what makes a surface roll
instead of slide. Quake's amplitude is 8 texels of 64 and its index is
`(other*0.125 + time) * 256/2pi`; ours works in normalised coordinates, so the
amplitude is 8/64 and the 0.125 absorbs the texture width.

**Those coordinates are normalised, not texels** -- `tw` and `th` are
reciprocals of the texture size. An earlier version scrolled by `time * 8`
there, believing it was texels, and moved the texture eight entire widths a
second: consecutive frames were uncorrelated, which looks like static and which
a two-frame diff reported as "no animation at all".

The perturbation is per vertex, which is as fine as this renderer goes. Quake
does it per span inside its own texture mapper, and uGL's mapper is not ours to
change.

**A chain's frames sit anywhere in the lump.** e1m1 has `+0planet` at
55 and `+1..+3planet` at 70..72, `+0slip` at 56 and `+1..+6slip` at
73..78. `mod_link_anims` used to count the frames and link a contiguous
run from the first, which made `slip1`, `sliptopsd` and the `trigger`
texture frames of the planet and the button: the wall above the exit
showed the Earth. It now does what `Mod_LoadTextures` does -- finds
each frame by its digit, `+a..+j` as the alternate sequence, links a
ring -- and `d_faces.c` steps round the ring at 10 fps from the face's
own frame. dm3ish has no `+N` textures; the exit frame of
`tools/check.sh --e1m1` is where the chains are exercised.

**Brush entities are submodels 1 upward.** `r_draw_world 0` draws the world;
`r_draw_brush_model` adds each other submodel to the same draw order without
resetting it, so entities sort against the world back to front rather than
being drawn over it.

Their leaves are not in the world's PVS -- that answers where the camera can
see from, and a lift is not part of it -- so `r_ignore_pvs` skips the
visibility test for them and lets the frustum decide alone.

A trigger volume is a submodel too, and must not be drawn: `mdl_draw` is false
for any submodel some `trigger_*` entity claims, or dm3ish hangs two slabs of
teleport texture in mid air.

~~Verified by A/B: looking at the func_plat, 176 polys with brush entities
against 170 without, and 10% of the frame's pixels different.~~ **Withdrawn.**
That A/B was taken from (-80, 700, 100), which is leaf 0, contents SOLID. It
measured nothing. See the note below on viewpoints.

**Ordering brush entities correctly without a depth buffer is not possible in
general, and it is worth knowing why before trying.** A BSP back-to-front walk
is not an approximation -- it is exact, and that is the theorem the tree exists
for (Fuchs, Kedem, Naylor 1980): every polygon lies on a plane in the tree, so
the traversal is a total depth order valid from any camera. That exactness
holds for **one** tree. A brush entity is a second, independent, moving tree,
and two trees admit no exact whole-object ordering: they can overlap
cyclically, and even without a cycle an entity straddling a world plane has
world faces both in front of part of it and behind another part.

The exact answers are (a) split the entity's polygons against world planes so
every fragment lands in one leaf -- which this pipeline cannot express, since
it addresses faces by index out of the face buffers and a fragment has no
index -- or (b) what Quake actually did, which is not to sort at all: the
software renderer builds a global edge list and resolves depth per span with
1/z (`R_DrawSurfaces`, `surf->key`). Quake never needed a whole-object order.
Do not cite Quake as precedent for a painter's algorithm; it is not one.

**What is here is (c): insert the whole entity at the right node.** It is an
approximation, and correct exactly when the entity does not straddle a plane
separating world geometry in front of it from world geometry behind it -- a
door in a doorway, a lift in a shaft. Say so rather than claiming the bug is
fixed.

`ent_find_node` descends the world tree with the entity's box and stops at the
deepest plane the box does not straddle; `r_emit_entities` draws it there,
between the far subtree and that node's own faces.

Three placements were tried, and the two failures are the instructive part.
The leaf holding the entity's *centre* is often solid -- a lowered lift sits
inside its own shaft -- and a solid leaf is never walked, so the entity
vanished. The **first visible leaf its box overlaps** was not merely
approximate but backwards: the walk reaches leaves far-to-near, so "first"
means "furthest", and everything the entity should hide is drawn after it.

**A viewpoint inside solid geometry makes every measurement from it
meaningless.** An A/B that seemed to prove entity rendering worked was taken
from a camera in leaf 0, contents SOLID. `ent_point_leaf` will tell you: leaf
contents -2 means the camera is in a wall, and the PVS from there answers
nothing.

**`-noents` is the reference image, and the only objective test here.** From a
viewpoint where the world fully occludes an entity, a correct renderer must
produce a frame *identical* to one drawn with no entities at all. That turns
"does it still overdraw" into a number instead of a squint. Generate the
viewpoints by walking the leaves in Python, keeping those whose centre has a
line to the entity that passes through SOLID, and sweep them.

**Occlusion means every point of the entity is hidden, not its centre.** The
first version of this test traced one ray, to the box centre, and called that
occluded. It let partly-visible entities count as failures and produced a
confident 0-versus-8 that measured nothing. The tell was that the two orderings
failed on nearly disjoint viewpoint sets -- 16 one way, 14 the other, 1 both.
Two orderings that disagree that way are not better and worse, they are drawing
a visible thing at two different depths. Sample the box on a 3x3x3 grid and
require every point blocked, and trace in 2-unit steps: at 1/200th of a 900-unit
range the step is 4.5 units and walks straight through a thin wall.

**Aim it at something that draws.** dm3ish has three submodels and only one of
them renders: `*3`, the func_plat. `*1` and `*2` are `trigger_teleport` volumes
that `mdl_draw` deliberately suppresses. Viewpoints aimed at those can never
leak either way, so they pad both counts with free passes -- which is what made
an early sweep read 0-versus-8 rather than 0-versus-19.

Aimed at the lift alone, 50 viewpoints where all 27 of its box samples are
hidden: **0 leaks with insertion-node ordering, 49 with `-badorder`.** Frame
rate is unchanged at 33-34 on the spawn bench, which needed `vis.ent_left` -- a
SUB call at every one of a few hundred visited nodes cost about 3%, an integer
compare costs nothing.

**The A/B that shows ordering is `-badorder`.** It puts brush entities back
after the world walk from the same build, so one binary renders both. Pair it
with `-at X Y Z`, `-yaw D` and `-nostats`; `-yaw` is the map's angle
convention mirrored -- the freelook math makes the eye direction
`(cos a, -sin a)` in bsp x,y, so aim with `atan2(-dy, dx)`, not `atan2(dy, dx)`.

Most viewpoints render identically either way, because ordering only shows
where world geometry stands between the eye and the entity. The one that
shows it on dm3ish is (-608, -544, 176) yaw 265, looking at the door at
(-640, -192): 241 pixels of 64,000, a dark sliver of the door's edge bleeding
through the wall. Small, but it is the whole bug.

**A moving brush entity is traced by moving the line, not the hull.** Its tree
sits where the compiler put it, so subtracting `mdl_zofs` from both ends of a
sweep asks a stationary tree the same question that moving the tree would ask
of a stationary line. `pl_trace` walks the world's hull and then every solid
submodel's; `tr` keeps the earliest hit by itself, since `pl_hull_check` only
writes when it beats `tr.frac`. `all_solid` is the exception -- each walk sets
it -- so it is gathered by hand.

The renderer does the same offset in the other direction: one add per vertex,
`polyb.y = vz + zofs`, because renderer y is bsp z.

**A plat carries its rider upward only.** A descending one is left to drop away
under the player, which is what it looks like in Quake, and what avoids pushing
them through the floor of the shaft on the last step down.

**Teleporters are entities, not geometry.** A `trigger_teleport` has no
origin: it carries `"model" "*1"`, meaning submodel 1, whose bounding box is
already in `mdl_buffer`. Its `"target"` names an `info_teleport_destination`,
which has the origin and facing. `ent.bas` reads both and pairs them, in two
passes, because a trigger can name a destination that appears later in the
text.

**An entity value arrives already split.** The block is tokenised with space
among the separators, so `"origin" "448 416 176"` is four tokens and the
"value" of origin is just `448`. `ent_vec` reads three consecutive tokens;
`ent_value` reads one. Getting this wrong teleports the player to
(448, 0, 0) -- x right, the rest zero, which is exactly what it looked like.

**`-at X Y Z` starts the player somewhere specific**, which is how the water is
tested at all: dm3ish's pool is at x 336..688, y -336..256, z -128..-16, a long
walk from the spawn. Dropped in at (500, 0, -60) the player should report
water_level 3, water_type -3, on_ground 0, and sink to about -104 with vz back
at 0.

**A resting z always ends in .03125.** That is `PL_CLIP_EPS`, the distance the
trace stops short of a surface. Seeing it is how you know a landing is a real
trace stop rather than a coincidence.

## Assets

    make assets          # or: python3 tools/mkassets.py <map.bsp> <base.dat> data/assets

Emits two 8-bit atlases -- `texr.raw` raw indices, `texs.raw` with colormap
row 0 applied -- plus `texofs.bld`, a `[id*4 + level]` table of cells: the
byte offset in bits 0..22, log2 of the cell width in 23..26 and of its height
in 27..30. Already in the game palette. The Makefile regenerates them when the map, `base.dat`,
or the tool changes.

**Two atlases and a handful of views, not 648 dcs.** A dc costs conventional
memory for its struct and scanline table whatever its pixels cost -- 264 bytes
measured, and dm3ish made 160 of them. e1m1 would make 648, which is the
~171K that stops it loading. `mod_tex_raw`/`mod_tex_shaded` re-aim one view
instead: `mem textures` went 42,176 -> **0** and `mem_avail` up 25,296.

**It costs 1.8% of the frame, and that is the trade.** Six runs per arm
interleaved on the dm3ish campath: 82.774ms before, 84.229ms after, medians,
spreads 0.000 and 0.213. The cost is a procedure call and a re-aim per face
where an array index used to do. Memoising the aim -- each view remembers
which cell it points at, and consecutive faces usually share a texture --
recovered only 0.14ms of it, so most of the remainder is the call itself.
Pinning `gv_buf` into DGROUP to make the address hoistable again was tried
and abandoned: a bounded `dim` in `main.bas` puts the DECLAREs after an
executable statement and BC rejects the file.

**A cell is a flat run of `cw*ch` bytes, not a window on the 8192-wide
image.** The fillers map one page and then walk the cell by the VIEW's bps,
which is the cell width -- `qgl$fillView` strides by the view's own `bps` and
never the parent's. So the packer writes cells linearly and the file is just a
container for that byte stream. Every cell is a self-aligned power-of-two run
of at most 16,384 bytes, packed largest first, so none straddles a 16K page.

**A cell is the texture's OWN power-of-two size, and that is where the blur
was.** Cells were 64x64 down to 8x8, square whatever the texture was: on e1m1
20 of 81 textures had a side over 64 and lost it, and 19 non-square ones had
their aspect squashed as well -- a 16x128 texture was upsampled 4x across and
downsampled 3 octaves down. `tex_cell_levels` rounds each side DOWN to a power
of two (Quake sizes are multiples of 16, not powers of two) and halves the
larger side until the pair fits one EMS window. Nothing else can be the bound:
a view addresses its cell by masking, and the filler maps one page.

Measured before assuming the mip choice was at fault: `-nomip` at the e1m1
spawn moves **213 of 16,000 pixels, 1.33%** -- nearly every visible face is
already at mip 0, so the blur was the cell and not the level. The new atlas
moves 23.7% of that frame and is SMALLER, 539,008 bytes a variant against
565,760, because the 32x32 textures stop being upsampled to fill a 64x64 cell.

**A level under 64 texels shares the level above** rather than getting a cell
of its own: 2x2 is a scanline table for nothing, and the saving is 440 bytes of
e1m1's 508K, so this is about degenerate views and not memory. So a level's
cell is NOT `cell(0) >> mip` and nothing may derive it that way.

**A view is one per cell HEIGHT, not one per mip.** `qglSfViewShape` changes a
view's width, xRes, bps and xMax but not its height -- the address table is the
height's -- so `mod_tex` keeps `TEX_HEIGHTS` slots and makes one on first use.
Same shape the surface cache's five drawing views already have.

**The wrap needs a mask per axis.** `SBPARM.sb_msk` is the u wrap and
`sb_vmsk` the v wrap; with one mask a 32x8 cell reads row `(y and 31)`, which
for y >= 8 is a row the cell does not have, and the face comes back carrying
whichever cell is packed after it. `t36sbcell` is the regression test and it
fails with the masks merged.

**The placement is emitted, not re-derived.** mkassets owns the layout and is
free to pack in whatever order is tightest; deriving it twice is the bug the
luxel atlas already avoids by shipping its own table.

**`-dumptex` reads every cell back THROUGH ITS VIEW** with `qglSfPget` and
writes `texdump.bmp`, a contact sheet of one column per texture by four
stacked mips, measured from the cells rather than assumed square.
Diffing that against the atlas is what proves the views deliver the right
pixels with the renderer out of the way -- 108,800 texels, 0 mismatches. Do
this before suspecting the atlas: it was right, and the fault was a stale far
pointer in `d_poly` that the memory change merely exposed.

`tools/sbref.py` reads the atlas through the same table, so it stays a
reference for `uglBuildSurf` -- but note it reads the SAME assets the target
does, so it validates the builder and not the data. To check the data,
compare against the previous build's per-texture bmps.

Three things this has to get right, all found the hard way:

**Apply colormap row 0 on the way in.** The original read every miptex byte as
`colmap[byte]`, not as a palette index. In this data set row 0 is nowhere near
the identity -- 221 of 256 entries differ -- so skipping it shifts nearly every
texel. Frame comparison against the old path caught it: 0% identical before,
99.2% after.

**`BMPOPT.NO332` on the load.** Without it uGL remaps the image into its own
3-3-2 palette and destroys indices that are already correct.

**`uglBlit` does not exist in the VBD library.** It is declared in `ugl.bi`,
which is why an atlas-per-mip design compiled; LINK resolved the call to an
`int 3` and the program died on the first blit. `uglNewBMPEx`, `uglPutBMP`,
`uglPut` are all present. Check a symbol is really in `uglv.lib` before
building on it -- `ugluBMPSave` was the same trap earlier in this project.

**LINK emits an EXE even with an unresolved external**, so the harness's
"did an exe appear" test reported PASS for a build that could not run.
`tools/dosbox.sh` now greps `link.out` and reports LINKERR.

**Names beginning with `FN` are reserved** for `DEF FN` user functions. `dim
fname as string` fails with "Simple or array variable expected", and the use
sites report "FUNCTION not defined".

## Method

**Linking clean proves nothing** for a `COMMON SHARED` change — binding errors
are runtime failures. Every module cut needs a *run*.

**When something looks broken, A/B against the last known-good build under the
identical harness before diagnosing.** Four separate times a harness artifact
was mistaken for a code fault here. The A/B settles it in one cycle; reasoning
about each anomaly in isolation cost about a dozen.

**A real bug fixed with no change in symptom means the wrong cause, not a
doomed approach.** The texture cut took three attempts. On the first I found
`hTextrDC` losing its bound to `COMMON`, fixed it correctly, saw no
improvement, and concluded the cut was hopeless — when the evidence actually
said there was a second, different fault. It only became findable when
`screen.bas` failed identically and gave a second data point to triangulate
from. Two failures with one signature are worth more than one failure studied
twice.

**Measure, do not infer.** "Ran for 150s" read off a background task hitting
its timeout was wrong; sampling CPU showed it exited in under 30.

Two techniques that paid for themselves:

- **Step logging.** Insert a `dbgMark "name"` before each step of a long
  routine, writing to a file with open/close each time so it survives a
  non-returning error. Located the `texiCount` subscript bug in two runs.
- **Control builds.** Compile the pre-change version alongside and compare
  error counts. Separated 3 pre-existing defects from 14 introduced ones in a
  single pass.
- **Offline analysis.** Parsing the BSP in Python to check lump counts,
  texture sizes and per-face UV ranges answers questions in seconds that cost
  minutes per emulator round trip.

## Open

### The walk must read pvs_now, not pvsb

**Fixed, and the HUD is how it was caught.** `r_walk.c` tested
`rdr->pvsb` -- the raw PVS for the camera's leaf -- where the BASIC
passes `pvs_now()` into that routine's own `pvsb()` PARAMETER
(`r_bsp.bas:439`). Once the arrays became `Renderer` fields the name
matched the wrong one, so `r_portal_mark` built the narrowed set,
reported `pt_culled` to the overlay, and the walk then ignored it:
portal culling moved a HUD counter and nothing else.

The tell was exactly that -- "leaves portal-cut" changed while polys
did not. A toggle that reports work and does not change the picture is
not a toggle, and this is the second time in this project a counter has
been believed over the frame.

The A/B, and the regression check until cport has a harness -- e1m7,
`-lm -nostats -ticks 60`, static at spawn:

| | frames | polys | polys/frame |
|---|---|---|---|
| `-noportal` | 17 | 8466 | 498 |
| portal on   | 39 | 3889 | 100 |

Identical numbers on both sides means the bug is back.

Same commit fixed a second defect at that call site: `r_portal_mark`
and `r_portal_draw` were passed `0.0f, 0.0f, 0.0f` for
`xresh`/`yresh`/`z_near` under a comment saying "until Config exists".
Config exists; they get `x_res/2`, `y_res/2` and `z_near` now, which is
what their own projection needs to answer for the right screen.

### cport's freeze was an EMS window read through a stale record

**Fixed.** It is not the mouse and it is not live input: `-walk` on
e1m1 reproduces it headless in about eight seconds, which is what made
it scriptable. The chain, each link measured:

`dctems` kept its own per-slot record of which page it had put in a
window (`qgl$emsCtx.ppgTB`) and answered a matching request with no map
at all. Its header said that was coherent "because every map anywhere
goes through gem" -- but going through gem is not going through
dctems, and `snd.c`, `snd_mix.c` (once a frame), `mdl.c`, `d_alias.c`
and `ar.asm` all map `PAGE_SLOT` with a plain `qglGemMap`. From the
first mixer paint onward the copy is stale, and a stale copy answers
with the segment and no INT 67h: the caller gets a window holding
somebody else's page. Silently -- the bytes read back are a
plausible-looking record of the wrong thing.

On e1m1 that made the geometry store's rows 28 and 29 (one page, hence
one key, hence exactly two rows) read back as sound samples: `vcnt`
20174 where the file says 4, and a luxel grid `lmw` -4077 where it
says 7. `sb_build` guarded its scratch copy with `lmw * lmh <= 1024`,
which a negative width passes, and `(size_t) -4077` is 61459 -- a
`rep movsw` from the lightmap atlas through a 1024-byte buffer, BSS
and the stack, landing in `__fmemcpy`'s own return address. The branch
ring's last two entries were `_sb_build+0x248 -> __fmemcpy` and
`__fmemcpy's ret -> 0000:0000`, and the "freeze" is the program dying
without restoring the video mode, so the last frame stays on screen.

The record belongs to gem, which owns the mapping; the slot-taking
paths now compare against `qgl$gem_key`, gem's own record packed the
way an addrTB entry packs it, so the compare still costs a compare.
`t35emsslot` is the regression test, and it fails with the copy put
back. e1m1 `-lm -walk -ticks 400` completes, and `polys` went 65321 ->
67670 because faces that were reading another store's page now read
their own.

`qrender.exe` maps `PAGE_SLOT` the same way from `model.bas` and
`d_mdl.bas`, so the same defect was under it. Whether it is the whole
of that build's own crash is not claimed here; it was not re-tested.

**The old investigation, kept for what it ruled out.** `cport/` crashed
after a minute or two of being driven
by hand -- `-comp -lm`, dm3ish -- and does not crash any other way. It
is not a hang: the CPU takes a real invalid-opcode exception (vector 6)
at `CS:EIP` pointing somewhere it was never meant to jump (`0000:0000`
once, `ugl_text+0x22E0` another time, and once the whole DOS session
reset -- `lastExitPsp` went from 2068 to 52501).

**What is ruled out, each by measurement rather than argument:**

| Ruled out | How |
|---|---|
| The camera path / map geometry | Recorded a crashing session's real input, replayed it: 489 frames, same end position (`72,238,-545` vs recorded `74.4,238,-545.7`), no crash. Three separate paths replayed, none crashed. |
| A specific place in the map | Crash positions differ every run -- `104.8,206,369.5`, `131.7,206,226.4`, `74.4,238,-545.7`. |
| Stack overflow | Painted the stack below SP and read the low-water mark back: **3,126 bytes used, 5,066 still untouched**, and the region is real stack (an 8K paint runs clean, 16K breaks -- so the floor is 8-16K down). An interrupt frame is 32-64 bytes. It cannot cross 5K. |
| `_stklen` | Set to 32768 and reads back as 32768, but SP top does not move (0xDF88 -> 0xDFAA) and the image grows only by the size of the added code. tc201's `C0M.OBJ` wins; the variable is a decoy. Left in place, and it is NOT a fix. |
| `mousePos`'s unconditional `cursorShow` | Real bug, found and patched in `mdmouse.asm` (every other entry point gates on `ms.hidden`; that one did not). Crash survives it. |
| The `-comp` composite DC living in EMS | Switched to `UGL_DC_MEM`. Crash survives it. |

**What is left, and it is the whole finding:** the crash depends on
*when* an interrupt arrives relative to the renderer, not on what the
interrupt carries. Replay feeds identical input synchronously at the top
of a frame and never fails; live, the ISR fires at an arbitrary
instruction, including deep inside the EMS and rasteriser paths. The
same sensitivity showed up twice more: adding a per-frame `fopen` to the
recorder changed whether it reproduced at all, which is why the recorder
now writes to a far buffer and touches no file during a run.

**Tools built for this, worth keeping:**

- `-record F` / `-play F` in `demo.c`. Until `tools/test-demo.sh` the
  file was never the recording: `fwrite` takes a near pointer, the far
  buffer lost its segment, and every R.BIN was DGROUP -- identical run to
  run, so two recordings agreeing proved nothing. One fixed record a frame: nine
  shorts of real mouse/key state, then the camera position and yaw that
  frame produced. `-play` feeds the input back through `host_advance`
  (no position override) and reports the first frame that drifts more
  than a unit off the recorded answer, so a replay that has stopped
  being the same run says so instead of being quoted as one.
- Recording lives in a far-allocated buffer whose address is printed
  once via `mark()` (`rec_buf seg=... ofs=...`). A crash never reaches
  any flush, so the buffer is read out of guest memory afterwards.
- **`mem_dump` in dosbox-x's debug socket** (`src/debug/debug_socket.cpp`):
  `{"cmd":"mem_dump","addr":N,"len":N,"file":"/host/path"}` writes guest
  memory straight to a host file. `mem_read` returns hex and is capped
  well below a 64K buffer; this is what makes dumping the recording
  after a crash one call instead of sixteen.

**Do not repeat these:** the first recorder gated its input capture on a
`FILE*` that the far-buffer rewrite had already set to 0, so every
`in[]` came back zero and the first replays reproduced nothing -- which
read as "input does not describe the run" when it was only ever that
bug. And `-at` takes BSP coordinates (Z-up) while the recorder writes
renderer coordinates (Y-up); feeding one to the other aims the camera
into solid geometry and returns a black screen that looks like a
rendering fault.

**The next test that would actually split it:** do not install the mouse
driver at all (skip `mouseInit`, walk on the keyboard). Survives ->
it is the mouse ISR and `handler` in `mdmouse.asm` is the whole search
space. Still crashes -> the mouse is a red herring and the variable is
interrupt load in general, with the 144 Hz timer the next suspect.

**A mapped EMS pointer is only as good as its lock, and the lock has to
come before the next ACQUIRE -- not before the use.** mgl's four page
slots are an LRU pool now: `emsSlotAcquire` evicts the least recently used
UNLOCKED slot, so a pointer from `mod_lm_map`, `mod_cm_map` or
`mod_geom_map` stops being valid the moment anything else wants a window.
It still reads. It just reads another page, with no error anywhere.

`sb_build` holds two at once -- the face's luxel rect and the colormap --
across `uglBuildSurf`, which acquires two more of its own. Unpinned, the
colormap window went first and every texel came out of a wrong lookup:
the frame was RGB confetti with the geometry still perfectly correct.

Pin at the point of handout:

    lmp = mod_lm_map( g, lmy )
    lm_lock = mod_lm_lock            '' before anything else can acquire
    ...
    sbp.cmap_ptr = mod_cm_map( g )
    cm_lock = mod_cm_lock

Locking both AFTER both maps looks equivalent and is not: the colormap's
own acquire is itself allowed to take the luxel slot. That mistake cost a
build cycle and looked exactly like the unpinned case.

Two of four, never more. `emsSlotLock` refuses the LAST unlocked slot, so
the pool always keeps one in reserve; pinning two leaves the builder the
two it needs. Unlock only what locked, and there must be no `exit sub`
between the lock and the unlock.

**Pinning is necessary and is NOT sufficient, measured.** With both
windows pinned exactly as above, a campath run still renders a different
picture every time. The pinning is reverted in the tree rather than left
in looking like a fix -- and note it makes the squeeze worse, not better:
four windows want slots (destination, atlas, luxels, colormap) and pinning
two leaves the builder cycling the atlas through the other two.

## The wandering streaks were the MODEL, and the near plane was half of it

Long black diagonal bands across walls, moving as the camera moves. They
were read as a surface-cache fault for a long time. They are alias-model
triangles projected into screen-spanning slivers.

The bisect, all at campath tick 360 (`-campath -bench 4000 -ticks 360`),
which is the first frame that shows them, counting index-0 pixels below
the HUD bar:

| arm | stray pixels |
|---|---|
| lit, depth on | 138 |
| `-noz` | 138 |
| unlit -- `sc_built 0`, no surface cache at all | 138 |
| `-nocull` | 138 |
| `-nomip` | 138 |
| `-affine` | 138 |
| **`-nomdl`** | **0** |

Identical to the pixel across every renderer variable, and gone entirely
without the model. Two more measurements pinned what they are:

- **Clearing the backbuffer to 251 left them black.** So the filler
  WRITES those pixels; they are not cracks between polygons showing the
  cleared ground through.
- **Drawing the model `QGL_M_FLAT` in colour 251 turned them red.** So
  they are model triangles, and the fault is in the geometry, not in the
  skin fetch.

`mdl_draw` (`d_mdl.bas`) REJECTED vertices behind the near plane
(`if tw < z_near then mdl_okv(v) = 0`) and drew a triangle only when all
three survived. That is not clipping, and it was the obvious cause: a
triangle entirely in front but with one vertex a hair past `z_near` keeps
an enormous `rw = 1/tw` and projects to a sliver. `d_faces.c` does the
real thing for world faces (`clip_w`).

**Porting `clip_w` to the model made it WORSE -- 138 stray pixels to
381.** That is the interesting part, and it is why this note is longer
than the fix. The near clip was correct and it exposed more of the same
defect, because it kept triangles the old reject threw away.

A second false step is worth recording because it looked like a fix and
was a bug: the draw was `QGL_M_PTEX`, which wants u/w and v/w, so raw u,v
was "corrected" to divided ones (381 to 378, i.e. nothing). **The model
is drawn AFFINE.** It always was -- this module's own header names
`uglTriT`, mgl's affine textured triangle -- and the affine filler steps
u and v linearly in screen space, so raw coordinates were right all
along. It is `QGL_M_TEX` now, with raw u,v, and only `QGL_M_PTEX` owes
the divided pair.

**The cause is that clipping to the near plane does not bound the
projection.** A corner at `w = z_near` with a large `x` still projects to
tens of thousands of pixels: measured, the model reached screen x 76,750
on a 160-wide surface. qgl clips to the view rectangle -- `clip.asm`,
Sutherland-Hodgman on four edges -- but AFTER the divide, and that clip
does not DISCARD such a triangle, it stretches the surviving sliver across
the whole frame. That is the streak, and mgl behaves the same way; the
world path escapes it only because `r_cull_box` and the frustum keep
world faces off that ledge.

So the clip has to be five planes in CLIP space: `w >= z_near`, then
`|x| <= w` and `|y| <= w`, which is the view rectangle expressed before
the divide. With that, every projected corner lands inside [0,160] x
[0,100] (measured: x max exactly 160.0, min -4e-06) and the strays go to
zero.

The general rule, worth more than this bug: **clipping to the near plane
bounds w, not the projection.** Anything handing a projected polygon to a
rasteriser that clips in screen space owes it side planes too.

`tools/check.sh --model` is the regression test, `-noai` on the away arm
since the monsters wander -- by tick 360 a knight had reached a visible
leaf, `mdl_drawn 1` under an identical picture. Two viewpoints, because
one assertion cannot fail both ways: at campath tick 360 no entity is in
frame so the model must add NOTHING, and from a fixed camera 200 units in
front of spawned entity 1 it must add SOMETHING. Campath tick 240 was the
positive control first and is not one -- unlit the model contributes zero
pixels there, and the 185 it seemed to contribute with `-lm` were cache
noise. It runs UNLIT on purpose -- with `-lm` the two arms render a
different number of frames over the same ticks (drawing the model costs
time), which evicts the surface cache differently, `sc_evict` 10 against
2, and the frames then differ for reasons that are not the model's
geometry. That confound cost a false FAIL here before it was recognised.

**The loop is `d_alias.c` now.** Profiled with two models in view, the
BASIC triangle loop was 19.5 ms of a 27.1 ms model cost against 4.8 for
the raster calls it fed; in C, with each vertex tested against the five
planes and projected once so an inside triangle skips the clip, the
model costs 9.7. Same arithmetic in the same order: the frame is
byte-identical. Two things bcc taught on the way: a `static` named `cx`
becomes `DGROUP:cx[di]`, which TASM reads as the register, and a C file
named like a `.bas` file overwrites its object.

### And the other half was the clipper's ring walk, not the scanner

Stand next to a spawned model after the clip fix and the streaks were
still there -- filled wedges from a screen corner, not slivers. Every
measurement said the scanner: 179 polygons inside an 11x18 pixel box, all
in front of the near plane, and `qglRsPoly` answering 100 lines for one
of them. Two sessions went into `qgl$drawA`'s edge arithmetic on that
reading, and none of it was wrong.

What found it was a breakpoint on the BASIC line after the call, armed
only when `qz > 30`, and then reading `qgl$src` -- the clipper's output
-- against the caller's `qv`:

    qv      (142.2,54.7) (142.3,50.3) (120.0,50.6)      area -98, fine
    qgl$src (142.3,50.3) (142.2,54.7) (0,0, z 0, u 0, v 0)

The third vertex was never the caller's. `qglClPolyEx` walks the ring
from the topmost vertex with a signed step -- backwards for a CCW ring --
and wrapped by comparing the pointer against `vtx[0]` AFTER stepping,
unsigned. `qv` is a BASIC far-heap array and sits at offset 0 of its
segment, so `0 - 20` is `0FFECh`, "past the end", and the vertex came out
of whatever lay 64K above. Zeros here, hence the screen corner. World
faces never saw it because `d_faces.c`'s ring sits at a DS offset well
above 20, and the earlier live read of tri 39 happened to have its
topmost at index 2, which walks 2,1,0 and never wraps.

The wrap now happens BEFORE the step: at `vtx[0]` going backwards, jump
to one past the end and then subtract. `t09rs` case 13 hands the CCW
square in at `seg:0000` and failed all three of its assertions before
the fix; `tools/check.sh --model` reads 0 / 55 pixels against the 2125
it read before. The lesson is the second rule again: the runaway was
measured from the caller's side and the scanner's side, and never from
the buffer between them.

## The perspective half texel goes AFTER the divide, and mgl's does not

`-qgldiff` measures both rasterisers against the exact answer and asserts
qgl is no further from it than mgl. It passed at 28c3d56 with qgl at 6
and 2 texels off against mgl's 8 and 4, and failed from 47e3897 -- the
scanner rewrite "per mgl" -- at 9 and 4, for three commits.

The rewrite transcribed mgl's span start (`uglplxtp.asm` :820): half a
texel added to u/z and v/z as `0.5 * (1/z)` BEFORE the divide. After the
divide that is `0.5 * z(x) / z(start)`: half a texel at the left of the
span and two texels where 1/z has fallen to a quarter, which the 28c3d56
message had already measured and avoided. The flat 32768 after each
sub-span divide is back (`b8span.asm`, `PDIV`), and `t09rs` case 14 --
u constant while 1/z falls 4x across the span, every pixel must read the
same texel -- fails on mgl's arrangement and passes on this one.

`-qgldiff` read 8 and 4 against mgl: parity, not the 6 and 2, and the
assertion held on equality. mgl's arm is gone with the library, so 8
and 4 are now the bounds it asserts, with the coverage every case drew
when the two still agreed pixel for pixel. The remaining two texels are WHERE the span
is sampled: `65536 - frac` puts the first sample on the pixel's right
edge in the perspective path, whose converter adds no half pixel
(F2FX_tp2d). Sampling at the centre (`32768 - frac`) reads 7 and 3 --
measured, and not committed: it resamples every textured pixel half a
pixel over (48% of the bench frame) and is a fourth deviation from mgl.

`tools/ref/bench.bmp` moved with it: 564 of 16000 pixels, single texels
scattered over textured walls, no geometry. A reference regenerated for
a sampling change is the fix, not a regression, but say so each time.

## The file layer: one name space, drivers that register by linking

`qgl/file.asm` is what mgl's `uar*` was: `qglFileOpen` /
`qglFileOpenBas` / `qglFileSize` / `qglFileRead` / `qglFileWrite` /
`qglFileClose` on `"assets.zip::member"` and on plain names, and a
handle indexes a table there rather than the caller declaring a `UAR`
it only ever passes back. Which containers exist is the drivers'
business: a row of `FsDrv` -- check claims an open file by its header,
find turns a member name into a position and a size, write is 0 where
the format cannot, which is why a zip member refuses `qglFileWrite`
and a plain file takes it.

**A driver registers by being linked.** `QGL_FSDRV` puts its row in
segment `QGLFS$M`; file.asm brackets that segment with `QGLFS$A` and
`QGLFS$Z` and every open walks the bracket -- the trick DOS C runtimes
use for their initialiser tables, since nothing else runs before
BASIC's `main`. The one contract is link order: MS LINK lays a class
out in the order it meets its segments, so file.obj has to come before
every driver or the row lands in front of the bracket, unseen --
`QRENDER.MAP` with zip.obj first shows `QGLFS$M` at 47E26 and `QGLFS$A`
at 47E32. jwlink sorts the class by name, which is why the native
suite cannot demonstrate it. Both Makefiles list `file` first,
`qglFileDrivers` reports what the walk finds, and `t05file` asserts the
count.

`qgl/zip.asm` is the first driver, and the only one.

**Stored only, and `tools/mkassets.py` now writes stored.** mgl's reader
carries an inflate and a window table to drive it; deleting that is most
of the point. A member by any other method is REFUSED rather than read
as though it were stored -- the difference between a missing file and
plausible garbage. The archive goes 236,287 -> 570,377 bytes on disk,
which nothing here is measured in.

It walks the LOCAL headers rather than the central directory. Reaching
the directory means scanning backwards for a signature past a comment of
unknown length; walking forward from byte 0 needs no such guess, and
sixteen members is sixteen seeks, once.

**The extra field is the trap, and our own archive cannot catch it.**
mkassets writes no extra field, so the skip past one is dead code
against every asset we ship -- and a reader that forgets it lands 28
bytes into the data of any archive from any other tool. `t25zip` builds
a second archive with the SYSTEM `zip -0`, which stores a 28-byte
timestamp field on each local header, and diffs a read of it against the
same member extracted by `unzip`. Mutation-checked: drop the
`lh_xtralen` term and that case alone goes red.

`lm.bmp` is still loaded by `uglNewBMPEx`, which reads the archive
through mgl's own uar. That keeps working because mgl reads stored
members; it goes when the lightmap atlas becomes a Surface.

One thing this cost that was not the reader's fault: `t25zip`'s clamp
case asks for 4096 bytes of a 320-byte member and its buffer was 256,
so the 320 it correctly returned ran into the reference buffer behind
it and the compare two cases later failed. The reader was right for an
hour of looking at it. Size a test's buffer for the largest read the
test makes, not for the one the case in front of you does.

## Retiring mgl, module by module: screen.bas

`screen.bas` was the first module cut over, and it was not a port so
much as a repair: the HUD's panels, bevels, bars and graphs went through
`uglRectF`/`uglHLine`/`uglPset` onto `h_dst_dc`, which has been a qgl
Surface since bd04500. mgl reads its scanline table at `DC_addrTB` (32)
where a Surface keeps `SF_addrTB` (38), so every HUD write landed at an
address read out of `zsf`/`zmode`, and a `-stats` run never returned:
no frame, no `error.log`, killed at 150s and at 600s under both cores.
`tools/check.sh --hud` is that run -- two ticks, unlit, so the fps still
reads 0 and every counter is fixed, byte-identical run to run -- against
`tools/ref/hud.bmp`.

What replaced what: `uglRectF` -> `qglDrFill`, `uglHLine`/`uglVLine` ->
`qglDrHline`/`qglDrVline`, `uglRect` -> `qglDrRect`, `uglLine` ->
`qglDrLine`, `uglPset` -> `qglSfPset`, `uglShadeRect` -> `qglDrShade`,
all with the same argument order and the same inclusive corners.
`uglSetVideoDC(8BIT, 320, 200)` for the loading screen is `qglVgaInit`,
and the loading palette goes in through `qglVgaPalette`. At the time of
that cut `vid_init` still set the real mode through mgl afterwards; it
no longer does -- see the mode/palette/shutdown cut below.

The palette is no longer read back from the DAC. `mkassets.py` writes
`pal.raw` (768 bytes of `color/palette.lmp`) beside the atlases,
`scr_pal_load` reads it with a plain OPEN, `scr_pal_fit` is mgl's
`uglPalBestFit` in BASIC (6-bit channels, green 59 / red 30 / blue 11
squared, entry 0 never chosen), and the screenshot writes those bytes.
That moved `tools/ref/bench.bmp` by ZERO pixels and 254 palette entries:
the old reference carried mgl's 6-bit DAC read-back (every value a
multiple of 4), the new one the 8-bit palette. `imgdiff` says `DIFFER 0
of 16000` for that case, which is not IDENTICAL and not a picture change
either.

Two headers stay included for their types alone: `ugl.bi` for `RECT`,
which `mouse.bi` names, and `mouse.bi` for `MOUSEINF`, which `q_env.bi`
names. Those go when the input state does. `RGB` is a reserved word to
BC -- `type Rgb` fails with "Identifier expected" at every use and
nowhere near the definition -- hence `PalRgb`.

## Retiring mgl: the mode, the palette and the exit

`uglInit` stays and `uglEnd` stays -- mgl still owns the paged-array
stores, the lightmap atlas and `uglBuildSurf`, and `uglNew` refuses
before `uglInit` has filled the DC-type table. What went is everything
between them that touched the adapter: `uglSetVideoDC` -> `qglVgaInit`,
`uglRectF` on the video DC -> `qglDrFill` on `qglVgaScreen()`,
`uglPalSet` -> `qglVgaPalette`, and all five `uglRestore` ->
`qglVgaShutdown`.

**The mode is now brought up TWICE and the restore has to survive that.**
`scr_begin_loading` enters 13h for the loading screen and `vid_init`
enters it again for the run, and `qglVgaInit` recorded the current mode
on every call -- so the second one saved 13h as the mode to go back to
and the program would have exited into a graphics screen, where BASIC's
error text is drawn as pixels and a failed run looks exactly like a slow
one. It records on the first call only. `t26mode` is the regression: set
mode 3, init, init, shutdown, and read `0040:0049` -- the adapter's own
record of the current mode, not qgl's byte. Mutation-checked.

**Paging is gone, not disabled.** `display.pages` and
`display.usepaging` are out of `stuff.ini`, `EnvType` and the parser,
with `uglSetVisPage`/`uglSetWrkPage`. It was mgl's, it needed mgl to own
the mode, and the ini has shipped `usepaging = no` throughout -- so the
branch was dead before this and impossible after. `vid_update` takes
only `g` now; the backbuffer is the only destination there is.

**`mouseInit` takes the qgl screen Surface, and that is safe for two
specific reasons, not by luck.** `mouseReset` reads `xMin`/`yMin`/
`xMax`/`yMax`, which sit at identical offsets in `DC` and `Surface` --
the two structs are the same up to the scanline table, which is where
they diverge (38 against 32). And every mgl routine that would DRAW
through that handle -- `cursorPut`, `bakgrdGet`, `bakgrdPut`, the ISR's
own move path -- returns early while the cursor is hidden, which it is
from `mouseInit` onward because nothing here calls `mouseShow`. Adding a
`mouseShow` would put mgl's filler on a Surface at `DC_addrTB`, which is
the wandering-write bug in this file's other notes.

Dropping `Game.pal` and three `EnvType` fields moved `Game` under the C
side by eight bytes, and both hardcoded offsets -- `GAME_VIS_OFFSET` in
`r_walk.c`, `GAME_DLIGHT_OFFSET` in `sb_build.c` -- had to follow. The
startup assertion is what said so, by name and with the measured number
to paste in. Removing a field ahead of `vis` is a C change too.

`g.pal` and its `uglPalLoad` are gone with the rest: `pal.raw` and
`scr_pal_load` have been the palette since the screen.bas cut, and
`scr_hud_colors` is now `scr_pal_install` -- it puts those bytes in the
DAC and then best-fits the overlay against them, in that order, because
the second cannot run before the first.

## Retiring mgl: the paged-array stores

`uglArrLoad`/`uglArrMap` are `qglArLoadBas`/`qglArMap` in model.bas,
r_bsp.bas and pl_move.bas -- nodes, leaves and clipnodes, MEM first
and EMS on a refusal, as before. `qglArLoad` streams the member in
through `qglArWin`, a page payload at a time, so the seam padding is
the store's own arithmetic and not the loader's.

**The three stores came up all zeros, and every instrument agreed.**
Handles live, counts right, descriptors bound to the right segments,
the read reporting every byte -- and 17,820 bytes of nothing. `-qglarr`
passed on the same loader. The loader tested `qglArWin`'s pointer with
`or ax, dx` and stored `ax` AFTERWARDS, so the destination offset was
`0 | segment` and every read landed that many bytes past the window:
in the renderer 53,961 bytes past a 22,926-byte block, which is
somebody else's memory. What found it was the native `t27arload`
dumping the MEM store: its contents were the file shifted by exactly
7,459 bytes, and 7,459 is 0x1D23, the window's segment.

**`-qglarr` passed because it could not fail.** Its loader arm freed
the hand-filled EMS store first, and the loader's store got the same
pages back still holding the fixture; a loader writing nowhere near
the window read back a perfect copy. The hand-filled store now stays
alive across the loader arm. Both tests were watched to fail with the
bug put back.

## Retiring mgl: every EMS page goes through qglGemMap

mgl's `emsAlloc`/`emsMapEx`/`emsFree` and `uglNew(UGL.EMS)`/`uglMapEx`
map nothing now. The colormap, the luxel atlas and the geometry store
are `qglArLoadBas` EMS stores of one row per record -- 16384x1, 8192xN,
8192xN -- and `mod_cm_map`/`mod_lm_map`/`mod_geom_map` are `qglArWin`.
The model's vertex page is raw `qglGemAlloc`/`qglGemMap`, and ar.asm's
EMS backend is `qglGem*` too, so the native suite no longer needs an
`emsstub`. `lm.bmp` is `lm.bin`: the same bytes, top down, without a
header to un-flip.

**The per-slot record moved into `qglGemMap`.** mgl's `emsMapEx` kept
one and ar.asm leaned on it, asking for its page on every access; a
store keeping its own "I mapped page N here" is wrong the moment another
store sharing the slot takes the window. `qglGemFree` forgets the handle,
because EMM hands a freed number straight back to the next allocation
and a surviving record would answer its first map with the old page.
`t28gemmap` hooks INT 67h and counts function 44h, so "no remap" is a
number: the same page again costs none, a new page one, a slot 4 request
none and a 0. Both halves were watched to fail with the code put back.

**Two `@@:` in one loop is one label too many.** `qglGemFree`'s forget
loop ended `loop @B` with an `@@:` in front of `add bx, 2`, so it went
back to that one and only slot 0 was ever asked. Named labels for
anything a second jump targets -- the plan file says so, and this is why.

**Arm the case before trusting the mutant.** The first version freed
with page 1 on record and then asked for page 0, so the map issued
whatever the free had forgotten, and the forgetless mutant passed. The
test now maps page 0 before the free -- and with that armed, the real
code failed too, which is how the loop above was found.

## Retiring mgl: memory and the selftest that could not fail

`memCopy` is `qglMemCopy` (d_surf.bas, sb_build.c), `memAvail` is
`qglMemAvail(QGL_MEM_LARGEST)` in the memtrace and gone from bench.txt
-- `qgl_avail` had been beside it saying 64 bytes where it said
246,848, and the difference is BASIC's heap, not free memory --
`emsCheck` is `qglGemFrame`, and `-dumptex` reads its palette from
`pal.raw` instead of the DAC. The timer is `qglTmrInit`/`qglTmrTicks`/
`qglTmrShutdown`, `t29tmr` proving the BIOS chain and the restore, and
`qglTmrCycles` is the RDTSC that came from the sound mixer.
`uglInit`/`uglEnd` are gone from production too: what they did for us
was link the upper memory blocks and set strategy 81h, which is
`qglMemInit`/`qglMemShutdown` now, `t31meminit` reading both back
through DOS's own 58h queries. `-qgldiff` brings mgl up and ends it
itself, being a differential against mgl by design. The music player
-- snd.bas, `sound.enabled`, `mymod`, the HUD's VU meters -- is deleted
rather than ported, and with it the last mgl call in production; the
hang its init had is gone with it. `Env.sound` sat ahead of `vis`, so
both C offsets moved by two. The camera's matrices are `qglM4Persp`/`qglM4LookAt`/
`qglM4Conc` in `qgl/m4.asm`, mgl's `mdu3d.asm` FPU sequence kept
instruction for instruction because `tools/ref/bench.bmp` is a
byte-for-byte reference and a rounding that moves by an ulp moves it;
`t32m4` checks them on values a real4 holds exactly, and the bench
staying IDENTICAL is the real test. `u3dVector3f` folded into bspfile.bi's own
`Vec3`, the same three singles; `u3dMtrx` is `Mat4` beside it; `u3d.bi`
is out with the rest. `ugluCubicBez3D` is `v_bezier` in view.bas, the same
forward differencing in BASIC, and `ugl.bi`, `uglu.bi`, `dos.bi`,
`arch.bi`, `font.bi`, `pal.bi` and `ems.bi` are out of every module
but the three oracles; `PalRgb` moved to bspfile.bi so mod_tex reads
pal.raw through it instead of mgl's `tRGB`.

**And then the oracles' mgl halves and the library itself.** `-qgldiff`
keeps the exact-answer oracle and pins coverage and texel distance to
the figures of the last differential run; `-qglarr`'s reference is the
fixture read straight into a BASIC array, and its hand-fill maps pages
through `qglGemMap`. `UGLV.LIB` is off the LINK line with mgl's `U3D.OBJ`,
no tool mounts the mgl tree, `ugl-patch/` and the native mgl build are
deleted, and the qgl benches that raced mgl went with them. The bench
stayed IDENTICAL through every step.

**The depth buffer is conventional memory first, EMS on a refusal.**
With the per-texture DCs, the font DCs and the library gone the
memtrace shows 303,648 bytes in BASIC's far heap before it and 268,080
after: 160x100 of depth is 32,000 bytes, and a page the fillers no
longer map. 320x200 wants 128,000 and takes the EMS path as before.
The bench stayed IDENTICAL. A six-run interleaved campath A/B against
the EMS build was NOT a measurement: another emulator was running on
the host and the spread was 15ms an arm on a 54ms frame, medians 53.9
against 53.7. Repeat it on a quiet host before quoting a speed.

**`sc_selftest` wrote its row through mgl and read it back through
mgl, on a qgl Surface.** `uglRowWriteBuff`/`uglRowRead` take the
scanline table at `DC_addrTB`, 32, where a Surface keeps it at 38, so
both went through an address read out of the depth fields -- the same
wrong address, so the bytes came back equal and the test said 1 while
32 bytes landed somewhere else on every bench run. The write goes
through `qglSfWrRow` now and the readback through `qglSfPget`, which is
the surface's own pixel path and cannot agree with a write that missed
it; `check.sh` reads `sc_test` and wants 1. Aiming the write at row
126 reads -12 through that gate. Every other `-1..-55` was the same
instrument reporting on itself, so treat a green selftest as one
measurement, not fifty-five.

## Retiring mgl: the keyboard and the mouse

`qgl/kbd.asm` is mgl's INT 9 hook with the same table -- a word per
scancode, -1 down, word 0 the last code, `Keys` in `in.bi` keeping
TKBD's names so `g.env.keyboard.w` reads as before. `qgl/mouse.asm`
keeps its own cursor from the driver's mickeys, one pixel each, clipped
to the mode, and `qglMousePos` places it -- which is how the spawn yaw
and a teleport's facing are applied, so the clip range is part of the
picture. Both unhook on either exit; mgl did it from `uglEnd`'s queue.

**`t30in` drives the keyboard hook through the emulator, not a call.**
`run1.sh` types whatever a test's `keys` file says a second in, via
DOSBox-X's `AUTOTYPE`, and the test spins on the table until scancode
1Eh comes down and goes up, bounded by the BIOS tick. The mouse cannot
be moved headlessly, so `qglMouseEvent` is public and the test calls it
the way the driver does, mickeys in `si:di` and buttons in `bx`. Mutated
three ways: index by scancode instead of scancode*2, no word 0, no clip.

`AUTOTYPE` feeds the keyboard buffer from a host thread with no lock
against the emulation thread, and under load -- three gates and a
windowed run at once -- it dropped one release: down seen, up never,
the hook restored fine. The test types the key three times now, so a
later pair still shows both transitions; the assertion is unchanged.

**A multi-line mutation pattern misses a CRLF file.** The first clip
mutation "passed": its pattern spanned two lines with `\n` between,
matched nothing in a `\r\n` file, the assert failed inside a helper
the script did not stop on, and the unmutated source ran green. One
line per pattern, or match the line ending.

## Portal culling and the composite HUD

Ported from `alim/portal-culling`, which was written against mgl and the
flat `src/` layout; `docs/portal-culling.md` is its design note.

**The flood refines the PVS, so on and off must draw the same picture.**
`tools/mkportals.py` rebuilds the portals a compiled BSP no longer
carries and `mkassets.py` ships them as `portalidx.bld`/`portalref.bld`
in assets.zip, held to the PVS-subset check on every build -- a map whose
portals do not cover the PVS fails `make assets` rather than culling what
it should have drawn. `r_portal.c` floods from the camera's leaf every
frame with a shrinking screen rectangle and writes `pvs_now`, which the
walk reads instead of `pvs_buffer_b`; a negative return is a bail and the
PVS is copied through unchanged. `tools/check.sh --portal` is the gate:
the bench frame with and without `-noportal` must be IDENTICAL, and the
on run must report `portal_culled` above zero, or the identity is
vacuous. P toggles it, O outlines the portals the flood went through,
`-ptwire` starts with the outlines on.

**It is a net loss on dm3ish and is on by default anyway.** The branch
measured the campath at +4.03 ms with the flood on: the map is small and
the PVS already tight, so the flood costs more than the leaves it cuts
save. It is here for the maps where that reverses, and `-noportal` is
the A/B.

**`-comp` writes video memory once a frame.** The view is scaled into a
mode-sized conventional Surface, `scr_draw_hud` draws onto that at 1:1
-- it takes the destination's width and height now, not `x_res`/`y_res`
-- and one `qglDrBlit` carries the result to the screen. Without it the
overlay is drawn into the render target and magnified with the view.
`-bench`'s BENCH.BMP is still the render target, so under `-comp` it
carries no overlay; the HUD gate runs without `-comp` for that reason.

**Adding a field to `Env` or `RenderState` moves `GAME_VIS_OFFSET` and
`GAME_DLIGHT_OFFSET`**, in r_walk.c and sb_build.c: 4958/4942 became
4970/4954 here for `h_comp_dc`, three flags and `rdr.portal`. The
startup assertion says so, with the number to paste in.

## The game layer

The fight lives in `pl_move.bas` because it traces, and `pl_trace`'s
clip buffer is that module's `dim shared`: the shotgun (`pl_fire`), the
soldiers' think (`mdl_think`, 10 Hz on `anim_time`), the pickups and the
reset. The player's side of it is `Game.fight`, a `PlayerCombat` placed
AFTER `vis` so the C offsets stayed put; `MdlEnt` is an array element
and grows freely.

**The vertex page is the frame budget.** One EMS page holds the whole
model: 170 vertices x 3 bytes = 510 a frame, 32 frames = 16,320 of
16,384; the triangles are the page after the vertices, read by
`d_alias.c` through `CM_SLOT`. `stand,run,death,pain` fills it; the soldier's shoot set does
not fit, so a volley has no animation. `mkmdl.py` emits the frameset in
the order given and writes each set's count into the `.geo` header --
stand, run, death, pain, attack, by position -- so the Makefile's list
IS the frame layout and `MdlState` carries it. The knight's 108
vertices leave room for `attackb`, which is why it is the second
monster: `MdlEnt.kind` picks `g.mdl` or `g.kmdl`, every other spawn
is a knight, and `mdl_think` takes the `MdlState` it animates.

**The dog is the third kind, and it bites on the run.** dog.mdl has 236
vertices, so the page holds 23 frames: `stand:1,run,death,pain:1` --
mkmdl.py's `:n` keeps the first n of a set -- and no attack set. So
`mdl_think` bites from the run cycle: within 100 units with a clear
line, `(r+r+r)*8` once per 0.8 seconds, dog.qc's cycle without its
frames. Its run steps are dog_run's 16..64 a frame, which is why a dog
closes so fast. The leap is CheckDogJump and dog_leap2: 80 to 150
level units off with the player's body at its height, it faces them
and takes 300 forward and 200 up as a velocity, the one MOVETYPE_STEP
in the air here -- gravity at each 10 Hz think, a trace along the
velocity, the player's box met above 300 u/s bitten for 10 + 10 *
random once (Dog_JumpTouch), a stop while falling the ground and the
run again. It has no leap frames on the page, so it flies through its
run cycle. e1m1 on easy has one, at (88,1520,-200); the e1m1 gate counts
it by kind, and its tenth arm stands 120 units from it and wants
`pl_leaps` above zero. `MDL_MAXV` is 236 in
d_mdl.bas and d_alias.c both: at 191 the dog loaded past the BASIC
check and overran the C scratch, and e1m1 died in `runtime error 14`,
out of string space -- a corrupted near heap, not a full one. The
memtrace prints `fre("")` as its third column now, so the next
error 14 can be told from a real exhaustion in one look.

**The ogre and the demon are kinds 3 and 4**, `mon()` in main.bas
holding one MdlState a kind. The ogre's page is `stand:1,run,death,
pain:3,shoot` (169 vertices, 32 frames): it saws on the run within
ai_melee's 100, swing5..11's `(r+r+r)*4` every other think with no saw frames
on the page, and past melee range throws OgreFireGrenade from shoot
frame 2 at CheckAttack's 0.2 near and 0.05 mid. The grenade is a
`Spike` with `grenade` set: `pl_nails_tick` flies it under gravity,
bounces it off hull 1 at ClipVelocity's 1.5, and blows it up on the
player's box or at 2.5 s -- T_RadiusDamage's 40 less half the
distance, to the player alone. The demon's page is `stand:1,run,
death,pain:3,attacka` (143, 34 of 38): attacka's charge strides with
the claws on frames 5 and 11, 10 + 5 * random within 100, and
CheckDemonJump's leap from 100 to 200 level (past 200 one think in
ten), 600 forward and 250 up, 40 + 10 * random past 400 u/s -- the
dog's leap state with its own numbers, and both draw the run cycle
in the air now; the dog drew its last pain frame. e1m2 on easy has
four ogres and a demon; the e1m2 gate's fifth arm stands 146 units
before the ogre at (1790,-146) for six seconds and wants a bite with
the ogre hunting -- health under 100 or a death, since it kills in
that time and the respawn reads 100.

**The zombie and the wizard are kinds 5 and 6**, their four sounds at
`SND_MON2` since the table after the first five kinds' is taken;
`mdl_say` picks. The zombie's page is `stand:1,run:8,death,paina:8,
atta` (177 vertices, 30 frames): no death set, since zombie_die is a
gib -- a dead one with `ndeath` 0 is not drawn -- and no paine, so a
dropped one lies on paina's last frame. zombie_pain's rules are in
`combat_damage_monster`: the health back to 60 after every hit, so only one hit of
60 kills (the SSG's 56 does not, as in Quake); under 9 ignored; 25 or
more, or any hit, sets `pain_finished` -- three seconds down, or one
flinching -- and nothing flinches it again before. CheckAttack with a
th_missile alone (0.9 / 0.4 / 0.1) starts `atta`, and frame 13 throws
the gib: a `Spike` with `grenade` and `gib` set, ZombieFireGrenade's
600 toward the player with 200 up, that bites 10 where it lands on
the player's box, stops dead on a wall and goes out at 2.5 s with no
blast. The wizard (80 vertices, all 54 frames) flies: `mdl_flystep` is
SV_movestep's FL_FLY case -- a straight trace, 8 up or down toward 30
to 40 above the player while hunting, once more level when blocked --
and `mdl_spawn` does not drop it to the floor; WizardCheckAttack's 0.9
/ 0.6 / 0.2 starts `magatt`, frames 3 and 8 fire Wiz_FastFire's spikes
(hostile, 600, 9), two seconds to be ready. Its corpse stays in the
air. `tools/check.sh --e1m3`'s third and fourth arms stand inside
RANGE_MELEE of one of each -- outside it a wanderer must be facing the
player, and the zombie at (800,-216) wanders off in the first second --
and want health under 100 and the kind hunting.

**BC reads a user FUNCTION inside a CALL-less SUB's argument list as an
array.** `snd_play g, mdl_snd( ent.kind, 1 ), ent.pos` was `Argument-
count mismatch` at every site, the DECLARE in place and the function
compiling; `l = mdl_snd( ent.kind, 1 )` on the line before compiled.
Assign to a local first, or make the helper a SUB, as `mdl_say` is.

**A monster that only looks while standing never sees anyone.** The
port called FindTarget from `ai_stand` alone; id's `ai_walk` calls it
every frame too, and a player who fired within the second is noticed
from behind at RANGE_NEAR (`show_hostile`, W_Attack's `time + 1`;
RANGE_MID has that term commented out in ai.qc and still wants
infront). Before this, ten seconds beside a knight cost no health at
all: all eight were wandering, and a wanderer never looked.

**The first bench run in which anything hurt the player crashed with
`runtime error 9 at line 0`.** `scr_pal_shift`'s shifted palette was a
module-level `dim shared` under `'$DYNAMIC` in screen.bas, allocated on
first use behind a `ubound()` test -- and `ubound` of an array that was
never allocated is itself error 9. It is `redim`med in `scr_pal_load`
now, and `tools/check.sh --fight` stands ten seconds beside the knight
at (464,-40): health must drop and nothing may crash.

**Finding a `runtime error 9 at line 0` with the debugger.** Line 0 is
all BC says without line numbers, and `ON ERROR` swallows the address.
What worked: a conf whose autoexec stops at `C:\>` and `core=normal`
(breakpoints do not fire on the dynamic core), `dosbox_load_and_run_to`
the program with `B$RUNERR` as the name -- it stops at the entry with
symbols loaded -- then `bp_set B$RUNERR` and continue. At the stop
`[SP+2]` is the far return address into the BASIC module; subtract
`loadLinear` and the MAP's publics name the procedure. Ack the stale
`process_exit` first or the shell command is refused.

**`make assets` will not rebuild the model for a frameset change** --
the rule depends on the pak and the tool, not the Makefile. Delete
`data/assets/soldier.geo` first, and check `nframe` in the header.

**The HUD's footer draws under `-nostats` too**, so the status line and
the crosshair moved `tools/ref/bench.bmp` and `hud.bmp` -- by the text
rows and four centre pixels, checked with a bounding box each time.
The state messages do not: a bench, ticks or campath run starts in
`GS_PLAY`, where nothing is drawn but the crosshair.

**`cam.look_at` is a POINT once `v_update_camera` returns**, one unit
from `cam.pos` in the direction of the view; it is a direction only
inside that routine. The first shotgun aimed along the point and every
pellet flew from the origin; the view weapon built from it turned by a
position and drew nothing -- 0 triangles, `vmdl_loaded -1`, and the
memtrace's cap of 20 marks had hidden that the second `mdl_load` got
through at all. Subtract `cam.pos` first. `MEM_MARKS` is 40 now and
`mdl_load` names the step it stops at.

**The view weapon is v_shot from the PAK**, `mdl_draw_view`: the model
at the eye, turned by the view's yaw and pitch -- `mdl_draw_tris` takes
the pitch as a cos/sin pair, applied in model space before the yaw --
and drawn last with depth off, as Quake does. `shot2..` play at 10 Hz
from the shot. `-noview` leaves it out, which the model gate needs:
its away tick asks that the soldiers add nothing. Both references
carry the gun now.

**`weapon_supershotgun` is v_shot2 and W_FireSuperShotgun**: the
pickup is an item kind that puts it in hand with five shells, `1` and
`2` choose (W_ChangeWeapon, `pl_select_weapon`), and a shot is 14
pellets at 0.14 by 0.08 for two shells, 0.7 to be ready -- or the
shotgun's six with one shell left. `fight.items` holds the weapons
owned as bits and `fight.weapon` the one in hand. mkmdl.py writes
`<name>.vtx` and `<name>.skn` beside the `.geo` now: the old
`<name[:5]>vtx.bin` cut v_shot and v_shot2 to the same v_sho. In hand
it shows as a band four rows deep above the bar, 40 pixels: its top
edge is 6 units under the eye 14 forward, and this view's centre is
the frame's, where Quake's is the centre of the rows above the bar --
8 rows higher on 100. Rendering above the bar instead of under it would
move every reference; not done.

**`weapon_nailgun` is v_nail and W_FireSpikes**: the pickup brings 30
nails and `item_spikes` 25 or 50, capped at 200; `3` chooses it; a
nail every 0.2 s leaves 16 up and 4 to alternating sides at 1000 u/s
and lives in `nail()`, `PL_NAILS_MAX` of them (`Spike`, no gravity:
MOVETYPE_FLYMISSILE). `pl_nails_tick` steps each along its velocity
through `pl_trace` -- hull 1, so a wall stops it 16 early, as the
pellets -- bites the first monster on the way for 9 (spike_touch),
fires a shootable trigger or secret door, and ends it on a wall or at
six seconds. In flight it is a two-unit box, no spike.mdl. The bar's
ammo is the weapon in hand's, `SB_NAILS` cell 19. The e1m1 gate's
fourteenth arm stands on the nailgun with fire held a second and wants
27 nails: 30 less three shots.

**`weapon_grenadelauncher` is v_rock and W_FireGrenade**: the pickup
brings five rockets and `item_rockets` 5 or 10, capped at 100; `4`
chooses it; a grenade every 0.6 s leaves at 600 along the aim and 200
up, the ogre's `Spike` with `grenade` set and `dmg` 120 -- the ogre's
carries its own 40 in `dmg` now. `pl_grenade_tick` is either side's
MOVETYPE_BOUNCE, split out of `pl_nails_tick`: what it can hurt over
the step blows it up (the player's box for an ogre's, a monster's for
the player's), else gravity, a hull-1 trace, the bounce; the fuse is
2.5 s and `pl_grenade_explode` is T_RadiusDamage to the player and,
for the player's own, every monster standing. `SB_ROCKET` is cell 20
of the bar's band, the last that fits in 512. `tools/check.sh --e1m3`'s
second arm stands on e1m3's launcher and fires at the ceiling (`-pitch
89` looks up) for four seconds: rockets under 5 and a blast felt from
the grenades falling back (health -70 the first time,
so a death counts too). The launcher's model and code ran e1m3 out
of string space (3.2K after depth, error 14 in the first frame); cut:
`SC_NBLK` 1024 to 512, the bound its own comment gave (7K), `pt_idx`
freed when the portal table bails (3K on e1m3), `item()`'s backpack
room the map's monster count instead of 48 (0.8K) -- 14.5K after
depth on e1m3.

**`weapon_supernailgun` and `weapon_rocketlauncher` are v_nail2 and
v_rock2**, `5` and `6`, `PL_IT_SNG` and `PL_IT_RL`. The super nailgun
is W_FireSuperSpikes through `pl_fire_nail`: with two nails a shot
from the middle for 18 and weapons/spike2, with one the nailgun's
nail; a nail carries its own `dmg` now. `pl_fire_rocket` is
W_FireRocket, a `Spike` with `rocket` set, 1000 straight along the
aim from 8 before the origin, 0.8 to be ready; `pl_nails_tick` flies
it as a nail and where it stops -- a monster hit for 100 + 20 *
random, the box, a switch, a wall -- `pl_grenade_explode` blasts 120
from there (T_MissileTouch); one that hits nothing is gone at 5 s,
unexploded, as SUB_Remove leaves it. Both quad in the tick. And
`pl_nail_free` clears the flags of the slot it hands out: a nail
fired into a spent grenade's slot kept `grenade` and bounced. The
e1m4 gate's second arm stands on the map's super nailgun with fire
held a second and wants weapon 64 and 49 nails, its 30 and the pack
beside it less three shots; e1m4's rocket launcher is deathmatch-only, so the launcher's arm
waits for e1m5. That arm found `ent_items_drop` tracing the origin
itself through hull 1: an item the map put within 24 of its floor
started inside the grown solid, and where a room lay under it within
256 -- the super nailgun's, 192 down -- it fell through, while one
dropped from higher stopped 24 above the floor and floated. It
traces from `PL_FEET` above the origin now, SV_ClipMoveToEntity's
offset of the hull's clip_mins against the item's mins, and lands
the origin on the floor.
The view models load lazily: v_shot at host_init and the rest the
first time their weapon is in hand (`host_view_load`, from the tick).
Each costs the far heap 600 bytes, and with all six loaded at init
e1m4 read 3,984 after the depth buffer and died in error 14 at the
bench write -- which `run_frame` took as a run, since a 54-byte
header-only BENCH.BMP existed; it wants 1,000 bytes now.

**The shambler is kind 7**, `stand:1,run,death,pain,magic` on its
page (144 vertices, 36 of the 37 frames that fit), health 600. It
smashes on the run in RANGE_MELEE, sham_smash10's (r+r+r)*40 landed
as the swing starts, once per 1.2 s, and past that -- ready, a clear
line, within 600 -- runs the magic set: `mdl_bolt` on frames 5, 8
and 9 is CastLightning, a line from 40 up toward 16 above the
player's origin traced 600 through the world and LightningDamage's 10
where it crosses the player's box; nothing is drawn for it. sham_pain
flinches only when random() * 400 is under the hit. Its four sounds
sit at SND_MON2 + 8; melee1, smack and sboom follow the rocket's
sgun1. e1m5 is in `MAPS`: `tools/check.sh --e1m5` is its spawn
frame, the shambler arm (128 units before it, past the shut door that
hides it from farther off: the lightning, then the smash, a hit and
hunting) and the rocket launcher arm (on it, one rocket fired straight
down). That rocket stops on hull 1's floor at the origin -- a rocket
is traced as the nails are, and id's 8 forward would start one aimed
at the floor inside the grown solid and carry it through unstopped,
so it leaves from the origin -- and its blast is T_RadiusDamage's,
halved for its owner (`head == attacker`): 59 at the feet, health 41,
as the gate reads. The rule reaches the grenade launcher too.

**e1m6, e1m7 and e1m8 are in `MAPS`**, each smaller than e1m4 in every
lump, with spawn-frame gates. `sv_gravity` is the map's now:
`fight.gravity`, from the ents header, which mkassets sets to 100 for
e1m8 as world.qc does and 800 otherwise; the player's fall, a leap
and a grenade all read it. The e1m8 gate holds jump from the spawn,
which hangs 630 over its floor, and at tick 400 wants the body over
-600: the jump goes 364 up by v^2/2g against 48 under 800. `peak_z`
read 0 on any map under z 0 -- nothing set it -- so `pl_init` starts
it at the spawn, and the gate wants e1m8's -104. Not ported:
`func_wall` draws as any solid brush model.

**Chthon is a trigger, unseen.** boss.mdl has 325 vertices and 106
frames -- seven pages of vertices against `MDL_MAXV`'s 236 and the one
page a model gets -- so `monster_boss` is `ENT_TRIG_BOSS`: the
rune (`item_sigil`, "You got the rune!", its target fired as any
pickup's) wakes him to ARMED with boss_awake's health, 1 on easy and 3
above; `event_lightning` is `ENT_TRIG_BOLT`, lightning_use with both
electrode doors up -- the doors whose target is `lightning`, their
indices in the bolt's `ofs_out` -- taking a point, and at none his
target fires: the relay that opens e1m7's exit. `trigger_relay` is
`ENT_TRIG_RELAY`, a use passed on at once.

**`misc_fireball` is a trigger kind too**, `ENT_TRIG_FIREBALL`: its
origin in `mins`, the next ball in 0..5 s and then every 3..8
(fire_fly), `pl_traps_tick` sending it. The lava ball is a hostile
`Spike` with `toss` set: walked as a point through hull 0 as a trap's
spike is, since it leaves from inside the lava, under the map's
gravity, gone on a wall or at five seconds, 20 to the player's box
(fire_touch); monsters are not bitten. id's default speed is
`self.speed == 1000`, a compare, so an unset one is 0; every map sets
it. Drawn as an 8-unit red and yellow box. Nothing armed an emitter
until `ent_move_trigs` ran fire_fly's timer, so e1m6 threw no ball.
`tools/test-fireball.sh` wants 15 in ten seconds, from the summary's
`balls=`: the emitters' `left`.
e1m7's gate stands on the rune and wants Chthon ARMED; the bolt is
read, not run -- its buttons cannot be pressed headlessly. The chain
closes: e1m7 exits to start, e1m8 to e1m5.

**`trigger_monsterjump` is a trigger kind**, `ENT_TRIG_MJUMP`, its
throw in `ofs_out`: movedir times speed, and height up, 200 each
unless the map says. `ent_monster_jump` runs for a monster that moved
that tick, as id's touch runs at a link, against id's box for its
kind. A walker in it takes the push; one on the ground goes into the
leap state with the throw, `leapt` at `MDL_THROWN` so it bites
nothing. The wizard is left alone: qcc binds `&` tighter than `!=`,
so id's flags test is the walker test it reads as. A thrown monster
runs on landing where id's keeps its think, and cannot attack in the
air. Its brush used to be hidden and nothing more, though e1m6's easy
ogre behind the t44 doors reaches it once their button opens them.
`tools/test-monsterjump.sh` presses that button and wants `mjumps=`,
throws that rose, above 0.

**start, the hub, is in `MAPS`** with its spawn frame as the gate.
Its gates are `EntsGate` records the loader decides by
`World.serverflags`: a `func_episodegate` stands only for its rune
held, the `func_bossgate` until all four are, and a gone one is draw
off, solid off. The rune ORs its spawnflags' low four bits into
`Fight.serverflags`, CARRY.BIN takes them on, and any flag spawns the
player at `info_player_start2` (the header's second spawn) and, on
start, drops the carried kit. `-sflags N` sets them when nothing is
carried; `tools/test-serverflags.sh`.
`trigger_onlyregistered` is OnlyRegisteredTouch's shareware branch:
a multiple with its message and no target, so the episode 2 to 4
doors it would open stay shut and "For registered users only!" is
said every two seconds -- the gate's second frame stands in the
episode 2 hall's. `trigger_setskill` is `ENT_TRIG_SKILL`, the skill
the next map loads by; every changelevel here names a map the
shareware PAK has only for e1m1.

**The powerups are thirty-second clocks on `fight`**: the quad
(`quad_until`) makes every pellet and nail four times (T_Damage's
super_damage_finished), the envirosuit (`suit_until`) turns the slime
off and lava down to a bite a second, the pentagram (`pent_until`)
makes `combat_damage_player` return after the armor's share is taken --
T_Damage's invincible_finished, protect3 two seconds apart while it
does; returning first left armor unworn (`tools/test-fall.sh` arm 3).
The ring (`invis_until`) hides the player from FindTarget's look at
the stand and the walk, not from a hunt already on or a hurt monster,
drops PainSound and the view model, and whispers items/inv3 every
1..4 s. `-noitems` only hides pickups: they are still taken, so a
control for a pickup starts off it. e1m8's gate stands on its own. Slime and lava hurt at all now,
`pl_env_damage`, PlayerPreThink's 4 and 10 a water level; e1m1 has 112
slime leaves and no lava. Drowning is WaterMove's: `PL_AIR` seconds
of breath from the last tick with the eyes out, then a bite a second,
two more each time and back to 10 past 15; `tools/check.sh --drown`
sits under dm3ish's pool fifteen seconds and wants health 82. `misc_explobox` is an item that is shot, not
taken: 20 health against the pellets and nails (`pl_box_ray`, its brush
plus the touch slack), then barrel_explode -- 160 less half the
distance to the player through the armor and to every monster standing,
CanDamage's line not asked. Drawn as its 30x30x62 box, and solid:
`pl_trace.c` keeps a table of up to eight boxes (`pl_boxes_sync`) and
sweeps every trace against each one grown by the player's hull-1 box,
so the monsters walk round it too. Four e1m1 arms: the quad taken,
two seconds in the slime pool, the box shot from 100 units.

**Pickups are the map's own `item_health`/`item_shells`**, shipped in
`ents.bin` after the hides, dropped to the hull floor at load, and drawn
as flat boxes by `mdl_draw_box` in `d_alias.c` -- six quads, no clip
beyond "any corner behind the near plane drops the box", which at ten
units wide means the player is in it. A box far down the hall is in
the bench frame: 24 pixels at the centre. `item_armor1` and `item_armor2`
are two more kinds, 100 at 0.3 and 150 at 0.6: armor_touch takes one
only when type * value beats what is worn, and `combat_damage_player` is T_Damage's
split, the armor taking ceil(type * damage) and losing its type with its
last point. That was `-(long)-x`, and a cast chops (`F_FTOL@`), so it
was trunc: four drowning bites under red armor saved 21 where id saves 24.
It is `ceil()` of a product stored in a float first, as QuakeC's is: at
80 bits 0.8 * 10 is 8.00000012 and would save 9. `item_armorInv`
is 200 at 0.8 and `item_cells` 6, or 12 with WEAPON_BIG2, to 100; both
are deathmatch-only on start, so their arms stand on dm3ish, and the
cells `CARRY.BIN` carries are untested for the same reason.

**A landing reads the fall speed from before the floor.** `pl_gravity`
sees ground a tick after `pl_step_move`'s slide has clipped `vel.z` to 0,
so `pl_land` testing `vel.z` never thudded or hurt: a 426-unit fall left
health 100. `fall_vz` is id's `jump_flag`, stored before gravity on each
airborne tick and zeroed wherever the velocity is. `tools/test-fall.sh`. The bar shows the icon and the count at 0 and 24 while any is
worn -- Quake draws a 0 there too, left out so the references stand.
`trigger_secret` is a once with "You found a secret area!" unless the map
says, counted in `fight.secrets`; six on e1m1. The e1m1 gate's last two
arms stand in `*44` and on the green armor.

**The b_*.bsp pickups are textured crates.** Health, shells, nails,
rockets and the exploding box are Quake's own brush models, six-face
boxes from the origin with two to five 32-pixel textures; mkassets
(`crate_src`, given the PAK as its fifth argument) cuts each one the
map uses into a `CrateModel` in ents.bin -- the size and five faces,
the bottom left on the floor, each an atlas id, a +N frame count and
four corners as (axis bits, u*32, v*32) bytes -- and appends the
textures to the map's atlas after its own, frames consecutive, so
`mod_tex_shaded` aims at them as at any cell (q_map.bi's `ofs(1023)`
holds 256; e1m1 uses 81 + 23). `mdl_draw_crate` in d_alias.c draws
the five quads through the mip views, affine as the models are, the
chain stepped at 10 Hz; an item's `crate` is -1 for the kinds Quake
draws as alias models -- armor, keys, weapons, powerups -- which stay
flat spinning boxes. A crate stands still, centred on the origin as
the touch and the exploding box's trace already are, where Quake's
spans origin to origin + size. Its cost is the record, 92 bytes a
model used, and the cells in EMS. Every image reference moved with
it, the crates being in most of them.

**The status bar is Quake's own, out of gfx.wad.** `tools/mkgfx.py`
writes `sbar.raw` and `sbnum.raw` -- the bar, the digits, the shells
icon and the five faces -- with the qpic's 255 kept transparent: the bar
is textured differently under every slot, so nothing can be composited
offline. Everything lives in ONE EMS surface, 512 wide because an EMS
row must divide 16K: the untouched bar in rows 0..23, the twenty
cells side by side in 24..47, the composed bar in 48..71, which a
320-wide view aimed at row 48 hands to the blit. `scr_sbar_paint`
copies the bar band over the working band and pokes each cell's opaque
pixels through the read window (slot 0) and the write window (slot 1),
only when health, shells or the face change; `scr_sbar_draw` scales the
result into the bottom twelfth of the render target. The far-heap
tables this replaced -- rows, cells, spans -- were 28K, which e1m1 does
not have. Traps: the wad's names are uppercase; a `const SBAR_FACE` and
a `dim sbar_face` are the same name to BC; the scaled blitter's 8.8 step
was a 16-bit divide that truncated any source wider than 255
(`t15blit` case 7); a `get` reads the whole fixed string, so a 576-byte
cell buffer swallows 576 bytes of a 320-byte bar row; and `p \ 65536`
on a far pointer truncates TOWARDS ZERO, so a window at E000h -- a
negative long -- came out one paragraph high and every digit was read
sixteen bytes off. `scr_seg_of` subtracts the low half first.

**Sound is a Sound Blaster at 220h, polled.** `qgl/dsp.asm` is
Quake's snd_dos.c on the SB16 path: reset, version 4 or nothing, a
4096-byte ring from `qglMemAlloc` aligned to 4096 so it never crosses a
DMA page, filled with 128 (silence is not zero), DMA channel 1 in
auto-init -- the mode byte is 59h; 58h is channel 0, and `t34dsp` fails
on it -- and the DSP playing 8-bit unsigned mono at 11025 in blocks of
half the ring. Nothing waits for the interrupt: `qglDspPos` reads the
count register (flip-flop cleared, interrupts off) and `snd_mix.c`
paints a quarter second past it once a frame, between the tick and the
render, where no one holds an EMS window; a stub on IRQ 7 acknowledges
the block ends or the BIOS's iret leaves the PIC's in-service bit set.
The samples are `tools/mksnd.py`'s `snd.bsc`, every wav the ported
QuakeC plays back to back at the DSP's own rate, in one EMS handle read
through `PAGE_SLOT`; `sndtab.raw` is the (offset, length) table in the
order of `q_pl.bi`'s `SND_*`, and `snd_init` refuses a count that
disagrees. A sound starts at Quake's distance falloff from where it
began, once, mono; eight channels, the one with least left is stolen.
The map's `ambient_*` points -- e1m1 has four comp_hum and a drone --
ship after the items in ents.bin and loop from sample 0 on 32 static
channels -- e1m2 has 20 drips and swamps, e1m4 31 -- placed from the
player every frame at ATTN_STATIC, three a thousand units; one out of
earshot costs the mixer its pointer arithmetic and nothing else.
`snd_loops` counts them. mksnd refuses a wav whose cue point is not
0, since the mixer knows no other loop start.

**The stream is bsc4/32n and the mixer decodes it by table lookup.**
32 samples a block, one byte selecting the block's gain from a ladder
of 32 and sixteen bytes of 4-bit codes: 17 bytes a block, 4.26 bits a
sample, 698,022 bytes for the 1,311,418 the game plays -- 53% of raw,
and 23.0 dB SNR over the whole set, which mksnd prints per wav. No
predictor, so any block decodes on its own and the mixer still seeks
anywhere; `snd_fetch` maps the page, reads the scale byte and indexes
one of 32 rows of `snddec.raw` with each nibble. That table is SHIPPED,
not recomputed -- a decoder building the ladder out of its own `exp()`
agrees with the encoder to whatever its libm does, and nothing in a DOS
box would say when it did not.
963 blocks fit a 16K EMS page and the page's last 13 bytes are padding,
so no block straddles a page and one window reaches any of them; a
SOUND spans as many pages as it likes, `snd_fetch` returning short at
each end. Every sound starts on a block, its length staying the wav's
own, so the silence a block's tail is padded with never plays.
The cost is a nibble and a lookup where a byte used to do, and the
whole sound layer -- card, mix and decode -- is 1 frame of 127 over 400
ticks on e1m1, measured against `-nosound` with the same binary.
`tools/test-sndcodec.sh` is the gate, and it is a checksum for a
reason: a decode reading the high nibble for the low one, or dividing
by the wrong blocks-per-page, still fills the ring on time and leaves
`started`, `loops` and `under` all correct while the card plays noise.
`-sndsum` walks every sound through the paint's own `snd_fetch` and
prints the sum; the test asks mksnd.py what it should have been. Both
halves were watched to fail: the nibbles swapped reads E9909737 for
0C00A2D9, and the padding dropped from `pack_pages` is caught by name
-- a code byte read as a scale.
The ring and the mixer's scratch are one DOS block, not DGROUP -- that
is BASIC's string space -- and with the code they cost the e1m1 far
heap 16K. The codec's table and its decoded run are two more buffers in
that block: `SND_CHUNK` went from 512 to 256 to pay half of it and
`DSP_SCRATCH` from 2608 to 2864, which the layout now fills exactly --
512 of paint, 1024 of table, 560 of channels, 512 of codec table and
256 of decoded run. Exactly is safe only because `snd_mix_setup` adds
those five up against `qglDspScratchBytes()` and refuses (0x0053)
rather than running off the end, which is what it did by 304 bytes
once. cport keeps both buffers in DGROUP instead, having one to spare,
and its `SND_CHUNK` stays 512; the two decoders are otherwise the same
code, and only cport's is under a checksum. `-nosound` leaves the card alone; a machine without one fails
the reset and plays nothing. Every conf pins `[sblaster]` to the
emulator's own 220/IRQ 7/DMA 1, `[mixer] nosound=true` still advances
the DMA, and `viz` turns the sound on. `snd_started` and `snd_under` are
in bench.txt; the two underruns a lit run always shows are the first
frames, which build every surface.

**The mixer's scratch is dsp.asm's to size, and snd_mix.c checks it.**
`DSP_SCRATCH` was 1792 for a 1024-byte paint buffer, a 512-byte table
and 8 channels of 14; the 32 static channels ran 304 bytes past the
DOS block, and the 71 sounds past the 64-record table into the
channels, whose clear then zeroed records 64 and up. dm3ish never
noticed; e1m2 hung in BASIC's heap compactor once two more models
moved the block, with nothing in run.out and no error.log -- the
shape AGENTS.md's `B$FCompactMove` note describes. `snd_mix_setup`
takes `qglDspScratchBytes()` and returns -1 when its layout does not
fit, which `snd_init` reports (`0x0053`). A trace that stops at a
model's marks may just be `MEM_MARKS` -- 64 now; 40 ran out at the
eighth model's triangles.

## e1m1: the first id map

`tools/check.sh --e1m1` pulls `maps/e1m1.bsp` out of the shareware PAK,
builds its assets into `$VBD_OUT/e1m1-assets`, and compares two frames
with `tools/ref/e1m1-spawn.bmp` and `e1m1-exit.bmp`: the hall from the
spawn, and the exit slipgate from its approach. 5,516 faces, 2,750 nodes 62 deep, 1,531 leaves, a 40,843-byte
PVS, a portal table past 64K. Five things stood between the loader and a
frame, and the first is not e1m1's at all.

**DOSBox-X's dynamic core gets BASIC's long compare wrong.** BC routes
every `long` compare through the runtime's `B$CPI4`, which compares the
high words and, when they agree, turns the low words' carry into a sign
bit through `lahf`/`shr`/`shl`/`or`/`sahf`. On `core=dynamic` -- the
pinned core -- that sequence answered `23760 >= 40843` true and
`40843 > 32767` false; `core=normal` answered both right. Same binary.
The loader took the conventional-memory branch it could not afford and
the trace showed 41K leaving the far heap with every guard "passing";
three spellings of the compare all agreed, which is the second rule's
tell. `qgl/cpi4.asm` replaces the routine: the low words get their
sign bits flipped and a plain signed `cmp`, no flag surgery. The runtime
keeps `B$CPI4` in one module with `B$CMI4`, `B$MUI4`, `B$DVI4` and
`B$RMI4`, so all five live there or LINK pulls the module and reports
`L2025`; the three arithmetic ones are far jumps to the runtime's own
`__aFl*`, which `tfw.asm` stubs with an `int 3` for the native suite.
`t33cpi4` runs the six cases through BC's own calling sequence under
run1.sh's dynamic core and failed on the two straddling ones with the
original code in place. Every long compare in this program went through
that routine, so any past oddity involving one is suspect.

**BASIC links a 4K stack, and the C walk recursing 62 deep ran through
it** into the string space above: `String space corrupt`, raised from
`B$CompactSB` under `d_draw_faces` -- lazily, at the next compaction,
nowhere near the write. `tools/link-qr.sh` passes `/STACK:8192`. dm3ish
is 42 deep and never came close. e1m3 is 85 deep and ran 8K out too,
and a bigger stack comes straight out of the far heap (16K was `Out
of memory` at load), so `r_walk_rec`'s frame shrank instead: the
context it needs lives in one struct reached through a pointer and
the leaf and emit halves are their own routines, so the recursion
holds a node pointer and a side.

**A map whose portal table does not load must still walk its PVS.**
`pt_ref` is 6,624 x 7 x 2 bytes on e1m1, past a BASIC array's 64K, so
`r_load_portals` bails and `pt_ok` stays 0 -- and the walk reads
`pvs_now`, which only the flood writes. `pvs_count 78`, the offline
decode's number, and `polys 0`. The copy from `pvs_buffer_b` now also
runs when `pt_ok` is 0.

**Decide a store's placement by `qglMemAvail`, never by a refusal.**
`qglMemAlloc` answers a DOS refusal by shrinking BASIC's heap through
`B$SETM` and retrying, so "DOS refused, go to EMS" never fires -- the
allocation succeeds out of the memory the map needs. The PVS goes to EMS
when DOS's largest block is smaller than it, read through `PAGE_SLOT` by
`mod_pvs_page`, and `r_mark_leaves` steps its offset across the 16K
seams. The depth buffer decides the same way.

**Nodes, leaves and clipnodes cannot go to EMS.** `r_walk.c` and
`pl_trace.c` index those stores through flat far pointers; an EMS store
gives them one page and a cycle in `pl_hull_contents_c` that never
returns. Tried, hung, reverted.

**What the memory went on, and what was cut.** Conventional memory is
BASIC's far heap (334K) plus the UMB pool (82K), and `qglMemAlloc` makes
them one pool. The faces store takes 56K of UMB; the far heap goes
bsp_arrays 79K, leaves 34K, marksurf 14K, nodes 60K, clipnodes 32K,
textures 17K, surface cache 57K, models 12K. Cut so far: the status bar's
28K into its EMS surface, `face_mdl` (11K) into the bits above
`Face.side`'s one, written at load by `ent_load_brushes` and read back
as `side >> 1` in `d_faces.c`, and `CacheSlot.cls` (11K), which only the
selftest read and `sc_bord` already held per block. Then, for the dog:
a model's triangles into page 1 of its EMS handle (20K, the four arrays
and their plumbing gone), `Plane.ptype` (3.6K, never read), the campath
arrays only under `-campath` (3K). The nailgun and the powerups then ran
the far heap to 7.9K and `Out of string space` -- every kilobyte of code
is a kilobyte of far heap -- so the node and leaf bounds are six bytes:
`mkassets.py`'s `bound_bytes` writes `(v + 4096) / 32` a coordinate, the
min rounded down and the max up, `PackedBounds`; `r_cull_box_c` unpacks
to six floats up front and `r_leaf_bound` for the spawn scatter. Node
and Leaf are 16 bytes, 25.6K back: 33.5K of far heap after the depth
buffer with `MDL_MAX_ENTS` at 48 and three view models loaded, 17.6K
once the sound layer took its ring and its code, 16.3K with the
ambients. A box
only grows, so the walk marks a few more leaves: the world's pixels did
not move on any reference, but a pickup in the far doorway of
`tools/ref/bench.bmp` is drawn now and `hud.bmp`'s leaf counter reads
391 for 376 -- both regenerated, 20 and 12 pixels. The oracles are out
of the production link: `qglstub.bas` answers their flags with `0x0060`
and `make ORACLES=1` builds the EXE `check.sh` runs them from, 13K of
code. `sc_slot` is one integer a face now -- the block's generation tag
and style epoch sit on the block, `sc_btag`/`sc_bstag`, since they
describe its content and a face keeps only its block -- 22K back on
e1m2's 5,516 faces less 4K on the blocks. e1m2 plays: far heap after
the depth buffer 23,024 with eight models and the sound layer.

e1m3 (5,274 faces, 2,942 nodes 85 deep, 1,689 leaves; 7.3K after depth
with the zombie and the wizard loaded) loaded with 1K
and died in its first frame with 7K, and three cuts got it to 5.8K
after the depth buffer: the per-face frame stamp is a bit
(`pflag()` is faces/16 integers, `r_pflag_clear` zeroes it a frame,
the walk sets and `d_faces.c` tests a bit -- 10K), the monster kinds
loaded are the ones the map's ents.bin names (`ent_monster_kinds`,
soldier and knight on a map with none) and `mdl_ent()` is the map's
count (6K), and the surface cache's drawing views are one a HEIGHT
re-shaped to the class at every use (`qglSfViewShape`, five views)
where they were one a class, 22 of them made at the first face of
each class -- past the load trace, which is why the first frame died
with 7K to spare and nothing in the trace said so (6K). A lazily
made thing is invisible to a trace taken at load; count what the
first frame makes too.

**e1m4 (6,120 faces, 3,193 nodes, 1,817 leaves, 6,948 clipnodes) is
18K bigger than e1m3 in the far heap, and two cuts paid for it.** The
leaf face list is a `qglArLoadBas` MEM store now, as the leaves are:
`qglMemAlloc` takes the UMB pool before it shrinks BASIC's heap, and
the faces store leaves 26K of it, so the list's 13K (16K on e1m4)
costs the far heap 16 bytes -- the walk reads it through the bound
descriptor as before. That pool is spent now: faces 56K and the
lists 16K, so a MEM store for the submodel table cost e1m4 700
bytes MORE than the array (measured after depth, reverted), and
`QGL_AR_MAX`'s eight stores are all taken -- a ninth fails as
`0x0015 colormap would not load`. `SC_NBLK` 512 to 384 is the
cheap cut left. `MapStore.lfaces` holds the handle, which moved
`GAME_VIS_OFFSET` and `GAME_DLIGHT_OFFSET` by four. And the disk
texture headers, 40 bytes a texture, are erased once the anim chains
are linked. e1m3 reads 30.4K after the depth buffer with both, e1m4
8.7K. e1m4's rebuilt portals miss 152 of 1,230 PVS leaves, so mkassets
ships it an index of zeros instead of failing -- `r_load_portals`
leaves `pt_ok` 0 and the PVS alone draws, as it does on any map whose
table passes 4,681 refs. `tools/check.sh --e1m4` is its spawn frame.

**`ubound()` of an array never made is error 9, and per-map loading
found one.** `mdl_ent()` and `nail()` were sized inside `if ( mon(
MDL_KIND_ARMY% ).loaded )`, which every map had satisfied until e1m3
-- ogres and a demon, no soldier -- and the first frame's `for mdl_i
= 0 to ubound( nail )` was `runtime error 9 at line 0`. Both are
sized before the gate now. What found it, after the debugger's stack
read as garbage: a `tk_mark` writing a tag to a file, open/append/
close, before each stage of the tick and the render -- three builds,
the last one marking each item -- and the run stopped between the
items and the nails. `tools/check.sh --e1m3` is the gate, the spawn
frame against `tools/ref/e1m3-spawn.bmp`; the run died on it before
the fix.

**A black lit world with every counter normal is a missing allocation.**
The surface builder's 16K conventional scratch was taken at the first
lit face by shrinking BASIC's heap, and short of it every lit surface
stayed zeros: `polys` and `sc_built` as always, the unlit frame right,
no error anywhere. A session went on EMS slot theories before the unlit
frame pointed at the builder. The e1m1 spawn and exit arms were the
regression test; both failed on it. The scratch is gone now: the
builder writes the surface in place, one EMS window held across the
texel loop, and refuses a surface whose last row is not in that window.

**Four files make a map's assets, not one.** mkassets writes assets.zip
and, beside it, texr.raw, texs.raw and pal.raw -- the atlases qgl reads
with plain INT 21h. e1m1's zip staged over dm3ish's atlases drew every
texture as some other one, since the offset table indexes whatever
atlas is there: a yellow hazard floor at the spawn, the slipgate pad
brown. `mod_load_textures` refuses an atlas shorter than its table
(`0x0018`), and the gate copies all four in and all four back.

**`make build` rebuilds dm3ish's assets.** It copied
`data/assets/dm3ish.qmp` with nothing tying that file to mkassets, so
an ents.bin header change shipped the old one: the reader took its
blank map name for 8,224 messages and hung in the first frame, and
test-exit's run-after-run arm read 0 of 2. `tools/test-assetdeps.sh`,
run from `make test`, asks make for its plan with mkassets.py newer.

**Every `trigger_*` brush is a volume.** Only `trigger_teleport` hid its
submodel; e1m1's `trigger_changelevel` drew as a column of the
`trigger` texture in front of the exit. `parse_entities` hides them all.

**The spawn yaw is mirrored.** `angle 90` is +y in the map and `-yaw
270` looks down +y here, so `ent_load_spawn` sets `360 - angle`. The
monster ring `main.bas` scatters on a map with no monsters of its own
is seeded from the map's angle as before, so the crowd the fight and
model gates stand beside stays put. The gate aims with `-yaw 270`.

**The map's monsters come from `ents<N>.bin`, one a skill.** mkassets
spawns the map four times -- NOT_EASY 256, NOT_MEDIUM 512, NOT_HARD 1024,
on monsters, items and triggers alike; nightmare is hard's set, with
Chthon's health baked per variant -- against one CrateSet, easy first,
so the crate records and the atlas are the union's. The run loads
`World.skill`'s: the carried one under `-carry`, else `-skill N`, else
easy, which is what every gate plays; id's cvar defaults to normal.
start's `trigger_setskill` sets `Fight.skill` on every touch, and
CARRY.BIN takes it to the next map, as the cvar outlives a changelevel.
`tools/test-skill.sh` stands in start's hard brush and walks the e1m1
slipgate at `-skill 2` and `-skill 0`. Not ported, nightmare's rules:
pain_finished +5 on every hit (`combat.qc`), SUB_AttackFinished and
SUB_CheckRefire (`subs.qc`), the shambler's second bolt (`shambler.qc`).
e1m1 has 10 monsters on easy, 23 on normal, 42 on hard; `MDL_MAX_ENTS`
is 48 for it. The monsters ship with origin and angle, first in the file;
`ent_load_monsters` spawns them through `mdl_spawn` and faces them the
map's way (a model's yaw is Quake's, CCW from +x, no mirror). Nine
soldiers and a dog on e1m1. A map with none, dm3ish, still gets the scattered crowd of
`MDL_CROWD`, eight. The e1m1 image arms run `-noai`: a soldier
behind the exit camera shot the player inside the second and the
health digits moved the frame. `tools/ref/e1m1-spawn.bmp` moved by 13
pixels with this: the old one carried a sliver of the scattered crowd
at its right edge, and the map's own soldiers stand nowhere near.

## Doors

`func_door` is doors.qc without sounds, keys and damage. mkassets
resolves each one offline -- `travel` is `movedir * (size along it -
lip)`, SetMovedir's -1 up and -2 down, speed 100, wait 3, lip 8 unless
the map says -- and `ent_door_init` puts the brush at the shut end, or
the open end for DOOR_START_OPEN. A door with a targetname waits for a
trigger; the rest open by touch: `spawn_field` is the
brush grown 60 in x and y and 8 in z, and the player's box in it sends
the door's whole linked group out. `ent_link_doors` is LinkDoors, brushes
that touch unless DOOR_DONT_LINK, so both halves of e1m1's first double
door go when one is reached. Open, the door holds `wait` seconds and
comes back; a touch while closing sends it out again, which is also what
keeps it off a player standing in the way. 14 doors on e1m1, four by
touch.

`func_door_secret` is the same array with two legs: back `t_width`
along the angle's right (or down), a second's pause, then `t_length`
along its forward, resolved offline as `mid` and `travel`; speed 50,
wait 5, open_once stays, and the way home is the same legs reversed
with the same pause. It fires only from rest (fd_secret_use), a touch
says its message and nothing else (secret_touch), and one without a
targetname takes damage: `pl_fire` runs each pellet against its brush
as it does the shootable switch. Seven on e1m1, four of them shot, the
rest by their triggers. Not ported: its own targets, blocking damage.
`tools/check.sh --e1m1`'s eighth arm shoots `*43` level from 60 units
and wants its brush 60 aside.

**The brush offset is a `Vec3` now, not a z.** Doors slide along any
axis, so `BrushModel.ofs` carries all three: `d_faces.c` adds it to every
vertex, `pl_trace.c` subtracts it from both ends of the sweep, and
`ent_find_node` sorts the box where it is. Plats use `.ofs.z`.

**Placement is cached.** `ent_find_node` for every brush model every tick
was 81 of e1m3's 131 ms frame. `BrushModel.node` now holds
`ENT_NODE_DIRTY` until `ent_place_models` recomputes it, once a frame
before the draw. Only load,
`ent_reset` and a mover whose `ofs` changed that tick set it. A new mover
must do the same. `place_stale` in bench.txt re-derives every placement
at exit and counts the ones that disagree; `check.sh --e1m1` wants 0.

**The frame profile has to account for the frame, or it describes one
that is not running.** cport's `pt_tick/cull/draw/hud` looked reasonable
for weeks while three passes had no bracket at all -- the alias models
(7.6 ms of e1m1's 53), the mixer and the present -- and an unbracketed
pass reads as ZERO, not as missing. `pt_frame_mean` is the frame timed
at its own boundaries and `pt_other_mean` is the frame less every
phase, so the instrument now reports what it cannot see: 0.2 ms of 53.
`pt_frame_mean` against `ft_mean` is a second check, the profiler's
rdtsc against `sys_frame_time`'s PIT ticks -- 53.264 and 53.361.
Profiling arms after frame 3, the same predicate `ft_*` uses, so the
warm-up frames that build every surface in the view are not averaged
into a mean describing no frame that ran. Frame counts and `ft_mean`
repeat exactly run to run; the phase microseconds repeat to about half
a percent, the interrupts landing an instruction apart, which is what
`tools/test-prof.sh` sets its 2% residual bound from. Its third
arm is the one that catches a bracket around the wrong lines: with
`-nomdl -noitems -noview` the alias phase has to fall to nothing.

**Stale: e1m1 in cport is lit now** (`sc_init ok largest=121472`). What
follows is why it was not, and timings quoted from then are the unlit
fill.

**e1m1 in cport drew UNLIT, and one mark was the only trace.** `sc_init`
fails there -- 5,516 faces of `CacheSlot` plus five `SC_NBLK` arrays --
so `sc_ready` is 0, `d_faces` takes the unlit path for every face, and
`-lm` changes nothing: two runs with and without it gave the same
BENCH.BMP md5 and the same 14.12 ms of raster. Nothing says why; the
mark is `sc_init FAILED` and sc.c prints no reason. dm3ish is lit
(`pt_build_mean` 0.089). So any cport frame timing quoted for e1m1 is
the unlit fill, and a lit one will be larger.

**The profiler costs 0.6% of an e1m1 frame, and the logging costs
nothing.** `src/qrcfg.h` gates both -- `QR_PROF` is PhaseTimes'
brackets, d_faces' two rdtsc a face, its nine sc_find key sums that
nothing ever read, and the walk's bench-only counters; `QR_LOG` is
`mark()` and the cstep.txt trace. Both default ON. `make BUILD=<dir>
CDEFS="-DQR_PROF=0 -DQR_LOG=0"`, and each arm needs its OWN build
directory: a define changes what an object means while leaving its
source untouched and make cannot see that.

Measured, `-lm -nostats -noai -ticks 200`, one binary a arm, the
picture byte-identical in every case:

| arm | e1m1 ft_mean | dm3ish ft_mean | DGROUP | near heap |
|---|---|---|---|---|
| both on (default) | 35.905 | 25.525 | 33,016 | 7,944 |
| `-DQR_PROF=0`     | 35.667 | --     | 32,824 | 8,136 |
| `-DQR_LOG=0`      | 35.905 | --     | 32,648 | 8,312 |
| both off          | 35.678 | 25.456 | 32,456 | 8,504 |

So the frame time is ALL the profiler -- 0.23 ms of e1m1's 35.9 and
0.07 ms of dm3ish's 25.5, scaling with faces drawn (325 a frame against
150), which is where the per-face rdtsc pairs are. Logging is 0.000 ms
to three decimals: every `mark()` is load-time or exit-time, and what
it buys is 368 bytes of DGROUP -- the string literals -- in a build that
was once 22 bytes from the 64K ceiling.

**`QR_LOG=0` deletes cstep.txt, and the gates that grep it.** The run
summary -- `frames=`, `pos=`, `fight=`, `gs_state` -- goes out through
`mark()`, so test-walk.sh, test-items.sh and their kin need the default
build. `tools/test-cdefs.sh <with> <without>` is the regression
test: the same frame from both arms, byte-identical, plus an assertion
that the gates fired at all. It compares the PICTURE and not frames or
polys -- `-ticks` bounds the simulation, so the faster arm renders one
more frame over the same 200 ticks and draws more polygons, which is
the change working. Mutation-checked by moving `lm_on = 1` inside a
`#if QR_PROF`.

**`mark()` writes to port E9h, not a file** (`src/dbg.c`). DOSBox-X
logs it with `bochs debug port e9=true`, one line per `\n`, as
`Bochs port E9h: ...`; `tools/run.sh` and `tools/viz.sh` set that and a
`[log] logfile`, and run.sh turns the lines into cstep.txt -- only when
there are any, so a `QR_LOG=0` arm still leaves none. A line is in
dosbox.log the moment it is written: grep it while the run is in flight
instead of waiting for the run to end.

**`tools/test-frames.sh` is the refactor gate**, against
`tools/ref/frames.txt`: eight still cases (`-noai -ticks 120`) and two
live ones (`-walk -fire -ticks 600`, AI on), each comparing the last
frame's md5, `pos=`, `gs_state` and the hash of every `sum` line --
`host_state_sum` prints the player, the monsters, the brush offsets,
trigger states, items taken and the kit every 60 ticks. `tools/ledger.sh
<build> <label>` appends segment sizes, DGROUP, near heap, and per map
`mem_far_largest`, `stack_untouched` and the frame timings (CORE=normal)
to `docs/ledger.tsv`.

**Two replay cases play `tools/ref/<case>.rec`**, saved by `--save`
from the live run, and fail on any `DIVERGED` line. The `sum` lines
cannot catch a broken dt feed: `-ticks` bounds the run, so the end
state is the same whatever the frames were. Only the camera path can.
Doubling dt on records 100..110 of e1m1's went unseen -- the player
stood against a wall there -- and on 20..30, walking, it failed.

**The game state has one owner.** `main` is `host_init`, `host_frame`
and `host_shutdown` on a `Host` on its stack (`src/host.c`); the
player's respawn and carry are `client.c`. `World` holds `Rng`
(Borland's `rand()` exactly, so no reference moved), `Pak` and `Sound`,
and a function without the World takes the one it needs as a pointer.

**`tools/bcc.sh` builds the `COLD` modules -- host, mdl_ai, weapons --
with `-Ox -Os`.** The explicit state cost 2,634 bytes of code, and every
map's far block lost the same. Putting `Rng` first in World, making
`snd_play`/`snd_self` functions and handing `combat_damage_player` the World took
back 515; `-Os` took back 2,048, leaving +71 on p6. `-Os` changes no
float: the modules' `bcc -S` listings match -Ox's x87 code block for
block, struct copies becoming `F_SCOPY@` calls. `-O1` came out
different: it drops the `fabs` intrinsic for a library call on a
double, and reloads from 32 bits a value -Ox keeps at 80. The frames
gate cannot stand in for that comparison: a float path no gate run
reaches changes unseen. A module joins COLD when `tools/x87cmp.py`
reads 0 for every function it defines, both arms listed with `LIST=1
tools/bcc.sh`. make cannot see COLD, so a change to it wants a fresh
build directory, as CDEFS does.

**bench.txt timers are `name min mean max`, three decimals.** `pt_<phase>`
is ms a frame; `pt_tk_<call>` is one `host_tick` call in ms, over the
ticks it ran in. `sys_rdtsc` wraps about once a minute, and
`host_lap` drops a lap that runs backwards.

**`func_train` rides the plat array** (`PlatEnt.kind`), and the map's
`path_corner`s are `ent_corner()` in ent.bas, each with its `nxt`.
mkassets resolves the route offline; func_train_find puts the brush's
mins on the first corner, a train with a targetname waits there for
its trigger (train_use, from `ent_use_targets`), the rest go at once.
`ent_move_train` is train_next/train_wait/SUB_CalcMove: a straight line
to the next corner at speed, the corner's wait there (id's -1 is a wait
of nothing: `ltime + wait` is past), and the rider carried sideways and
up by the same delta (`ent_mover_ridden`). e1m2's two lifts end on a
pair of corners at one point, and bounce between them standing still.
`tools/check.sh --e1m2`'s first arm stands on their button and wants
both at that corner. Not ported: the ratchet sounds, blocking damage.

**The plat tick walked the trains too.** `ent_move_plats` set every
entry's state to plat UP or DOWN each tick, so no train reached its
wait or left its first corner: e1m5's four untargeted trains read one
brush checksum for ten seconds. It skips trains now, and
`tools/test-trains.sh` wants three distinct `br` values on e1m5.

**Keys.** `item_key1`/`item_key2` are item kinds: taken once into
`fight.items` as `PL_IT_KEY1`/`KEY2`, "You got the silver key" (the
worldtype's word: key, runekey, keycard), misc/medkey or runekey, and
-- as every pickup does now, SUB_UseTargets -- the item's `target`
fired: e1m2's key opens door *49. A door with DOOR_SILVER_KEY (16) or
DOOR_GOLD_KEY (8) has no spawn field, like a targeted one: touching
its brush without the key says its message -- mkassets puts door_touch's
"You need the silver key" there when the map gives none -- and plays
doors/medtry two seconds apart; with it the key is spent, meduse, and
the group goes. Base's key wavs are registered, so worldtype 2 plays
the rune set. Keys never carry to the next map (SetChangeParms). Not
ported: the bar's key icons -- the cell band is 512 wide and full.
The e1m2 gate's second arm stands on the key, the third walks at the
key doors without it.

**`trap_spikeshooter` is a trigger kind**: used, it arms
(`ENT_TRIG_ARMED`) and `pl_traps_tick` sends a hostile spike from its
origin along its movedir at 500, weapons/spike2, 9 or 18 for
SUPERSPIKE. A hostile spike is walked as a POINT through hull 0
(`pl_point_contents` a step at a time) rather than traced through hull
1: its origin sits 8 units off its wall, inside hull 1's grown solid,
and the hull-1 trace ended it before it flew -- the first run read
health 100 with six spike sounds. It bites the player's box
(spike_touch), not monsters, and passes brush entities. The nail step
divides by each nail's own speed now, not the nailgun's 1000. The e1m2
gate's fourth arm stands in the trap's trigger for 2.5 s and wants
health under 100.

**A monster with a `target` patrols** (walkmonster_start_go's
th_walk): `EntsMon.first` is its path_corner in ent.bas's corner table,
`MdlEnt.patrol` keeps it for the respawn and `corner` is the one bound
for. `mdl_patrol_to` sets the goal and the facing (t_movetarget), the
walk is the run cycle at `MDL_PATROL_STEP` a think -- army_walk's
average stride, 18 u/s, since no walk frames fit the page -- and
arrival within 24 takes the next corner, stands the corner's wait, or
stands for good at a corner with no target. FindTarget runs every
think of the walk as before, and a hunt ends the patrol. The e1m1
gate's patrol arm runs ten seconds with the AI on and wants a soldier
between (950,2048) and (1200,2048); the run read 1105.

**BC miscompiles a store of a single into a member of an indexed
element when it is the second such store in a row.** `door(k).model = m`
then `door(k).speed = dr.speed`: the second keeps `k*70` cached in AX,
loads the value into AX:DX, then adds the member offset to AX and stores
through it -- the value lands at `base + low word of the value + 26`. A
speed of 400.0 has a low word of 0, so every door's speed was written
into door 0 and the others read 0: the second half of the double door
sat "opening" for ever at offset zero. The third store reloads the
cached offset from `[bp-1Ah]` and is right, which is why `hold` beside
it was. The listing is what showed it (`bclst.sh`, `ENT_DOOR_INIT`);
`ent_door_init` fills a local `DoorEnt` and stores the element whole.
Any load that fills a record member by member into an indexed element
is suspect; check the listing for `add ax,` right after a load into AX.

`tools/check.sh --e1m1`'s third frame is the test: `-walk` from
(330,576) at the first double door must carry the player through it,
px below 190; a door that does not open stops them at 272.03.

## Triggers and buttons

`trigger_once`, `trigger_multiple`, `trigger_counter` and `func_button`
are one array, `trig()`, since all four do one thing: fire a target.
mkassets turns every targetname into a number, so a door's `targeted`
is its name's id and `ent_use_targets` is a loop over ids, never a
string compare. A touch in a trigger's volume fires its target and says
its message; once is a multiple with wait -1, a multiple re-arms after
wait (0.2); a counter fires its own target when used `count` times (2).
A button slides in when touched and fires when it ARRIVES, as
`button_wait` does, holds `wait` seconds (-1 stays), and comes back.
A trigger with health (e1m1's `*16`, a wall switch) is shot, not
touched: `pl_fire` runs every pellet against such boxes short of what
it stopped at plus `PL_HALF`, and the first hit fires it (multi_killed).
The margin is there because `pl_trace` walks hull 1, the player's, so
a wall stops a pellet 16 units early, and e1m1's switch volume stands
8 units off its wall: without it four shots at the switch fired
nothing. A point trace through hull 0 is the real fix. Nothing in
sounds. `-fire` holds the trigger for a
headless shot and `-pitch D` aims it up or down, as `-yaw` aims it
round: the default view looks 11 degrees down, mouse y 110 of 200.

A message is a centerprint: `ent_say` puts it in `g.fight.msg` for two
seconds and the overlay draws it where the state messages go, under
`-nostats` too. A targeted door says its own when its brush is touched
(door_touch), which is why such a door's field is the brush plus two
units where a touch door's is grown 60. `tools/ref/e1m1-exit.bmp`
carries "Walk into the slipgate to exit." because its camera stands in
that trigger; the reference is what proves the text draws.

`killtarget` removes: a trigger's use puts every trigger named by it in
DONE before its target fires, which is SUB_UseTargets' order. e1m1 uses
it twice, each a trigger_once that removes the hint trigger it also
names, so the hint is never said again once the player has passed. A
door or a monster named by a killtarget stays. `delay` is
SUB_UseTargets' DelayThink: a fired trigger with one holds its kill
and its fire in `delay_left`, run down by `ent_move_trigs`; a fire
inside the delay restarts it where id's spawns a second thinker.
e1m6's nine-door stair opens three seconds after its trigger, and the
e1m6 gate stands in it a second: the trigger spent, the first door
still shut.
The ninth e1m1 arm stands in `*54` and wants `*51` DONE.

`trigger_changelevel` is the level's end: touching it is `GS_EXIT`,
the view cut to the map's first `info_intermission` at its mangle
(`pl_intermission`: noclip, held still, no gun), LEVEL COMPLETE with
the worldspawn's message -- the map's title -- above it and the kills,
secrets and time under it, and fire starts the map over: `pl_game_reset`
puts the monsters, items and player back, on foot and facing the
spawn's way, and `ent_reset` every door shut and every trigger and
button as it loaded. A map with no info_intermission looks from its
start, as Quake does. The slipgate arm wants the exit at (-112,704).

**The next map is a new process.** Fire held two seconds into the
intermission (IntermissionThink) runs `host_next_level`: the kit to
`CARRY.BIN` by SetChangeParms' rules -- keys and powerups dropped,
health 50..100, 25 shells at least -- and `NEXT.BAT`, `call GOMAP.BAT
e1m2` then `qrender.exe e1m2.bsp -carry` with the run's own flags (-lm,
-nosound, -bench, -ticks, the -no* set, -fire; never -at or -yaw), and
the host loop ends on `GS_NEXT`. GOMAP.BAT copies `MAPS\<map>\*.*` --
the bsp and its four asset files, `make maps` builds `data/maps/<map>/`
for every map in `MAPS` and the build stages them -- over the ones
beside the exe; dosbox.sh's run.bat copies NEXT.BAT to RUN1.BAT,
deletes it and calls it while one is written, since a batch deleted
while it runs is "Batch file missing". `-carry` reads CARRY.BIN over
`pl_reset_player`; a missing or short file logs `carry refused` and plays
the fresh kit, and test-exit's fifth arm is the missing half. Each changelevel's `map` key ships as a message whose
id is its trigger's `count`, and the touch copies it into `next_map`.
The header's one `next_map`, the last changelevel read, is gone: every
slipgate on start led to `end`. bench.txt says `map`. The e1m1 gate's last arm walks into the slipgate
with fire held and the soldier awake and wants e1m2's bench with the
damage carried: e1m1's shells 25 and fire's 14 shots read `pl_shells
11` there.

`tools/check.sh --e1m1`'s fourth frame walks into the first button:
the plunger floor (`*3`, a door targeted by `*4`) must go down with the
player on it, pz below -100 by tick 240. The sixth walks and jumps
into the slipgate -- its pad is 32 units up, past the 18 a step climbs
-- and wants `gs_state 4`. The seventh fires at `*16` from 108 units
and wants door `*15`, the bridge it lifts, off its -64.

## `-nostats` makes the picture deterministic

With the HUD off the renderer is **byte-identical run to run** -- one
binary, two runs, `imgdiff` reports IDENTICAL. An 8-to-40 pixel spread
that looks like surface-cache churn is the overlay, not the cache. Every
A/B on an image wants `-nostats`, because it turns "within noise" into an
exact test. The churn below is real, but it is what `--churn` provokes,
not what a still frame shows.

## The surface cache draws a different picture every run

**Closed by 8e57e79, as far as `--churn` can see.** The picture varied
because `d_faces.c` walked BASIC's far-heap arrays through pointers taken
once at entry, and the heap compacts under the calls inside the loop; two
runs then compacted at different faces. With every pointer re-taken per
face, `--churn` gives two byte-identical frames over 266 frames. Note it
now reports `sc_evict 0` -- the campath does not evict on qgl's 4MB store
-- so reuse after eviction is untested, and the bisection below is kept
for whoever next sees a varying picture.

`tools/check.sh --churn` reproduced it: one binary, `-ticks 900` so the
camera stops in the same place, two runs, ~68% of pixels different. A wall
that is tan brick in one run is dark with red and green streaks in the
next.

What the bisection established, each step a measurement:

| test | result | what it rules out |
|---|---|---|
| `-lm` off | byte-identical x3 | the campath, physics and tick loop |
| flush every frame | byte-identical, `sc_built` equal too | anything within one frame |
| `sc_find` forced to always miss | byte-identical x3, and CORRECT | the builder, under any eviction history |
| ownership check on hit | never fires (`0`) | a block changing hands behind a slot |
| overlap scan at alloc, 1070 runs | never fires (`0`) | two live faces allocated the same bytes |
| re-read after forcing the window elsewhere | same bytes | a stale EMS page on the READ side |
| fingerprint at build, checked on hit | **48-110 mismatches** | nothing: the bytes really do change |

So the builder is right, the allocator's books are right, and the bytes of
a cached surface are overwritten between the build and the reuse. Forcing
every face to rebuild hides it completely, which is why a fixed camera --
which evicts nothing and rebuilds nothing -- has never shown it.

**The clobbering write is megabytes from where it belongs.** Watch one
cached surface and re-read it after every later build: the surface at
granule 15040 was destroyed by the build of a face at granule **7072**,
both order 5. Not adjacent, so not an overrun -- a write landing in the
wrong EMS *page*, which is the `gv_dst` failure again. The builder streams
the atlas across many pages while holding a pointer to its destination,
and the pool may take the destination's slot to map the next atlas page.

Pinning the destination page across `uglBuildSurf` took the hit-time
mismatches from 48-110 to **0** and cost about 12% of the frame rate --
and the picture still varies, so there is at least one more path. The fix
belongs in `uglBuildSurf` itself: re-derive the destination pointer per
row instead of holding it, exactly as `d_poly` now re-takes `gv_dst` per
face.

**A run in four dies before writing anything** -- empty `run.out`, no
`error.log`, no `bench.txt`. Unrelated to the above and still unexplained;
`--churn` retries rather than call it a failure.

The slot NUMBER stays with whoever owns the resource -- `mod_lm_lock` and
`mod_cm_lock` live in `model.bas` next to the maps they pin, and callers
pin by name. Nobody names a slot they do not own; that is the whole point
of the pool.

Two sites are safe and worth knowing why, so they are not "fixed" by
mistake: `d_poly`'s per-face geometry copy and `sb_fetch` both go from map
straight to `memCopy` with nothing but integer arithmetic in between.
`hud_shade` is safe only because its destination is the MEM backbuffer --
see the note there.


- **`render.scale` wedges under `core=dynamic`, draws correctly under
  `core=normal`.** The view is rendered into a small backbuffer and blown up
  to fill the screen; `uglPutScl` does the magnification. Under
  `cycles=75000 core=dynamic` the program stops responding -- black screen,
  Esc dead. Under `core=normal` the identical binary draws the scaled view.

  What it is NOT, all tested:
  - not our computed values: hardcoded `160, 100, 3.0, 3.0` hangs too
  - not the position: `uglPutScl` takes the TOP-LEFT, whatever its header
    comment says about a "center col" -- `h_precalc` passes x,y straight to
    `DC_CLIP_SCL`, which measures width as `xmax-x+1`. Confirmed on screen.
  - not clipping or an off-by-one at the edge: a whole-number scale putting
    the rect strictly inside the mode (192x192 at 64,4) hangs just the same
  - not one entry point: `uglBlitScl` hangs identically
  - not self-modifying code as such -- the texture mappers patch immediates
    into their own inner loops and run fine under dynamic

  Where it ends up: sampling under the debugger caught it once in
  `ugl_text` near `UGLBUILDSURF` and once spinning in `B$FCompactMove`, the
  VBDOS runtime's local-heap compactor (`lcompact.asm`, LMEM segment). That
  loop deserves a note of its own:

      097A  inc bx              ; block header 0xFFFF -> 0x0000
      097B  add si,bx           ; si += 0, no carry
      097D  jnc 0x943           ; loops instead
      097F  call B$CorruptHeap  ; the check it should have reached

  The runtime DOES guard against a runaway heap walk, on the carry out of
  `add si,bx`. A header of `0xFFFF` is the one value that defeats it, because
  `inc bx` wraps to zero and never carries -- so it spins instead of
  reporting a corrupt heap. Worth knowing whenever this codebase wedges with
  no output: that is what it looks like.

  **`/MAP` on the LINK line is what made any of this readable.** Without it
  `qrender.map` carries segments only and the debugger can say
  `LMEM+0x943` but not which routine that is; with it there are 2,545
  publics and the same address resolves to `B$FCompactMove`.


- ~~`clpBuffer` in `model.bas`~~ hardened to `'$STATIC`.
- ~~Remaining cuts~~ done — `main.bas` is 427 lines: `doInit`, `doMain`,
  `doEnd`, `ExitError`. All ten modules carry `OPTION EXPLICIT`.

## Dynamic lighting

A single light follows the player (`g.rdr.dlight`, updated in `host_tick`
from `g.pl.pos` every tick) and brightens nearby luxels live, additively,
on top of whatever a face's baked lightmap and light style already
produced -- Quake's own dynamic-light effect, minus the emitters this
renderer has no rockets or muzzle flashes to drive. `DL_RADIUS#` in
main.bas is Quake's own rocket-light radius, 200 units, picked for no
better reason than fidelity to the source.

**Reuses the light-style scratch buffer (`ls_scratch` in `d_surf.bas`)
rather than adding a second one.** A face gets copied in, scaled by its
style if non-neutral, and now also has the light's falloff added per
luxel, all in the one pass -- the two effects were never going to collide
since both only fire for faces already taking the rare, non-neutral path.

**Coordinate space needed checking before any of the geometry math got
written, not after.** `PlayerState.pos` documents itself as BSP space,
Z-up; `Plane.norm`/`.dist` and `TexInfo.vecs`/`.vect` had to be confirmed
the same way, since a mismatch here is exactly the kind of thing that
compiles, runs, and silently produces wrong numbers. Two facts settled
it, read out of working code rather than assumed: `d_poly.bas` dots
`vecs`/`vect` against `vx,vy,vz` taken straight from `gv_buf` -- raw BSP
vertex data, before the Y-swap to renderer space happens a few lines
later -- so `vecs`/`vect` are BSP-space and want an UNSWAPPED point.
`r_cam_plane_dist`, by contrast, dots a Y-up point against a Plane with
the swap baked into the dot product itself (`pt.y*pl.norm.z +
pt.z*pl.norm.y`), which is the opposite convention on the opposite kind
of input. The two must never be mixed: this feature uses `g.pl.pos`
(already Z-up, no swap) with `vecs`/`vect` and its own plain,
un-swapped `Plane` dot product throughout, and never touches
`r_cam_plane_dist` at all.

**A dynamically lit face has to rebuild every frame it stays in range,
not just the first.** The light's own falloff is computed inside
`sb_build`, but the decision to skip the cache has to happen earlier, in
`d_poly.bas`, before `sc_find` ever runs -- a plain style-epoch match
would otherwise paper over the fact that the face needs redrawing. Forced
by overwriting `lm_stag` with `-1` (a value `ls_epoch` never returns)
whenever the face's plane is within radius. That also gives, for free,
exactly one extra rebuild the frame the light leaves -- the stale `-1`
from the last lit frame no longer matches the real epoch computed once
the light is gone, so it misses once more and washes the glow back out,
then settles. No separate "was lit last frame" flag needed.

**Verified the same way the style-scale math was: unit tests for the
formula (`ls_add_dlight`, hand-computed including a 3-4-5 triangle for an
exact check), then confirmed against real runs on two independently
converted maps.** Neither dm3ish nor e1m7 gives any way to eyeball this
in isolation, so `sc_dlit` (builds the light actually reached) is a
permanent counter, not scaffolding -- `-campath` on dm3ish shows several
hundred over one walk, and pixel-diffing e1m7's `-bench` frame
before/after the feature landed shows 69% of pixels uniformly brighter,
no noise, consistent with an additive light near the camera in a tight
corridor.

### cport: id's lights, not the test light

`src/dl.c` is WinQuake's: 32 slots allocated as `CL_AllocDlight`
does, `CL_DecayLights`, and the emitters -- a muzzle flash for every
player weapon and the soldier's volley, the quad's and pentagram's
EF_DIMLIGHT, EF_ROCKET on rockets and lava balls, TE_EXPLOSION on every
blast. They change in `host_tick` and never in a frame, and the flicker
has its own LCG so `World.rng`'s sequence is the game's. `-nodlight` is the
A/B. Not ported: the shambler's flash (NetQuake's monster QuakeC is not
on disk to say it has one) and model lighting (the models draw unshaded).

**A lit face's stag must change every tick.** The test light's constant
-1 was a hit on the second lit frame, so a glow froze at its first
build; that light never changed, and nothing showed it. `dl_stag` is
`-1 - dl_tick` now. `tools/test-dlight.sh` wants the quad's frame
to move from tick 32 to 33 and a dead muzzle flash to leave the
`-nodlight` frame.

**A luxel byte is id's blocklights over 264, not 256** -- 'm' is 264 and
`SB_LEVEL2T`'s 66 is 264/4 -- so a light adds `(rad - dist) * 256 / 264`.

**The mark is per face, not per node**: the plane within the radius and
the light's foot within the radius of the face's luxel rect. The plane
alone marks every coplanar face on the map and rebuilds them each tick
for the same picture.

**A light style lives in a face's second to fourth lightmap.** mkassets
shipped only style 0, so e1m1's 137 faces lit (0, 10) -- the fluorescent
flicker -- drew steady. Every plane ships now, stacked `lm_h` rows apart
in the face's slot, and `sb_build` sums them scaled by their styles as
R_BuildLightMap does; `ls.c` has all of world.qc's styles.
`tools/test-styles.sh`; `-nostyles` holds every style at "m"
(`ls_hold`), where it used to freeze each at its pattern's first letter.

**A style nothing sets is 256, not 'm'.** id's `d_lightstylevalue`
starts at 256 and only world.qc and the lights set any; 'm' is 264. So
styles 12..62 start at `LS_UNSET`, 116 of 'm''s 120 -- 256/264 rounds
to 116.4, 0.3% under.

**A flickering style keeps a surface per value.** The cache key was the
sum of a face's style epochs and a face held one surface, so each change
of style 10's m/a flicker rebuilt every (0, 10) face in view: 8.4 ms of
e1m1's 46 at the spawn, 1,184 builds between tick 300 and 600 of a still
camera. The key is the styles' values now (`ls_face_key`), and a face
chains up to `SC_VARIANTS` blocks through `bsib`, most recent first. A
new variant only comes out of free store -- with the block records
spent, taking one would evict a surface in view -- and a dynamic light's
key never comes back, so it writes over what the face has.
`tools/test-scvariants.sh`.

**Running out of block records looks like no variants at all.** At
`SC_NBLK` 384, e1m1's (480,-19,29) yaw 295 wanted 409. Each new key was
refused a block and took the face's other one, so the flicker rebuilt as
before: 800 builds in 300 ticks, 41.3 ms against 37.4. `sc_nofresh` counts
the refusals, and `sc_blocks_peak` is a high-water mark. The records add
11.5 KB. test-scvariants' spot arm is the test.

**`sc.c` freed `qglMemAlloc` blocks with `farfree`.** `farfree` only knows
Borland's heap, so the DOS blocks stayed allocated. `sc_selftest` runs on
every launch and kept its whole throwaway cache: 18,960 bytes at
`SC_NBLK` 1024. That is why going from 384 to 1024 cost 23 KB of largest
free block instead of 11.5. `sc_selftest_leak` in cstep.txt is the free
total before the selftest less after, and test-lit wants 0.

**`QGL_MEM_TOTAL` reads 16 bytes low for every freed block DOS has not
merged yet.** It adds up free MCB sizes, and each unmerged neighbour still
has its own header. With the leak fixed the selftest read 256, sixteen
blocks. A `QGL_MEM_LARGEST` query first is a failing 48h, which walks the
chain and merges it.

**A cache hit aims row 0 alone.** `qglSetView` writes the view's whole
address table, and `sc_find` called it once per lit face drawn. The
texture fetch reads nothing past row 0, so a hit calls `qglAimView`.
`sc_alloc` still aims every row, because the builder writes all of them.
`t38aimview`.

**Switchable lights are `ents.bin` records.** A light of style 32 or more
with a targetname is `light_use`'s: START_OFF starts its style "a", its
trigger toggles it. Each light carries a stamp, its map index at load and
the next of `light_clock` at each use, and a style shows the light on it
stamped last: of two lights on one style the later USED wins, as each
use's `lightstyle` overwrites the other's. `ent_lights_selftest`. Nothing of this reached cport before, so e1m1's four
START_OFF lights were on from the start. The header grew to 78 bytes,
and mkassets read its monster records back at a hard-coded 76: e1m1
shipped the knight and the ogre and died at `soldier.geo: no such
member`. `ENTS_HEAD` is the one format now, and every staged `.qmp`
needs rebuilding. `tools/test-lightsw.sh`.

## Culling: the subtree skip, and what it nearly cost

e1m1 at the spawn, cport, `-nostats -noai -ticks 120`, one binary,
`-nosubvis` the A/B flag:

| | cull | nodes | leaves | frame |
|---|---|---|---|---|
| before both changes | 31.45 | 2,493 | 2,502 | 69.99 |
| entity emit by bitmap | 7.59 | 2,493 | 2,502 | 45.19 |
| + the subtree skip | 3.12 | 540 | 411 | 39.38 |

**Most of what looked like walk cost was `r_emit_entities` rescanning
every submodel at every visited node.** 2,493 nodes x 58 models a frame,
to place about twenty doors -- 24 ms of a 31 ms cull, hidden behind a
call the profile attributed to the walk. `ent_nd`/`ent_lf` are one bit
per node and per leaf, rebuilt each frame beside `vis_walk`, and the walk
emits only where a bit is set. `tools/test-brushents.sh` is the
regression test it already had: with no bit ever set the e1m1 double door
draws 0 pixels instead of 2,122.

**Then Quake's `node->visframe`:** a bit a node saying some leaf below it
is in `pvsb`, computed post-order when `pvsb` is rebuilt -- which is only
when the camera changes leaf -- and tested before `r_cull_box`, which is
six byte-to-float unpacks and up to twelve multiplies against one bit.
dm3ish is smaller and gains less: 289 nodes to 94, cull 4.92 to 3.70.

**A brush entity is drawn with the PVS IGNORED, so the PVS bit alone
prunes it away.** Its own leaves are not in the PVS -- a lift sits inside
its solid shaft -- and `ent_find_node` places it at the deepest node (or
LEAF: the descent ends at one whenever the box straddles no plane all the
way down) its box does not straddle. That node is routinely one the PVS
pass leaves unmarked. So `vis_walk` is `vis_sub` plus the root-to-node
path of every entity about to be emitted, which needs `nd_parent` and
`lf_parent` -- there is no way up a Quake BSP otherwise. Without it e1m1
at the spawn draws 254 polygons a frame where it should draw 327, and
**the picture does not move**: those 73 are entity faces behind world
geometry from there. Quake never had this problem because it draws
entities from its own list, not inserted into the node walk.

**With a depth buffer, brush entities draw after the world.** World
faces write depth without testing it, so a world face later in the walk
than an inserted entity covered it: across e1m1's plunger pit the
button column drew over the platform, the column's foot matching
`-noents` on all 784 pixels. The world now writes, then the brush
entities test and write, then the models and items; `QGL_Z_TEST` writes
on a pass. Insertion stays for `-noz` and for `-entinsert`, the A/B.
`tools/test-brushdepth.sh`.

**A per-frame counter divided by the profiled frame count is not a
mean.** `pt_marked` read 575 against 486 on the two arms and that gap was
the instrument: the counters summed from frame 0 while `pt.n` counts from
frame 4, and two arms that differ in speed render a different number of
frames over the same ticks. Armed with `pt.n`, both arms read 589. Any
A/B is between a fast arm and a slow one, so this is every A/B.

**`-nostats` did not make the frame deterministic: the fps printed
anyway.** `scr_draw_hud`'s top bar drew `fps: [N]` right-aligned outside
the stats gate, so BENCH.BMP carried the frame rate and no two arms of
different speed could ever come out IDENTICAL -- 19 pixels at x 145..151
swapping between palette 15 and 16, which is four digits of 4x5 font and
reads exactly like a rasteriser difference.

It draws again, because a person watching a window wants it, and the
gate is `-ticks` rather than `-nostats`: every A/B recipe here is
tick-bounded and no interactive run is, so the readout is live on screen
and absent from every frame anything compares. A windowed `-ticks` run
has no fps either, which is the price. `-stats` overrides -- that frame
is never a reference. `tools/test-fps.sh` runs two arms of one binary at
25 and 32 fps and requires one picture, and fails if they ran at the
same speed, since then it is watching nothing. The `at:`/`yaw:` half was
never gated: it is deterministic and it is how a headless run says where
it is.

**DGROUP + `_stklen` must fit 64K, and nothing said so.** Borland's
medium-model startup puts near data, the stack and the near heap in one
segment and calls `abort()` before `main` when they do not fit:
"Abnormal program termination" on stderr, which DOS does not redirect.
So the symptom is an empty `> out.txt`, no `cstep.txt`, a nonzero exit
and no message anywhere -- indistinguishable from a broken build, and it
cost a session. cport was **22 bytes** inside the limit at `_stklen`
32768; the string literals of two new `fprintf`s crossed it. `_stklen`
is 24576 now (8,170 bytes of near heap), `tools/dgroup-check.sh` runs
from `link.sh` on the map it just wrote, and `tools/test-dgroup.sh` is
the regression test. Moving the state structs off main's stack into
`static` makes this WORSE, not better -- it moves them into the same
DGROUP -- which is why that attempt did not revive the build and read as
evidence against the stack theory.

**A far address handed to a near pointer reads DGROUP, and BC says
nothing.** In the medium model a plain `T *` parameter is near, so
`&ent->pos` with `ent` a `MdlEnt far *` loses its segment on the way in,
and an explicit `(BspVec3 *)` cast does the same without a warning.
`mdl_spawn`'s drop to the floor traced from DGROUP: on e1m4 a different
soldier stood somewhere else, or in mid-air, in every build, with the
frame and the player identical. Only `host_state_sum`'s monster total
moved, and it moved with the binary, not the timing -- 236979 at 59
frames and at 25. `-record` wrote DGROUP the same way through `fwrite`.
Copy to the stack. `tools/farnear.py` finds the pattern and runs in
`make test`; `tools/test-layout.sh` compares a default build's state
sums with a `QR_PROF=0` build's, which is the symptom.

## The alias pass was not drawing, it was asking

e1m1 at the spawn, cport, `-nostats -bench 4000 -ticks 120`, one binary,
`-nolcache` the A/B flag:

| | frame | alias | descents | polys |
|---|---|---|---|---|
| `-nolcache`, `-noai` | 39.426 | 10.028 | 316 | 327 |
| leaf cache, `-noai`  | 31.531 | 2.374  | 0   | 327 |
| `-nolcache`, AI on   | 40.071 | 10.007 | 316 | 327 |
| leaf cache, AI on    | 32.447 | 2.708  | 15  | 327 |

Same BENCH.BMP in all four.

**Almost none of the 10 ms was drawing.** `d_mdl_visible` sampled nine
points around each entity's box and descended the tree from the root for
every one: 62 levels on e1m1, nine per entity, 36 entities -- **13,007
node visits a frame**, against 540 for the entire world walk after the
subtree skip. Thirty-five of the thirty-six entities are invisible, so
that is what the money bought.

The leaves an entity's box lands in depend on the box and nothing else.
`LeafCache` keeps the nine, keyed on (origin, radius, zlo, zhi), and the
per-frame test is nine array reads into `pvs_now`. `-nolcache` is the
A/B.

dm3ish gains almost nothing, and for the reason the subtree skip gained
little there: 23.875 ms to 23.486, alias 2.779 to 2.428, 37 descents a
frame to 0. Its tree is 42 deep against e1m1's 62 and the frustum throws
out most of its 41 entities before any descent happens.

**Cache the LEAVES, never the answer.** `pvs_now` changes when the camera
changes leaf, so a slot holding visible/not goes wrong the moment the eye
moves -- and nothing catches it while the eye stands still. That version
draws the same frame, the same polygons and the same entity count as the
correct code at the e1m1 spawn over 900 ticks with the AI on;
`tools/test-lcache.sh` walks the camera for exactly that reason, and
there it reads 238 polygons for 240 and 2 entities a frame for 4.

**Freezing the key is not caught, and that is stated rather than papered
over.** Drop the origin from the comparison, so a MOVED entity keeps its
old leaves, and every arm tried -- the spawn and the monster viewpoint,
120 to 900 ticks, AI on -- renders an identical frame with identical
counters. A monster's nine leaves stay on the same side of the PVS over
the few hundred units it walks in a run this long. The defect is real;
it has no symptom here to assert on.

**`pt_dv_*` are means over the profiled frames, like every other `pt_`.**
They were raw totals first and read 48 against 61 on two arms doing the
same work, because the faster arm rendered 65 frames to the slower one's
52. That is the same denominator trap `pt_marked` sprang one section up,
in the same session.

## Where a map lives

Which lumps are conventional, which are EMS, the rule that decides, and
the 54 K of surface cache that does not fit on e1m1 -- `docs/cport-memory.md`.
