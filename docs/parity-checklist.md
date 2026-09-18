# Parity with id's Quake

What qcport still lacks against id's single-player Quake on the shareware
PAK0: start, e1m1..e1m8. `id:` is the WinQuake engine source
(github.com/id-Software/Quake) or, for QuakeC, the `.qc` file progs.dat
names for that function. `qcport:` is this repo.

Counts are entities left on easy or above, read from the maps' entity lumps.
Fish, enforcer, hell knight, shalrath, tarbaby and their maps are not in
PAK0 and are left out.

## Rendering

- [ ] Per-pixel liquid warp
      id: `d_scan.c` `Turbulent8`, `D_DrawTurbulent8Span`
      qcport (partial): per-vertex sine displacement, `src/render/d_faces.c:360`
- [x] Underwater view warp
      id: `d_scan.c` `D_WarpScreen`, gated by `r_main.c` `r_dowarp`
      qcport: a two-axis row-dispatch warp over the EMS render surface;
      `src/render/vid.c`
- [x] Screen colour shifts: damage, pickup, liquids, powerups
      id: `view.c` `V_CalcBlend`, `V_ParseDamage`, `V_SetContentsColor`,
      `cshift_water`/`cshift_slime`/`cshift_lava`
      qcport: `src/render/vshift.c`
- [x] View bob and roll
      id: `view.c` `V_CalcBob`, `V_CalcRoll`
      qcport: `src/render/viewfx.c`
- [x] Alias model lighting
      id: `r_alias.c` `R_AliasSetupLighting`
      qcport: face-normal colormap shading copied before the triangle page
      remaps its EMS slot; `src/render/d_alias.c`
- [x] Particles: blood, gunshots, explosions, trails, teleport and lava
      splashes
      id: `r_part.c` `R_ParticleExplosion`, `R_RocketTrail`,
      `R_BlobExplosion`, `R_RunParticleEffect`, `R_LavaSplash`,
      `R_TeleportSplash`, `R_ParseParticleEffect`
      qcport (partial): one 4 KB EMS pool draws id-style explosion, blood,
      gunshot, nail and wizard-spike particles, plus rocket, grenade and gib
      trails and teleport splashes, with depth testing; `src/render/part.c`.
      Chthon's death emits an evenly sampled `R_LavaSplash` volume into
      the same one-page EMS pool.
- [ ] Sprites: `s_explod.spr`, `s_bubble.spr`, `s_light.spr`
      id: `r_sprite.c` `R_DrawSprite`
      qcport: not loaded
- [ ] Temp entities and beams: explosions, Chthon's and the shambler's
      lightning, the lightning gun
      id: `cl_tent.c` `CL_ParseTEnt`, `CL_ParseBeam`, `CL_UpdateTEnts`
      qcport (partial): the shambler's bolt hurts and draws nothing,
      `src/game/mdl_ai.c:460`
- [x] Item and backpack models
      id: each classname's `precache_model` in `items.qc`; the `.mdl` files
      are in PAK0
      qcport: alias models for armor and all three skins, keys by worldtype,
      weapon pickups, powerups, the rune and dropped backpacks; ammo and
      health retain their original `b_*.bsp` models. Soldiers toss five-shell
      backpacks and ogres toss two-rocket backpacks; both land through hull 1,
      expire after 120 seconds and release cleanly; `src/render/d_alias.c`,
      `tools/test-backpacks.sh`
- [x] Projectile models
      id: `weapons.qc`; the `.mdl` files are in PAK0
      qcport: normal and super spikes, wizard spikes, grenades, rockets,
      lava balls and zombie chunks use their original EMS-backed alias
      models; `effects.bin`, `src/render/d_alias.c`
- [x] Gib and head models
      id: `player.qc`; the `.mdl` files are in PAK0
      qcport: overkill replaces players and all supported monsters with
      their original head and three original gib models. Velocity scaling,
      bounce, spin, blood trails and 10--20 second gib lifetime match
      `ThrowHead`/`ThrowGib`; transient state shares the renderer's existing
      EMS frame page and survives save/load; `src/game/gib.c`,
      `tools/test-gibs.sh`
- [ ] Full monster animation sets
      id: each monster's `.qc` (`soldier.qc`, `dog.qc`, ...) steps every
      frame by name
      qcport (partial): one EMS page a model keeps no walk set, no soldier or
      dog attack, no leap frames (`tools/mkassets.py:93`); a leap plays the
      run cycle, `src/render/d_alias.c:584`
- [x] Chthon drawn and fighting
      id: `boss.qc` `monster_boss`, `boss_missile`, `lightning_use`
      qcport: the loaded alias model rises from an id-style lava splash,
      faces the player, animates rise, attack, shock and death, and fires the
      paired 300-unit/s lava balls from attack frames 9 and 20. Lightning
      counts bolts and fires the exit; `tools/test-boss.sh`.
