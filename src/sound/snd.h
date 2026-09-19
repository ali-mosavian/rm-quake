#ifndef __SND_H__
#define __SND_H__

#include "bsptypes.h"
#include "pl_move.h"
#include "pak.h"
#include "sound.h"

/*
 * snd.h -- the sound layer: Quake's snd_dma.c stereo spatializer.
 *
 * qglDspInit starts a Sound Blaster playing an 8192-byte ring in
 * auto-init DMA; snd.bsc, mksnd.py's concatenation of the game's wavs
 * as bsc4/32n, goes into one EMS handle a page at a time and
 * snddec.raw is the table it is decoded through. The SB ISR gives each
 * completed 4096-byte half to snd_mix.c, which refills it before the DSP
 * acknowledges the interrupt.
 */

/* the sounds, in tools/mksnd.py's SOUNDS order -- q_pl.bi's own
   numbering, and sndtab.raw's; SND_COUNT is sound.h's. SND_MON + kind * 4
   is a monster's four for kinds 0..4, SND_MON2 + (kind - 5) * 4 for the
   rest. */
#define SND_SHOTGUN     0
#define SND_SSG         1
#define SND_NAIL        2
#define SND_BOOM        3
#define SND_HEALTH      4
#define SND_HEALTH_ROT  5
#define SND_HEALTH_MEGA 6
#define SND_ARMOR       7
#define SND_WEAPON      8
#define SND_AMMO        9
#define SND_QUAD        10
#define SND_SUIT        11
#define SND_SECRET      12
#define SND_TALK        13
#define SND_PAIN1       14    /* three of them, and SND_PAIN4's three */
#define SND_DEATH       17
#define SND_JUMP        18
#define SND_LAND        19
#define SND_LAND2       20
#define SND_SLIME       21
#define SND_BURN1       22    /* two */
#define SND_MON         24
#define SND_DOOR        44    /* doors.qc's 1..4: stop, move */
#define SND_SECRET1     52    /* func_door_secret's noise1..3 */
#define SND_BUTTON      61    /* func_button's 0..3 */
#define SND_KEY         67    /* a key taken: medieval, rune */
#define SND_KEYTRY      69    /* a key door refused then opened; rune is +2 */
#define SND_TRAIN       76    /* plats/train2 the stop, train1 the move */
#define SND_SPIKE2      78    /* trap_spikeshooter */
#define SND_DJUMP       79    /* the demon's leap */
#define SND_GRENADE     80    /* the ogre's grenade thrown, and bounced */
#define SND_BOUNCE      81
#define SND_MON2        82    /* the zombie's and the wizard's four, kinds 5 up */
#define SND_ROCKET      94    /* weapons/sgun1 */
#define SND_SHAM_MELEE  95
#define SND_SHAM_SMACK  96
#define SND_SHAM_BOOM   97
#define SND_PENT        98
#define SND_PENT_HIT    99
#define SND_WATER       100   /* the leaves' ambients */
#define SND_WIND        101
#define SND_PLAT        102   /* func_plat's sounds 1, 2: move, stop */
#define SND_TELE        106   /* play_teleport's five */
#define SND_TRIGGER     111
#define SND_H2OHIT      112
#define SND_FIRE        113   /* FireAmbient */
#define SND_FLUORO      114
#define SND_SPARK       115
#define SND_PAIN4       116
#define SND_DEATH2      119   /* four */
#define SND_H2ODEATH    123
#define SND_DROWN1      124   /* two */
#define SND_GASP1       126   /* two */
#define SND_INH2O       128
#define SND_INLAVA      129
#define SND_OUTWATER    130
#define SND_H2OJUMP     131
#define SND_UDEATH      132
#define SND_GIB         133
#define SND_QUAD_END    134
#define SND_SUIT_END    135
#define SND_PENT_END    136
#define SND_QUAD_SHOT   137
#define SND_RIC1        138   /* three */
#define SND_TINK        141
#define SND_SWIM1       142   /* two */
#define SND_ARMY_IDLE   144
#define SND_ARMY_PAIN2  145
#define SND_KNIGHT_IDLE 146
#define SND_SWORD2      147
#define SND_DOG_IDLE    148
#define SND_OGRE_IDLE   149
#define SND_OGRE_IDLE2  150
#define SND_OGRE_DRAG   151
#define SND_DEMON_IDLE  152
#define SND_Z_IDLE1     153
#define SND_Z_HIT       154
#define SND_Z_MISS      155
#define SND_Z_FALL      156
#define SND_Z_PAIN1     157
#define SND_WIZ_IDLE1   158   /* two */
#define SND_WIZ_HIT     160
#define SND_SHAM_IDLE   161
#define SND_SHAM_MELEE2 162
#define SND_BOSS_OUT    163
#define SND_BOSS_SIGHT  164
#define SND_POWER       165
#define SND_BOSS_PAIN   166
#define SND_BOSS_DEATH  167
#define SND_CRUC_IDLE   168
#define SND_INV         169   /* the ring taken, fading, its whisper */
#define SND_INV_END     170
#define SND_INV_IDLE    171
#define SND_DEMON_LAND  172
#define SND_AX_SWING    173
#define SND_AX_MEAT     174
#define SND_AX_WALL     175
#define SND_BOSS_THROW  176
#define SND_LSTART      177
#define SND_LHIT        178

