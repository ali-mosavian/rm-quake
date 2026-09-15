# QuakeC in 16K pages

Can qcport run id's `progs.dat` inside its existing EMS model, 16K pages
mapped through four slots? This note covers what was measured, how, and
which designs follow. The paged VM and its strings are built, in `qvm/`:
see "The paged VM, built".

The figures come from PAK0's `progs.dat` (v6). The tools are in
`tools/qcvm/`:

    PYTHONPATH=tools:tools/qcvm python3 tools/qcvm/<tool>.py PAK0.PAK

## Bird's-eye view

### Where QuakeC would sit in a frame

Today the game logic is C, ported from the `.qc` files. A VM would run
the same logic from `progs.dat`. Everything around it stays C:

    host_frame
    +-------------------------------------------------------------------+
    | host_advance: 0..HOST_MAXSTEPS ticks of HOST_DT                   |
    |  +-------------------------------------------------------------+  |
    |  | host_tick               today              with a VM        |  |
    |  |  input, camera          C                  C                |  |
    |  |  movers, triggers,      C port of the .qc  progs.dat on the |  |
    |  |  items, monsters,       ent_move_*, mdl_   VM, called as    |  |
    |  |  weapons, missiles      tick, pl_fire ...  think/touch/use  |  |
    |  |  traces, physics        C                  C, as builtins   |  |
    |  +-------------------------------------------------------------+  |
    | snd_frame     the mixer                        C                  |
    | host_render   walk, faces, models, HUD         C                  |
    | host_present                                   C                  |
    +-------------------------------------------------------------------+

### One tick, id's order

Each arrow into QuakeC is one **execution**. An execution maps the pages
its code sits on:

    tick (SV_Physics)
     |
     +--> StartFrame()                                  QC, every tick
     |
     +--> edict 1, the player
     |      PlayerPreThink()                            QC, every tick
     |      movement, traces                            C
     |      .touch() of what it walks into              QC
     |      PlayerPostThink()                           QC, every tick
     |
     +--> edicts 2.. in order
            .think() when nextthink is due              QC, ~10/s an entity
            movement and traces                         C
            .touch() / .blocked() on contact            QC