- [x] Runtime view size and fov
      id: `screen.c` `scr_viewsize`, `SCR_SizeUp_f`, `scr_fov`
      qcport: archived `viewsize`/`fov` cvars and `sizeup`/`sizedown`, with
      id's centered 30..120 layouts; `src/render/vid.c`, `src/host/host.c`

## Gameplay

- [x] Axe
      id: `weapons.qc` `W_FireAxe`; `v_axe.mdl`
      qcport: Quake's 4096 bit, 64-unit hull-0 trace from `origin + 16`,
      20 damage, view model and original swing/flesh/world sounds;
      `src/game/weapons.c`, `tools/test-guns.sh`
- [ ] Lightning gun and cells: `weapon_lightning` (1, start),
      `item_cells` (3, start), all deathmatch-only
      id: `weapons.qc` `W_FireLightning`, `LightningDamage`; `items.qc`
      `weapon_lightning`, `item_cells`
      qcport (partial): cells are taken, 6 or 12 to 100, from `b_batt`
      crates, on dm3ish in `tools/test-items.sh`. Untested, since no
      shareware map loads a cell box and no run starts with 100 cells:
      the carry in `CARRY.BIN`, and `item_take_cells` refusing only at
      200 (`ammo_touch`). No lightning gun
- [x] Pentagram still drains armor
      id: `combat.qc` `T_Damage` takes the armor save before the
      `invincible_finished` return
      qcport: `combat_damage_player`; `tools/test-fall.sh` arm 3, e1m8's armor,
      pentagram and lava
- [x] Ring of Shadows: `item_artifact_invisibility` (3: e1m3, e1m4, e1m8)
      id: `items.qc` `item_artifact_invisibility`; `client.qc`
      `CheckPowerups`; `ai.qc` `FindTarget`; `player.qc` `player_pain`;
      `r_main.c` `R_DrawViewModel`
      qcport: `ENT_ITEM_INVIS`; `tools/test-items.sh` on e1m3. Not ported:
      the whisper's half volume and the eyes model
- [x] T_RadiusDamage as id runs it
      id: `combat.qc` `T_RadiusDamage`, `CanDamage`; `pr_cmds.c` `PF_findradius`
      qcport: source-edict order, damage + 40 cutoff, hull-0 `CanDamage`,
      attacker and shambler halving, chained barrels, and shootable brushes;
      `tools/test-guns.sh`, `tools/test-shoot-brushes.sh`
- [x] Monsters see through hull 0
      id: `ai.qc` `visible`, a hull-0 point `traceline` eye to eye
      qcport: `pl_trace_point`; e1m2 `-at -150 625 340` is
      `tools/test-ai-visible.sh`
- [x] Fractional damage ceiled, not truncated
      id: `combat.qc` `T_Damage` takes float damage, `take = ceil(damage - save)`
      qcport: damage stays float through armor and final `ceil`, including the
      x87 float32 barrier; `tools/test-combat-round.sh`
- [x] Red armor: `item_armorInv` (1, start, deathmatch only: spawnflags 1792)
      id: `items.qc` `item_armorInv`
      qcport: `ENT_ITEM_ARMOR3`, 200 at 0.8; dm3ish's arms in
      `tools/test-items.sh`; red status-bar icon in `tools/test-sbar.sh`
- [x] Rune carried back to start
      id: `items.qc` `sigil_touch` sets `serverflags`; the engine keeps them
      across maps, `sv_main.c` `svs.serverflags`; `misc.qc`
      `func_episodegate`, `func_bossgate` read them
      qcport: `Fight.serverflags` in `CARRY.BIN`, `-sflags`; the gates are
      `EntsGate` records and start2 rides the header;
      `tools/test-serverflags.sh`; rune icons in `tools/test-sbar.sh`
- [x] `trigger_monsterjump` (1, e1m6)
      id: `triggers.qc` `trigger_monsterjump_touch`
      qcport: `ent_monster_jump`, `tools/test-monsterjump.sh`. A thrown
      monster runs on landing and does not attack in the air
- [ ] `air_bubbles` (2, e1m4)
      id: `misc.qc` `air_bubbles`, `make_bubbles`
      qcport: no case, dropped
- [ ] Player death animation
      id: `player.qc` `PlayerDie`, `player_diea1`, `GibPlayer`, `ThrowGib`;
      `client.qc` `respawn`
      qcport (partial): "YOU DIED", original overkill gibs/head and a reset
      respawn, but no ordinary death frames,
      `src/host/host.c:139`, `:142`