/* sound()'s attenuations and entity channels, id's numbers */
#define ATTN_NONE   0
#define ATTN_NORM   1
#define ATTN_IDLE   2
#define ATTN_STATIC 3
#define CHAN_AUTO   0
#define CHAN_WEAPON 1
#define CHAN_VOICE  2
#define CHAN_ITEM   3
#define CHAN_BODY   4

/* entity numbers for the channels: the player, a monster by its index,
   a brush by its submodel */
#define SND_ENT_MON   1024
#define SND_ENT_BRUSH 256
#define SND_ENT_TRIG  2048    /* a point entity, by its trigger index */

/*
 * name: snd_init
 * desc: The card, snd.bsc into EMS, and the mixer over dsp.asm's ring.
 *       Quiet and harmless on a machine with no card, on -nosound, or
 *       with no sounds in the container: every call below then returns
 *       at once.
 */
void snd_init( Sound *snd, Pak *pak, short off );

/* room for the map's n static points, before the first snd_ambient */
void snd_statics( Sound *snd, short n );

/*
 * name: snd_ambient
 * desc: ambientsound: recorded at map load, before the card is up,
 *       started with it and looped for good. The mixer re-places it
 *       from the player every frame.
 */
void snd_ambient( Sound *snd, short id, short vol, BspVec3 *org );

/*
 * name: snd_start
 * desc: sound(): id from ent's chan at org, full volume less the
 *       distance's share of a thousand units over attn. A new sound on
 *       an entity's channel (not CHAN_AUTO) ends the one there, which is
 *       how a mover's stop ends its looping move.
 */
void snd_start( Sound *snd, Player *player, short ent, short chan, short id, BspVec3 *org, short attn );
void snd_start_volume( Sound *snd, Player *player, short ent, short chan, short id,
                       BspVec3 *org, short attn, short volume );
void snd_stop( Sound *snd, short ent, short chan );

void snd_play( Sound *snd, Player *player, short id, BspVec3 *org );
void snd_self( Sound *snd, Player *player, short chan, short id );

/*
 * name: snd_frame
 * desc: Once a frame, update spatialisation and leaf ambient gains. The
 *       ISR callback owns decoding and painting; dt only drives the
 *       ambient fade.
 */
void snd_frame( Sound *snd, World *world, Player *player, Camera *cam, float dt );
void snd_volume( Sound *snd, float volume );

void snd_shutdown( Sound *snd );

/* -sndsum's: every sound decoded through the mixer's own fetch and
   summed. See snd_mix.h. */
unsigned long snd_sum( Sound *snd );

/* -sndsum regression: run decode and IRQ paint while another mapping
   owns the sample slot, then verify both put that mapping back. */
short snd_sum_slot_test( Sound *snd, unsigned long *sum );

/* for bench.txt: sounds started, ambients looping, and the legacy
   underrun field. IRQ refill has no foreground polling window, so the
   latter remains zero. */
void snd_stats( const Sound *snd, short *started, short *loops, short *under );

/* the run's sound record, one line: every id started as a bitmap in hex,
   the ids on the dynamic channels now, loop wraps, the leaf ambients'
   volumes and the statics */
void snd_report( const Sound *snd, char *buf );

#endif
