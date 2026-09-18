#ifndef __ITEM_H__
#define __ITEM_H__

#include "world.h"
#include "pl_move.h"
#include "fight.h"
#include "renderer.h"

/*
 * item.h -- the pickups. q_ent.bi's ItemEnt and pl_move.bas's
 * ent_items_drop / pl_items_touch: what the map put down, dropped to the
 * floor once at load, and taken when the player's box reaches it.
 *
 * They live in World because that is where the map's entity arrays live,
 * even though `gone` is state -- a door's `state` is next to its brush
 * for the same reason.
 */
#define ENT_ITEM_HEALTH   0
#define ENT_ITEM_SHELLS   1
#define ENT_ITEM_ARMOR1   2      /* green, 100 at 0.3; amount is the value */
#define ENT_ITEM_ARMOR2   3      /* yellow, 150 at 0.6 */
#define ENT_ITEM_SSG      4      /* weapon_supershotgun; amount is its shells */
#define ENT_ITEM_NAILS    5      /* item_spikes, 25 or 50 */
#define ENT_ITEM_NAILGUN  6
#define ENT_ITEM_QUAD     7      /* amount is its seconds */
#define ENT_ITEM_SUIT     8
#define ENT_ITEM_EXPLOBOX 9      /* misc_explobox; amount is its health, shot down */
#define ENT_ITEM_KEY1     10     /* the silver key: PL_IT_KEY1 */
#define ENT_ITEM_KEY2     11
#define ENT_ITEM_GL       12
#define ENT_ITEM_ROCKETS  13     /* item_rockets, 5 or 10 */
#define ENT_ITEM_SNG      14
#define ENT_ITEM_RL       15
#define ENT_ITEM_PENT     16
#define ENT_ITEM_SIGIL    17     /* the rune: its target wakes Chthon */
#define ENT_ITEM_ARMOR3   18     /* item_armorInv, 200 at 0.8 */
#define ENT_ITEM_CELLS    19     /* item_cells, 6 or 12 */
#define ENT_ITEM_INVIS    20     /* item_artifact_invisibility; amount is its seconds */
#define ENT_ITEM_KINDS    21

#define ENT_ITEM_HALF    10.0f   /* the flat box's half width */
#define ENT_ITEM_TOP     20.0f   /* and its height */
#define ENT_ITEM_REACH   32.0f   /* Quake's touch: item box against the player's */
#define ENT_ITEM_MEGA    100     /* item_health's healamount when it is the mega one */
#define ENT_BOX_HALF     15.0f   /* b_explob.bsp, 30 by 30 by 62 */
#define ENT_BOX_TOP      62.0f
#define ENT_BOX_DMG      160.0f  /* barrel_explode, less half the distance */
#define ENT_BACKPACK_CRATE -2    /* below this encodes the source monster */

/* flat-box colours the world's palette already has */
#define ENT_COL_WHITE   254
#define ENT_COL_RED     251
#define ENT_COL_YELLOW  111
#define ENT_COL_BROWN   28
#define ENT_COL_BLUE    210      /* the quad */
#define ENT_COL_GREEN   176      /* the suit */

typedef void (far *ItemNotice)( void far *ctx, const char *text );

/*
 * name: pl_items_touch
 * desc: Takes whatever the player's box overlaps, and rots what
 *       megahealth put over 100.
 */
void pl_items_touch( World *world, Player *player, Fight *fight, Renderer *rdr,
                     ItemNotice notice, void far *notice_ctx );

/* MOVETYPE_TOSS for the backpacks monsters leave. */
void pl_items_tick( World *world, Fight *fight, Renderer *rdr, float dt );

/*
 * name: item_selftest
 * desc: 1 when every row of the pickup table sits at its kind and has a
 *       handler.
 */
short item_selftest( void );

/*
 * name: item_box_colours
 * desc: The flat box's side and top colours for a kind, from the same
 *       table the touch reads.
 */
void item_box_colours( short kind, short *side, short *top );

/* items.qc's trigger box, relative to the entity origin. */
void item_bounds( short kind, short crate, BspVec3 *mins, BspVec3 *maxs );

/*
 * name: item_taken
 * desc: How many pickups are gone. The bench line's, so a headless run
 *       can say whether anything was picked up at all.
 */
short item_taken( World *world );

/*
 * name: pl_backpack_add
 * desc: DropBackpack for a normally-killed soldier or ogre. The dead
 *       monster owns the toss velocity, so no second per-map pool is needed.
 */
void pl_backpack_add( World *world, MdlEnt far *owner, short kind,
                      short amount, float now );

#endif
