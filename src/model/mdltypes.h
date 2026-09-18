#ifndef __MDLTYPES_H__
#define __MDLTYPES_H__

#include "bsptypes.h"

/*
 * mdltypes.h -- MdlState and MdlEnt alone, so world.h can hold them
 * without including mdl.h, which includes world.h. Everything else
 * about a model is mdl.h's.
 */
#define MDL_MAXV      324        /* boss.mdl's 325 vertices, 0..324 */
#define MDL_TRI_BYTES 18
#define MDL_KINDS     8

/* A model's frame sets, contiguous in its vertex pages in this order:
   a monster's as mkassets.py's MDL_SETS writes them, Chthon's as
   BOSS_MDL does, and a view model's one. */
#define MDL_NSETS      16
#define MDL_SET_STAND   0
#define MDL_SET_RUN     1
#define MDL_SET_DEATH   2
#define MDL_SET_PAIN    3
#define MDL_SET_ATK     4
#define MDL_SET_WALK    5
#define MDL_SET_DEATH2  6
#define MDL_SET_PAIN2   7
#define MDL_SET_PAIN3   8
#define MDL_SET_PAIN4   9
#define MDL_SET_PAIN5  10
#define MDL_SET_ATK2   11
#define MDL_SET_ATK3   12
#define MDL_SET_LEAP   13
#define MDL_SET_CRUC   14
#define MDL_SET_ATK4   15
#define MDL_MON_NEED    2       /* stand and run: the AI cycles modulo both */
#define MDL_BOSS_RISE   0
#define MDL_BOSS_WALK   1
#define MDL_BOSS_DEATH  2
#define MDL_BOSS_ATTACK 3
#define MDL_BOSS_SHOCKA 4
#define MDL_BOSS_SHOCKB 5
#define MDL_BOSS_SHOCKC 6
#define MDL_BOSS_SETS   7
#define MDL_VIEW_SETS   1

/* the view weapons, in the order host_view_load and d_draw_view index
   them -- v_shot, v_shot2, v_nail, v_rock, v_nail2, v_rock2, v_axe */
#define PL_VIEW_SHOT   0
#define PL_VIEW_SSG    1
#define PL_VIEW_NAIL   2
#define PL_VIEW_GL     3
#define PL_VIEW_SNG    4
#define PL_VIEW_RL     5
#define PL_VIEW_AXE    6
#define PL_VIEW_N      7

/* MDL_KIND_*, mkassets.py's own numbering */
#define MDL_KIND_ARMY     0
#define MDL_KIND_KNIGHT   1
#define MDL_KIND_DOG      2
#define MDL_KIND_OGRE     3
#define MDL_KIND_DEMON    4
#define MDL_KIND_ZOMBIE   5
#define MDL_KIND_WIZARD   6
#define MDL_KIND_SHAMBLER 7

/* mkmdl.py's .geo header, 60 bytes. Read whole and picked apart, so
   the field order here is the file's. */
#define MDL_MAGIC   0x34444D51L     /* "QMD4": sixteen semantic animation slots */
typedef struct {
    long  magic;
    short ntri, nvert, nframe;
    short skin_w, skin_h;
    BspVec3 scale;              /* vertex byte -> model unit */
    BspVec3 origin;
    short nsets;
    unsigned char set_n[MDL_NSETS];     /* frames a set, 0 past nsets */
    short vpages, page_frames;  /* vertex pages 0..vpages-1, whole frames a page */
} MdlHead;

#define REC_MDLHEAD 60
typedef char rec_mdlhead_ok[ sizeof(MdlHead) == REC_MDLHEAD ? 1 : -1 ];

/* One .geo triangle: three vertex indices and a texture corner for each. */
typedef struct { short a, b, c, u1, v1, u2, v2, u3, v3; } MdlTri;
typedef char rec_mdltri_ok[ sizeof(MdlTri) == MDL_TRI_BYTES ? 1 : -1 ];