### Build time to run time (proposed)

    BUILD  (Python, offline)                   RUN  (DOS)
    PAK0.PAK
     +-- progs.dat ------+
     |                   v
     |        call graph + function-pointer targets
     |                   |
     |                   v
     |        packer: 2,025 functions -> 14 code pages   ---> EMS handle: code
     |        dfunction_t -> 4-byte inline headers
     |        temps placed beside their function
     |                   |
     +-- maps/*.bsp -----+
                         v
              interner: QC string constants
                        + entity values   -> ids
                         |
                         +-- text used by prints    ---> EMS page: strings
                         +-- names, for errors      ---> file on disk
                         +-- entity records by id   ---> EMS store: edicts
                         +-- shared globals, 488 B  ---> conventional

### The four EMS slots over a frame

The same four windows serve every subsystem. The VM has them during the
tick; the mixer and the renderer remap them afterwards, so the next tick
starts cold:

             tick: the VM          snd_frame      host_render
    slot 0   code page, LRU   |                 | QGL_TEX_SLOT  texture atlas
    slot 1   code page, LRU   |                 | QGL_Z_SLOT    depth (EMS)
    slot 2   code / strings   | PAGE_SLOT       | PAGE_SLOT     stores, verts
    slot 3   edict window     |   samples       | CM_SLOT       colormap, tris
             ---------------> | --------------> | ------------------------->
             every page the next tick touches is mapped again

### One execution across pages (proposed)

    engine: run .think of edict 57
      |   function value = page 6 : 1A40h
      v
    page 6 in a code slot?  no --> qglGemMap( code, 6, LRU slot )
      |
      v
    +-- code page 6 ----------------------------------+
    | 1A40h  header: parm_start locals nparms sizes   |
    |        statements                               |
    |          STORE  temp        (on this page)      |
    |          EQ_S   .classname == id 417  integer   |
    |          LOAD_F edict 57 .health  --------------+--> edict window, slot 3
    |          CALL   page 9 : 0200h  ----------------+--> page 9 mapped? LRU
    |          CALL   builtin 24 (sprint)  -----------+--> engine C; the text
    |                                                 |    page only here
    +-------------------------------------------------+

## What progs.dat holds

    progs.dat v6                               bytes   a tick needs
    +--------------------------------+--------+------------------------------
    | statements   20,939 x 8        | 167,512| a few pages of it
    | function records  2,091 x 36   |  75,276| nothing: inline headers
    | strings      7,304             |  88,336| 7,203 B, or none if interned
    | globals      11,471 words      |  45,884| see "Globals and entities"
    | 195 fields -> one edict        |     780| times 78..311 edicts a map
    +--------------------------------+--------+
    2,025 QuakeC functions; 65 builtins (engine code, no page cost)

## Executions and reach

An **execution** is one engine call into QuakeC: StartFrame,
PlayerPreThink, PlayerPostThink, a think, touch, use or blocked target,
or a spawn function. Its **reach** is every function it can call,
following calls one function at a time (`qcgraph.py`).

- **Stack depth.** The largest function is ClientObituary at 2,800 B.
  The heaviest call chain on the stack at once is 7,832 B:
  PlayerPostThink → W_Attack → … → T_Damage → Killed → SUB_UseTargets →
  lightning_use → SUB_CalcMove.
- **Reach.** Code in reach is the problem, not the stack.
  PlayerPostThink reaches 41,624 B and PlayerPreThink 37,664 B. The
  player's per-tick set is 49,264 B, three pages.
- **Files.** 1,157 caller→callee pairs cross a `.qc` boundary; 412 stay
  inside one. The median execution reaches one other file (`qcfiles.py`,
  `qchist.py`), so files are not a unit to pack by.

**Follow functions, not files.** subs.qc calls only ai.qc's `visible`,
yet a file-to-file closure had it reaching dog.qc through ai.qc's other
functions.

## Function pointers are closed

QuakeC calls through a variable only to reach thinks and callbacks. Every
target is found by following function values from the named functions
until nothing changes (`qcptr.py`):

    named function (a constant global)
         |
         |  STORE_*          global -> global
         |  STOREP_* via ADDRESS     -> entity field
         |  LOAD_*           field  -> global
         |  CALLn            argument word -> the callee's parameter word
         |  RETURN, STATE    -> return word, -> .think
         v
    locations: global words and entity fields, by field name
         |
         +--> a CALL through a global: callee set = what that global holds
              (a new callee wires new parameter edges; repeat to a fixed point)

The analysis finds:

- **Fields only.** Calls go through entity fields, never through a local
  or a parameter.
- **No function values escape.** No function returns one, and no op
  outside the followed set reads one.
- **Engine writes.** The engine's only write is OP_STATE → `.think`
  (`pr_exec.c`).
- **Maps.** No entity key in the 21 PAK0 maps names a function field.

So the call graph is exact before the game runs:

    field       targets   field        targets
    .think        1,561   .th_run/stand/walk  13 each
    .touch           38   .think1              11   door/plat/button "done"
    .th_die          20   .th_missile          11
    .use             17   .th_melee             8
    .th_pain         16   .blocked              5

Each of these traps produced a wrong answer first:

- **Argument stores.** qcc passes every argument with `STORE_V` (three
  words), whatever its type. Tracking `STORE_F`/`STORE_FNC` alone leaves
  `.think1` empty.
- **Shared parm words.** Every call writes the same parm words 4, 7,
  10…. Wire them per call site, or unrelated functions leak in
  (army_atk1 showed up in `.think1`).
- **GOTO.** Its first operand is a jump offset, not a global.
- **Opcode numbers.** `STATE` is 60 and `LOAD_FNC` is 29
  (`pr_comp.h`). Guessing 61 and 49 miscounted everything built on them.

## Code: one copy, fewest pages

Each function is placed on a page once and never copied. Pages hold
whole functions: 10.2 pages of code are packed into 14 pages, the rest
being slack. The placement should leave each execution touching as few
pages as possible (`qcopt.py`):

    cost = sum over executions of  rate x ( pages + 50 x max(0, pages - slots) )

    rate:  per-tick entry points 60/s   a think 10/s
           touch, blocked 1/s           spawn, level change 0.01/s

A tick starts cold, because the renderer, mixer and models remap the slots
every frame. Each distinct page an execution touches therefore costs one
map. Past the slot count the LRU swaps inside the execution, so those
pages carry the penalty.

With 3 code slots, counting all 1,721 executions:

| packing | executions over 3 pages |
|---|---|
| qcc's own statement order | 383 |
| greedy by cost | 104 |
| + local search (moves, swaps) | 86 |
| + simulated annealing | 75 |

- **Robustness.** The ranking holds with each rate ×3 and ÷3, with the
  penalty at 5, and with the player's entry points left out
  (`qcrescore.py`).
- **The player pair.** PlayerPreThink and PlayerPostThink stay at 4
  pages. They share 30,096 B and their union is 49,192 B. Pinning each to
  3 pages pushes 513 other executions over.
- **Copying, the rejected alternative.** Copying shared code into each
  group needing it (`qcseg.py`) fits every execution in 6 three-page
  groups. That takes 266,872 B of EMS for 167,512 B of code, with 2,516
  copies of 2,025 functions.

Run-time executions (1,605, at 15,831 runs/s modelled) by code slot count
(`qcslots.py`):

| code slots | executions over | runs/s over |
|---|---|---|
| 3 | 74 | 642 |
| 2 | 114 | 961 |

A layout with one fixed page for the running script and one swapped page
of shared code is itself a 2-slot packing, so it does no better than 114.
Only three run-time reaches exceed 32K, the player pair among them. The
rest go over because of where their code lands.

### What a real game does (traced)

QuakeSpasm with `tools/qcvm/quakespasm-trace.patch` replays id's three
demo routes with a bot, at 60 ticks/s. Each map runs at skill 1 and at
skill 3 with every monster woken at spawn. On all six runs the traced
statement counts equal QuakeSpasm's own per-function profile
(`qctrace.py`).

Per tick, mean (p99):

| map | statements, skill 1 | statements, all awake | code maps, 3 slots, qcc order: skill 1 / all awake |
|---|---|---|---|
| e1m3 | 372 (812) | 1,202 (6,921) | 6.0 (12) / 7.9 (31) |
| e1m4 | 394 (904) | 1,206 (6,568) | 6.0 (11) / 7.5 (31) |
| e1m6 | 270 (551) | 661 (3,441) | 5.5 (9) / 5.7 (18) |

The annealed packing loses at run time. On e1m3 at skill 1:

| packing | pages an execution, mean (max) | executions over 3 | pages a tick, mean (max) |
|---|---|---|---|
| qcc order | 1.69 (6) | 99 | 5.61 (9) |
| annealed for 3 | 1.56 (4) | 46 | 7.33 (13) |

It wins per execution and loses per tick. It spreads code over all 14
pages, and a tick pays for the union of its executions' pages. On every
trace it needs 20–50% more maps than qcc's order. A packing has to be
scored on the pages executions share within a tick, trained on one map
and checked on the others.

The all-awake arm is bimodal. Every monster was woken on the same tick,
so about 56 thinks land together one tick in six: 16.7% of ticks run
over 5,000 statements, and the rest average 1.5 thinks.

### Monsters only

The monsters-only progs.dat builds from the full cport's `qc/progs.src`
and `stubs.qc` with Quake-Tools' qcc. It keeps ai, fight, combat,
monsters, subs, world and seven monsters. The knight is not among them,
and e1m4 has knights.

    174,332 B image, every lump under 64K:
    statements 58,440 B (7,305)   functions 34,488 B (958)
    globals    18,380 B (4,595)   strings   42,796 B
    edict 764 B (191 words)       highest lump offset 155,952

Cut to it (`recut.py`): an execution counts as kept when every function
it enters is in the image, mixed when only some are. Each kept or mixed
execution is one C-to-QuakeC crossing. Per tick, mean (p99):

| run | kept statements | mixed statements | crossings |
|---|---|---|---|
| e1m3 skill 1 | 208 (597) | 152 (364) | 10.7 (18) |
| e1m3 all awake | 1,007 (6,556) | 157 (430) | 13.9 (70) |
| e1m4 skill 1 | 126 (477) | 229 (717) | 10.0 (19) |
| e1m4 all awake | 665 (4,147) | 470 (2,207) | 13.9 (72) |
| e1m6 skill 1 | 109 (312) | 148 (366) | 7.1 (12) |
| e1m6 all awake | 480 (3,180) | 150 (422) | 8.5 (34) |

The columns' p99s come from different ticks. Taken as one total a tick
(kept plus mixed), p50 and p99 are 346 and 793 statements on e1m3 skill 1
and 234 and 6,704 all awake; 312 and 849, and 250 and 6,282, on e1m4;
243 and 527, and 259 and 3,321, on e1m6.

The trace numbers functions by the progs.dat the engine ran, pak0's
413,116 B one. Read against the full cport's own 410,616 B build, every
name is wrong and the cut still looks plausible: mixed 68.5 on e1m3
skill 1 instead of 151.5. `recut.py` checks each name against the
trace's `fn` lines and refuses a mismatch.

The mixed executions are PlayerPostThink calling CheckRules, WaterMove,
CheckPowerups and W_WeaponFrame every tick, and on e1m4 the knight. The
dropped ones are item, door and trigger thinks and touches, which C owns.

### What the full cport's qvm.c costs

`qvm.c` needed five real-mode fixes before it ran under bcc `-mm`. Each
has a case in `qvmdos.c`, seen to fail with its fix taken out:

- `QVal far *a, *b, *c` makes only `a` far, so every result was written
  into DS.
- Near `strcmp` on far strings.
- Image offsets past 64K: a huge add at load, and a lump over 65,520 B
  is refused (id's full statements are 165,856 B).
- Entity values are `page << 16 | offset` over pages of at most 85
  edicts, with the page word masked.
- id's 100,000-statement runaway guard, with the trap unwinding to the
  caller's depth.

Timed in qvmb's harness on the monsters-only image (BC++ 3.1 `-mm -Ox -3
-f87`, 75,000 cycles, core normal). The hot set is 11 of the traced top
functions. `monster_ogre` spawns on edict 1 with its enemy a player-like
edict 2 (`setup.py`), so ai_run runs 96 statements and nothing calls a
NULL function. Both loops then run the same opcodes: 294 statements, 9
QuakeC calls and 23 builtin calls a round.

| µs a statement | qvmb loop | qvm_run | qvm_run, ported |
|---|---|---|---|
| 11 hot functions, measured | 0.816 | 1.186 | 0.859 |
| skill 1 mix, from the streams | 0.779 | 1.173 | 0.813 |
| all-awake mix, from the streams | 0.776 | 1.121 | 0.794 |

The ported `qvm_run` keeps every fix but uses qvmb's loop shape: a far
statement pointer, operands decoded inside each case, and traps by
`goto`. It is 5% slower than qvmb on the hot functions. Two of qvmb's
shortcuts account for its largest stream gaps. It dispatches a builtin
without calling it (0.40 µs, where the port's far call through `bi[]`
costs 1.02), and it compares strings as interned numbers (EQ_S 0.48 µs,
where `_fstrcmp` costs 0.99). Both costs are real. `qvm_test` fails if the port's jump
offsets, GOTO's operand, `MUL_FV`'s swap or the saved return address are
broken.

Each loop's single-opcode streams predict its own measured pass within
3% (qvmb) and 0.1% (`qvm_run`).

These timings use core normal because the dynamic core charges a `rep`
string copy as one instruction, whatever its count. qvmb's CALL0 costs
3.65 µs with 0 or 16 callee locals on dynamic, and 3.65 against 4.70 on
normal. qvmb copies locals with `_fmemcpy`, so its hot functions read
0.795 on dynamic against 0.816 on normal; `qvm_run` copies in a loop and
reads 1.186 on both. The difference is per statement: `qvm_run`
takes about 42 instructions to reach its dispatch, keeping `i`, `s` and
three decoded operand pointers in stack memory; qvmb's takes 9 and
decodes operands inside each case. GOTO costs 0.20 µs in qvmb's loop and
0.67 in `qvm_run`.

## Function records become inline headers

    now   function value = index -> dfunction_t[2,091], 36 B each, 75,276 B
          first_statement parm_start locals profile s_name s_file
          numparms parm_size[8]

    then  function value = page:offset of a 4-byte header; code follows it

          31        26 25   22 21  19 18   14 13               0
          +-----------+-------+------+-------+------------------+
          |  unused   | sizes | nprm | locals|    parm_start    |
          +-----------+-------+------+-------+------------------+
                       4 x 1b    3b     5b          14b
          a size bit: 0 = 1 word, 1 = 3 words

          builtin: the function value is a flag and its number, no header

- **Measured maxima** (`qcrecords.py`): parm_start 11,452, locals 16
  words, numparms 4, parm sizes 1 or 3 only, builtins 1..78.
- **Names and profile.** `s_name`, `s_file` and `profile` go to a name
  table on disk, read only for errors.

## Strings become ids

    88,336 B string table                                    (qcstr.py)
    +- 74,021 B  def and function names ............. disk, errors only
    +- 14,312 B  read by statements                  (qcstrsplit.py)
    |  +-  7,203 B  run time: compare, sound, setmodel, find, field stores
    |  +-  5,158 B  prints: bprint, sprint, centerprint, dprint, errors
    |  +-  1,951 B  load: precache_*, ambientsound
    +  map entity strings: 820..1,875 B unique a map (2,932..8,777 stored)

String compares take only three kinds of operand: a constant (124
operands), an entity field (142) or `mapname` (2). None is a local, a
parameter or a builtin's return. Every compared value is therefore known
at build time:

    build   QC constant "monster_ogre" --+
            map value   "monster_ogre" --+--> id 417
    run     compare, find()          integer ==
            sound, setmodel          id -> precache index
            bprint, centerprint      id -> text, the only reads of the page

One page holds every string a map uses: 14,312 + 1,875 = 16,187 B at
most. With interning, a tick maps that page only to print.

The built VM uses hashes of the text instead of ids, so progs.dat and the
maps need no shared table: see "The paged VM, built".

## Globals and entities

    globals 45,884 B                                        (qcglobals.py)
    +-    112 B  reserved: null, return, 8 parms
    +-    256 B  system, shared with the engine
    +-  2,340 B  function locals and parameters
    +-    120 B  named globals QuakeC writes
    +- 22,328 B  temps: each written by exactly one function
    +- 20,728 B  constants: never written

- **Shared globals**, meaning reserved, system and named, are 488 B.
  Every temp belongs to a single function, so it can live on that
  function's page.
- **Entities** (`qcpack.py`) take 780 B each. The count is taken on hard,
  less what the spawn code frees: `light` without a targetname,
  `info_null`, and the four makestatic torches. `NOT_DEATHMATCH` keeps an
  entity in single player.

      map    edicts    bytes      map    edicts    bytes
      start      80   62,400      e1m5      304  237,120
      e1m1      171  133,380      e1m6      244  190,320
      e1m2      248  193,440      e1m7       78   60,840
      e1m3      311  242,580      e1m8      172  134,160
      e1m4      292  227,760

  `find`, `findradius` and `nextent` walk all of them.
- **Shrinking an edict** (`qcfields.py`) barely helps. There are 151
  fields over 195 words, 77 of them the engine's. With strings, entities
  and functions as 16-bit ids, an edict drops from 780 B to 702 B; a
  function value fits because functions align to 4 bytes (page 4 bits +
  offset/4 12 bits). Dropping the 7 user fields nothing touches gives
  668 B. So entities need a paged store however they are encoded.
- **The sparse layout is the lever.** 59 of the 74 user fields are
  touched by fewer than 10 functions. How many fields each kind of entity
  uses needs a trace, not the static graph.

## The 4-slot layout, checked

    slot  proposed                        measured
    ----  ------------------------------  ----------------------------------
     0    code, the executing script      2-slot code: 114 run-time
     1    code, shared, swapped           executions over; 6% of runs/s;
                                          the player's pair always swaps
     2    strings, fixed                  fits: <= 16,187 B a map;
                                          interned, unused in a tick
     3    state and globals, fixed        does not fit: edicts 61..243K,
                                          temps and constants 43K

What is settled:

- **Call graph.** Exact and static.
- **Code placement.** One copy, packed by cost.
- **Function records.** Gone in this design; the built VM still reads
  one from an aux page at each call.
- **Strings.** Hashes of their text, built.

What is open:

- **Packing objective.** The trace shows the per-execution cost picks the
  wrong layout. Score per-tick co-occurrence instead, and model eviction at
  frame boundaries: qcport runs 1.73 ticks a frame on e1m3, so a tick
  after the first in a frame starts warm.
- **find and findradius.** Walking paged edicts costs thousands of maps on
  a tick that calls them. An index in conventional memory avoids the maps
  but costs conventional bytes; it is not sized.
- **Entity store.** 61K..243K needs its own paged store. It is not
  designed.
- **Constants and temps.** Whether constants can move beside their
  users, as temps can, is not measured.
- **"Fixed" slots** hold only within the VM's tick.

## The paged VM, built

`qvm/qvm.c` runs id's QuakeC with only the globals in conventional
memory. `make -C qvm/test` is its suite.

    window      holds                                  mapped again
    QVM_W_ST    the running function's statement page  CALL, RETURN, after a builtin
    QVM_W_AUX   function records, string text, index   at each record or text read
    QVM_W_ED0   edict page 0: the world, the player    each run, after a builtin
    QVM_W_EDX   any other edict page                   when an entity is elsewhere

- **Functions are placed whole.** The loader packs them in file order
  onto 16K statement pages and rewrites each record's first statement to
  `page << 16 | statement`. A jump never leaves its page, so only CALL and
  RETURN remap. A function past 2,048 statements is refused, and so are
  first statements that fall as the function number rises. pak0 packs
  into 11 pages.
- **An entity is `page << 16 | byte offset`**, 21 edicts of 780 B to a
  page on pak0. QuakeC only compares entities, tests them against 0 and
  adds field offsets, and all three still hold. An offset past the last
  place a vector fits addresses page 0, never another window.
- **Nothing keeps a window.** A builtin, or C between runs, may map any
  slot. The VM asks for its own again at each run and after every builtin,
  and a return address is a placement, not a pointer. C reaches entities
  through `qvm_ent_get` and `qvm_ent_set`.
- **Asking costs a compare.** The host publishes its record of each
  window, gem's `qgl$gem_key`: page << 8 | the handle's low byte. The VM
  compares it inline and calls `map` only when it differs; through the
  host, those were 129 far calls a round of real QuakeC, 6 of which
  changed a page. On ARMD's monster rounds (core=normal, cycles 75000,
  every entity on page 0) that took 1.3683 µs a statement to 1.2238,
  against 0.8586 unpaged, with the same 294 statements a round. A key holds a page and a handle in a byte each, so
  anything past 255 pages is refused. `qglGemFree` forgets a freed
  handle's keys, or a handle number EMM hands straight back would match.
- **id's runaway guard**, 100,000 statements a run, and every trap unwinds
  to the depth the run started at.

### Strings: a hash to compare, text to print

`tools/qcvm/qchash.py` rewrites progs.dat. A string value becomes the
FNV-1a 32 hash of its text, so EQ_S compares 32 bits and a tick never maps
text. The text stays, deduplicated (88,336 B to 46,998 B on pak0), with an
index sorted by hash for `qvm_str_text`. The image carries its own
version, and `qvm_load` refuses id's and every earlier rewrite's.

    value          text         EQ_S                NOT_S  IF
    0              null         equals ""           1      false
    QVM_STR_EMPTY  ""           equals null         1      true, as id's offset is
    QVM_STR_TEMP   ftos, vtos   equals only a temp  0      true
    a hash         its text     equal text          0      true

- **The temp is id's one buffer.** Two temps compare equal whatever they
  hold, as id's two results share a pointer. A temp equals no constant,
  even one with its text, where id compares text. Stock QuakeC only
  prints what ftos returns.
- **The rewrite refuses** two texts with one hash; a text hashing to 0,
  TEMP, EMPTY or 0x80000000, which is -0.0 to AND and OR; a nonzero global
  no def covers, a vector def covering three; a string global not at the
  start of a string; and a global read as a string that is not typed as
  one. pak0 and the full and monsters-only cport builds pass.
- **Owed by the engine's string writers**, not built: map keys hashed
  after `ED_NewString`'s unescape; C's own name literals through the same
  gate, their constants generated from it; `precache_*`, `setmodel` and
  `sound` hashing a temp's current text, and a name missing from the
  resource table trapping by name.

### Tests

`qvm/test/qvm_test.c` hand-assembles bytecode and loads it through the
real loader. Natively, the EMS stand-in copies each page in and out of its
window, so a pointer kept across a remap reads the wrong bytes there too;
in DOS it is INT 67h (`qvm/test/dosconf.sh`). It then refuses pak0's
progs.dat, loads its rewrite and checks every function's placement.
A second run on pages already mapped must call `map` 0 times.
`make -C qvm/test mutate` builds 40 mutants of `qvm.c`, two of the EMS
stand-in and 13 of `qchash.py`, and each must fail; a host mutant counts
only if its own section fails.

## How the numbers were made

    PAK0.PAK
       |
       v
    qcpack.parse ------------+--------------------+-----------------+
       | names, sizes,       |                    |                 |
       | bodies, calls       v                    v                 v
       |              qcptr.resolve        qcstr.string_uses  qcglobals.writers
       |               value flow ->        who reads each    who writes each
       |               field targets,       string            global
       |               exact call graph           |                 |
       |                     |                    +--------+--------+
       |                     v                             v
       |              qcopt.executions         qcstrsplit: run/print/load,
       |               reach + rate a root      compares, map strings
       |                     |
       |                     v
       |              qcopt.Packing: greedy -> local search -> annealing
       |                     |
       |                     +--> qcopt      packings, 3 slots
       |                     +--> qcslots    3 vs 2 slots, run time
       |                     +--> qcrescore  ranking under changed weights
       |
       +--> qctrace + QuakeSpasm trace (quakespasm-trace.patch)
       |          statements, executions, page maps a tick, per packing
       +--> qcgraph, qchist, qcfiles   sizes, reach, file crossings (by name)
       +--> qcpack, qcpack2, qcseg     first packings, edicts, copy-per-group
    qcrecords (standalone)            the inline header's ranges