- [x] Water jump
      id: `client.qc` `CheckWaterJump`
      qcport: hull-0 waist/upper probes, 225 up and two-second state;
      `tools/test-waterjump.sh`
- [x] Skill chosen in game
      id: `host.c` `skill` cvar, set by start's `triggers.qc`
      `trigger_skill_touch` (4, start)
      qcport: `ents<N>.bin` a skill, `ENT_TRIG_SKILL`, carried in
      `CARRY.BIN`, `-skill`; `tools/test-skill.sh`. Nightmare plays hard
- [x] Episode end text
      id: `client.qc` `ExitIntermission` sends the finale on e1m7;
      `sbar.c` `Sbar_FinaleOverlay`
      qcport: finale text, sell screen, then start; fire or jump advances;
      `tools/test-finale.sh`
- [x] **Bug:** every slipgate on start leads to the last one parsed
      id: `client.qc` `changelevel_touch` reads its own trigger's `map`
      qcport: each changelevel's map is a message, copied on touch;
      `tools/test-exit.sh` arm 4. Not ported: NO_INTERMISSION, its targets

## Front end and engine

- [x] Title demos
      id: `cl_demo.c` `CL_PlayDemo_f`, `cl_main.c` `CL_NextDemo`;
      `demo1..3.dem` are in PAK0
      qcport: no-argument `startdemos demo1 demo2 demo3`, map-aware QDC1
      camera playback extracted from id's protocol-15 demos, with id's
      wrapping/suspended menu loop. QCR3 remains the deterministic input
      recorder; the runtime does not implement `.dem` network snapshots;
      `tools/test-title-demos.sh`
- [x] Menus, save and load
      id: `menu.c` `M_Menu_Main_f`, `M_Menu_Options_f`, `M_Menu_Keys_f`,
      `M_Menu_Load_f`, `M_Menu_Save_f`; `host_cmd.c` `Host_Savegame_f`,
      `Host_Loadgame_f`
      qcport: original main, single-player, options, keys and twelve-slot
      load/save menus; binary snapshots preserve the port's mutable world
- [x] Console and cvars
      id: `console.c` `Con_Init`, `cvar.c` `Cvar_RegisterVariable`
      qcport: queued keyboard input, scrollback/history/completion, bounded
      command buffer, archived `skill`/`fov` cvars and `+commands`;
      `tools/test-console.sh`, `tools/test-console-dos.sh`
- [x] Key bindings and `config.cfg`
      id: `keys.c` `Key_WriteBindings`, `host.c` `Host_WriteConfiguration`,
      `cmd.c` `Cmd_Exec_f`
      qcport: Quake key names and logical `+button` bindings, streamed nested
      `exec`, default/config/autoexec startup order and archived config writes;
      `tools/test-keybind.sh`, `tools/test-keybind-dos.sh`
- [x] Full status bar: weapons row, all ammo, keys, runes, powerups,
      armor type
      id: `sbar.c` `Sbar_Draw`
      qcport: id's WAD art in one EMS atlas, 48/24/0-line layouts,
      pickup and pain faces, solo score hold and death scorebar;
      `tools/test-sbar.sh`
- [x] Intermission screen
      id: `sbar.c` `Sbar_IntermissionOverlay`
      qcport: original `complete.lmp`/`inter.lmp` artwork and large time,
      secrets and monsters tallies; `src/ui/sbar.c`
- [x] Multi-line centerprint
      id: `screen.c` `SCR_CenterPrint`
      qcport: source newlines and blank rows, id's 40-character clipping,
      vertical anchors and two-second real-time expiry; messages share the
      map's NUL string pool; `tools/test-centerprint.sh`,
      `tools/test-centerprint-dos.sh`
- [x] Stereo sound
      id: `snd_dma.c` `SND_Spatialize`
      qcport: id's distance and left/right dot-product attenuation into an
      interleaved 8-bit SB16 ring; `tools/test-stereo.sh`
- [ ] CD music
      id: `cd_audio.c` `CDAudio_Play`
      qcport: none
- [ ] Deathmatch, coop and netplay
      id: `sv_main.c`, `net_main.c`, `host.c` `deathmatch`/`coop` cvars
      qcport: none; `info_player_deathmatch` and `info_player_coop` are
      dropped

## Done

Every episode 1 monster but Chthon, drawn and fighting; the shotgun,
super shotgun, both nailguns and both launchers; health, ammo, green and
yellow armor, keys, quad, pentagram, biosuit and megahealth rot; doors,
secret doors, plats, trains, buttons, teleporters and the other triggers;
spike shooters and fireballs; swimming, drowning, slime and lava;
dynamic lights and light styles; the sound mixer and ambients; map
changes with the kit carried.