/* One loaded model, shared by every monster of its kind. */
typedef struct {
    short   ntri, nvert;
    unsigned char nframe, loaded;
    QSurf   skin;
    short   vtx_hnd;            /* qglGemAlloc: vertex pages, then one of triangles */
    BspVec3 scale, origin;
    float   radius, zlo, zhi;   /* the box any frame at any yaw fits in */
    unsigned char set_bits[10]; /* sixteen 5-bit frame counts */
    short   vpages, page_frames;
} MdlState;
typedef char rec_mdlstate_ok[ sizeof(MdlState) == 62 ? 1 : -1 ];

/* A small bundled alias model lives in far memory. Armor shares its
   geometry but has three skins; other uses leave the extras zero. */
typedef struct {
    MdlState state;
    QSurf    skin1, skin2;
    short    id;
} AliasModel;

/* One model in effects.bin's shared geometry and skin banks. */
typedef struct {
    MdlState state;
    long     skin_ofs;
    short    skin_w, skin_h;
} EffectModel;

/* effects.bin order. Every projectile selects one from its existing
   gameplay flags, so Spike gains no render-only state. */
#define MDL_FX_SPIKE     0
#define MDL_FX_SSPIKE    1
#define MDL_FX_GRENADE   2
#define MDL_FX_MISSILE   3
#define MDL_FX_LAVABALL  4
#define MDL_FX_WSPIKE    5
#define MDL_FX_ZOMGIB    6
#define MDL_FX_GIB1      7
#define MDL_FX_GIB2      8
#define MDL_FX_GIB3      9
#define MDL_FX_HPLAYER  10
#define MDL_FX_HGUARD   11
#define MDL_FX_HKNIGHT  12
#define MDL_FX_HDOG     13
#define MDL_FX_HOGRE    14
#define MDL_FX_HDEMON   15
#define MDL_FX_HZOMBIE  16
#define MDL_FX_HWIZARD  17
#define MDL_FX_HSHAMS   18
#define MDL_FX_BOLT     19
#define MDL_FX_BOLT3    20
#define MDL_FX_N        21

/* One spawned monster: everything mdl_think owns, separate from the
   MdlState a whole kind shares. BSP space, Z up, like Player.pos, so it
   can be handed to pl_trace with no copy. */
typedef struct {
    BspVec3 pos;
    float   yaw;                /* Quake's own, CCW from +x -- not mirrored */
    float   ideal_yaw;          /* what change_yaw turns towards */
    short   kind;
    short   order;              /* source edict order, for findradius/owner */
    short   state;              /* MDL_ST_* */
    short   anim_frame;         /* within the set state selects */
    float   next_think;         /* anim_time of the next 10 Hz think */
    BspVec3 goal;               /* the wander or patrol destination */
    float   stand_until;        /* anim_time to leave STAND on */
    short   wander_ticks;       /* think-ticks spent on this goal */
    short   health;
    short   hunting;            /* has seen the player */
    float   next_attack;
    float   flash_until;        /* the volley's muzzle flash, until then */
    float   pain_finished;      /* no new flinch before this */
    BspVec3 spawn;
    BspVec3 vel;                /* a leaper's, MOVETYPE_STEP off the ground */
    signed char leap_touch;     /* landed its damage, or MDL_THROWN */
    unsigned char anim_set;     /* MDL_SET_*, separate from behavioural state */
    short   patrol, corner;     /* the map's path_corner, and the one bound for */
    float   idle_at;            /* the next roll for an idle sound */
    short   water;              /* SV_CheckWaterTransition's: 1 in a liquid,
                                   -1 out, 0 before its first look */
    short   cruc;               /* SPAWN_CRUCIFIED: still, moaning, unhurt */
} MdlEnt;

#define REC_MDLENT 104
typedef char rec_mdlent_ok[ sizeof(MdlEnt) == REC_MDLENT ? 1 : -1 ];


#endif
