/*
 * item.c -- the pickups: the table of what each kind is when touched, and
 * pl_items_touch.
 *
 * Pickup touch is a box test against the item's, exactly as Quake's own
 * touch is. Exploding-box solidity belongs to pl_trace.
 */

#include <mem.h>    /* _fmemcpy */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>   /* offsetof */

#include "item.h"
#include "snd.h"
#include "ent.h"
#include "triggers.h"
#include "pl_trace.h"
#include "weapons.h"
#include "qgl.h"

/* items.qc's netname for a key: silver or gold, then the worldtype's word */
static const char *pl_key_name( short wt, short kind )
{
    if ( kind == ENT_ITEM_KEY2 ) {
        if ( wt == 1 ) return "You got the gold runekey";
        if ( wt == 2 ) return "You got the gold keycard";
        return "You got the gold key";
    }
    if ( wt == 1 ) return "You got the silver runekey";
    if ( wt == 2 ) return "You got the silver keycard";
    return "You got the silver key";
}

typedef struct ItemInfo ItemInfo;

/* the pickup's sound, or -1 when the item is refused */
typedef short (near *ItemTake)( Fight *fight, Renderer *rdr, ItemEnt far *it, const ItemInfo far *inf );

struct ItemInfo {
    ItemTake take;
    short    kind;         /* the row's own index: item_selftest checks the order */
    short    ofs;          /* the Fight field an ammo, weapon or powerup adds to */
    short    cap;
    short    bit;          /* PL_IT_* */
    float    atype;        /* armor_touch's type */
    short    snd;
    short    side, top;    /* the flat box's colours */
};

#define FIGHT_SHORT( f, o ) ( (short *) ( (char *) (f) + (o) ) )
#define FIGHT_FLOAT( f, o ) ( (float *) ( (char *) (f) + (o) ) )

/* ofs columns are read as short and float: a retyped field stops the build here */
typedef char item_short_ok[ sizeof(((Fight *) 0)->shells) == sizeof(short) &&
                            sizeof(((Fight *) 0)->nails) == sizeof(short) &&
                            sizeof(((Fight *) 0)->rockets) == sizeof(short) &&
                            sizeof(((Fight *) 0)->cells) == sizeof(short) ? 1 : -1 ];
typedef char item_float_ok[ sizeof(((Fight *) 0)->quad_until) == sizeof(float) &&
                            sizeof(((Fight *) 0)->suit_until) == sizeof(float) &&
                            sizeof(((Fight *) 0)->pent_until) == sizeof(float) ? 1 : -1 ];

static void near item_switch_best( Fight *fight, short old_best )
{
    if ( fight->weapon != old_best ) return;
    fight->weapon = pl_best_weapon( fight );
    if ( fight->weapon == PL_IT_LIGHTNING ) fight->weapon_phase = -1;
}

/* ammo_touch: refused at the cap, capped after */
static short near item_take_ammo( Fight *fight, Renderer *rdr, ItemEnt far *it, const ItemInfo far *inf )
{
    short *v = FIGHT_SHORT( fight, inf->ofs );
    short best;

    (void) rdr;
    if ( *v >= inf->cap ) return -1;
    best = pl_best_weapon( fight );
    *v = (short) ( *v + it->amount );
    if ( *v > inf->cap ) *v = inf->cap;
    item_switch_best( fight, best );
    return inf->snd;
}

/* BackpackTouch always removes the bag, even when its ammo is already full. */
static short near item_take_backpack( Fight *fight, ItemEnt far *it, const ItemInfo far *inf )
{
    short *v = FIGHT_SHORT( fight, inf->ofs );
    short best = pl_best_weapon( fight );
    *v = (short) ( *v + it->amount );
    if ( *v > inf->cap ) *v = inf->cap;
    item_switch_best( fight, best );
    return SND_AMMO;
}

/* ammo_touch's cells: refused at 200, not at the 100 bound_other_ammo caps to */
static short near item_take_cells( Fight *fight, Renderer *rdr, ItemEnt far *it, const ItemInfo far *inf )
{
    short best;

    (void) rdr;
    if ( fight->cells >= PL_CELLS_TAKE ) return -1;
    best = pl_best_weapon( fight );
    fight->cells = (short) ( fight->cells + it->amount );
    if ( fight->cells > inf->cap ) fight->cells = inf->cap;
    item_switch_best( fight, best );
    return inf->snd;
}

/* weapon_touch: the weapon, its ammo, and it is the one in hand */
static short near item_take_weapon( Fight *fight, Renderer *rdr, ItemEnt far *it, const ItemInfo far *inf )
{
    short *v = FIGHT_SHORT( fight, inf->ofs );
    short slot = -1;

    switch ( inf->bit ) {
    case PL_IT_SHOTGUN: slot = 0; break;
    case PL_IT_SSG: slot = 1; break;
    case PL_IT_NAILGUN: slot = 2; break;
    case PL_IT_SNG: slot = 3; break;
    case PL_IT_GL: slot = 4; break;
    case PL_IT_RL: slot = 5; break;
    case PL_IT_LIGHTNING: slot = 6; break;
    }
    if ( slot >= 0 && !( fight->items & inf->bit ) ) {
        if ( slot < 6 ) fight->weapon_at[slot] = rdr->anim_time;
        else fight->lightning_at = rdr->anim_time;
    }
    fight->items |= inf->bit;
    fight->weapon = inf->bit;
    if ( fight->weapon == PL_IT_LIGHTNING ) fight->weapon_phase = -1;
    *v = (short) ( *v + it->amount );
    if ( *v > inf->cap ) *v = inf->cap;
    return inf->snd;
}

/* powerup_touch: the item's own seconds from the pickup */
static short near item_take_powerup( Fight *fight, Renderer *rdr, ItemEnt far *it, const ItemInfo far *inf )
{
    float until = rdr->anim_time + it->amount;
    *FIGHT_FLOAT( fight, inf->ofs ) = until;
    if ( it->kind == ENT_ITEM_QUAD ) fight->quad_warn = -until;
    else if ( it->kind == ENT_ITEM_SUIT ) fight->suit_warn = -until;
    else if ( it->kind == ENT_ITEM_PENT ) fight->pent_warn = -until;
    else if ( it->kind == ENT_ITEM_INVIS ) fight->invis_warn = -until;
    return inf->snd;
}

/* armor_touch: only what beats the armor worn, type * value */
static short near item_take_armor( Fight *fight, Renderer *rdr, ItemEnt far *it, const ItemInfo far *inf )
{
    (void) rdr;
    if ( fight->armor_type * fight->armor < inf->atype * it->amount ) {
        fight->armor_type = inf->atype;
        fight->armor = it->amount;
        return inf->snd;
    }
    return -1;
}

/* key_touch: one of each; the name and the sound are the worldtype's */
static short near item_take_key( Fight *fight, Renderer *rdr, ItemEnt far *it, const ItemInfo far *inf )
{
    if ( fight->items & inf->bit ) return -1;
    fight->items |= inf->bit;
    (void) rdr; (void) it;
    return (short) ( SND_KEY + ( fight->worldtype > 1 ? 1 : fight->worldtype ) );
}

/* sigil_touch: its bits held from here on, and its target fired */
static short near item_take_rune( Fight *fight, Renderer *rdr, ItemEnt far *it, const ItemInfo far *inf )
{
    fight->serverflags |= it->amount;
    ent_say( fight, rdr, (char far *) "You got the rune!" );
    return inf->snd;
}

/* T_Heal: the mega one ignores the 100 cap and stops at 250 */
static short near item_take_health( Fight *fight, Renderer *rdr, ItemEnt far *it, const ItemInfo far *inf )
{
    short cap = (short) ( it->amount == ENT_ITEM_MEGA ? PL_HEALTH_MEGA : PL_HEALTH );

    (void) inf;
    if ( fight->health >= cap ) return -1;
    fight->health = (short) ( fight->health + it->amount );
    if ( fight->health > cap ) fight->health = cap;
    if ( it->amount == ENT_ITEM_MEGA ) it->rot_at = rdr->anim_time + PL_ROT_DELAY;
    if ( it->amount == ENT_ITEM_MEGA ) return SND_HEALTH_MEGA;
    return (short) ( it->amount == 15 ? SND_HEALTH_ROT : SND_HEALTH );
}

/* the exploding box: shot, never taken */
static short near item_take_none( Fight *fight, Renderer *rdr, ItemEnt far *it, const ItemInfo far *inf )
{
    (void) fight; (void) rdr; (void) it; (void) inf;
    return -1;
}

#define B ENT_COL_BROWN
#define Y ENT_COL_YELLOW
#define W ENT_COL_WHITE
static const ItemInfo far item_info[] = {
    { item_take_health, ENT_ITEM_HEALTH,  0,                           0,              0,             0.0f,           0,          ENT_COL_RED,   W },   /* HEALTH */
    { item_take_ammo, ENT_ITEM_SHELLS,    offsetof(Fight, shells),     PL_SHELLS_MAX,  0,             0.0f,           SND_AMMO,   B,             Y },   /* SHELLS */
    { item_take_armor, ENT_ITEM_ARMOR1,   0,                           0,              0,             PL_ARMOR1_TYPE, SND_ARMOR,  B,             Y },   /* ARMOR1 */
    { item_take_armor, ENT_ITEM_ARMOR2,   0,                           0,              0,             PL_ARMOR2_TYPE, SND_ARMOR,  B,             Y },   /* ARMOR2 */
    { item_take_weapon, ENT_ITEM_SSG,  offsetof(Fight, shells),     PL_SHELLS_MAX,  PL_IT_SSG,     0.0f,           SND_WEAPON, B,             Y },   /* SSG */
    { item_take_ammo, ENT_ITEM_NAILS,    offsetof(Fight, nails),      PL_NAILS_CAP,   0,             0.0f,           SND_AMMO,   B,             Y },   /* NAILS */
    { item_take_weapon, ENT_ITEM_NAILGUN,  offsetof(Fight, nails),      PL_NAILS_CAP,   PL_IT_NAILGUN, 0.0f,           SND_WEAPON, B,             Y },   /* NAILGUN */
    { item_take_powerup, ENT_ITEM_QUAD, offsetof(Fight, quad_until), 0,              0,             0.0f,           SND_QUAD,   ENT_COL_BLUE,  W },   /* QUAD */
    { item_take_powerup, ENT_ITEM_SUIT, offsetof(Fight, suit_until), 0,              0,             0.0f,           SND_SUIT,   ENT_COL_GREEN, W },   /* SUIT */
    { item_take_none, ENT_ITEM_EXPLOBOX,    0,                           0,              0,             0.0f,           0,          B,             Y },   /* EXPLOBOX */
    { item_take_key, ENT_ITEM_KEY1,     0,                           0,              PL_IT_KEY1,    0.0f,           0,          B,             Y },   /* KEY1 */
    { item_take_key, ENT_ITEM_KEY2,     0,                           0,              PL_IT_KEY2,    0.0f,           0,          B,             Y },   /* KEY2 */
    { item_take_weapon, ENT_ITEM_GL,  offsetof(Fight, rockets),    PL_ROCKETS_CAP, PL_IT_GL,      0.0f,           SND_WEAPON, B,             Y },   /* GL */
    { item_take_ammo, ENT_ITEM_ROCKETS,    offsetof(Fight, rockets),    PL_ROCKETS_CAP, 0,             0.0f,           SND_AMMO,   B,             Y },   /* ROCKETS */
    { item_take_weapon, ENT_ITEM_SNG,  offsetof(Fight, nails),      PL_NAILS_CAP,   PL_IT_SNG,     0.0f,           SND_WEAPON, B,             Y },   /* SNG */
    { item_take_weapon, ENT_ITEM_RL,  offsetof(Fight, rockets),    PL_ROCKETS_CAP, PL_IT_RL,      0.0f,           SND_WEAPON, B,             Y },   /* RL */
    { item_take_powerup, ENT_ITEM_PENT, offsetof(Fight, pent_until), 0,              0,             0.0f,           SND_PENT,   ENT_COL_RED,   Y },   /* PENT */
    { item_take_rune, ENT_ITEM_SIGIL,    0,                           0,              0,             0.0f,           SND_KEY + 1, B,            Y },   /* SIGIL: misc/runekey */
    { item_take_armor, ENT_ITEM_ARMOR3,   0,                           0,              0,             PL_ARMOR3_TYPE, SND_ARMOR,  B,             Y },   /* ARMOR3 */
    { item_take_cells, ENT_ITEM_CELLS,    offsetof(Fight, cells),      PL_CELLS_CAP,   0,             0.0f,           SND_AMMO,   B,             Y },   /* CELLS */
    { item_take_powerup, ENT_ITEM_INVIS, offsetof(Fight, invis_until), 0,            0,             0.0f,           SND_INV,    B,             Y },   /* INVIS */
    { item_take_weapon, ENT_ITEM_LIGHTNING, offsetof(Fight, cells), PL_CELLS_CAP, PL_IT_LIGHTNING, 0.0f, SND_WEAPON, B, Y } /* LIGHTNING */
};
#undef B
#undef Y
#undef W

/* no designated initializers here: a row short is a zero row, and its take is address 0 */
typedef char item_rows_ok[ sizeof(item_info) / sizeof(item_info[0]) == ENT_ITEM_KINDS ? 1 : -1 ];

short item_selftest( void )
{
    short k;
    BspVec3 mins, maxs;
    Fight fight;

    for ( k = 0; k < ENT_ITEM_KINDS; k++ )
        if ( item_info[k].kind != k || !item_info[k].take ) return 0;
    item_bounds( ENT_ITEM_QUAD, -1, &mins, &maxs );
    if ( mins.x != -16.0f || mins.z != -24.0f || maxs.z != 32.0f ) return 0;
    item_bounds( ENT_ITEM_SHELLS, 0, &mins, &maxs );
    if ( mins.x != 0.0f || maxs.x != 32.0f || maxs.z != 56.0f ) return 0;
    memset( &fight, 0, sizeof(fight) );
    fight.items = PL_IT_SHOTGUN | PL_IT_SNG | PL_IT_AXE;
    fight.shells = 25;
    if ( pl_best_weapon( &fight ) != PL_IT_SHOTGUN ) return 0;
    fight.nails = 2;
    if ( pl_best_weapon( &fight ) != PL_IT_SNG ) return 0;
    fight.items |= PL_IT_LIGHTNING;
    fight.cells = 1;
    if ( pl_best_weapon( &fight ) != PL_IT_LIGHTNING ) return 0;
    return 1;
}

void item_box_colours( short kind, short *side, short *top )
{
    const ItemInfo far *inf = &item_info[ kind >= 0 && kind < ENT_ITEM_KINDS ? kind : ENT_ITEM_EXPLOBOX ];

    *side = inf->side;
    *top  = inf->top;
}

void item_bounds( short kind, short crate, BspVec3 *mins, BspVec3 *maxs )
{
    mins->x = mins->y = -16.0f;
    maxs->x = maxs->y = 16.0f;
    mins->z = 0.0f;
    maxs->z = 56.0f;

    /* The b_*.bsp item triggers start at their map origin. */
    if ( crate >= 0 ) {
        mins->x = mins->y = mins->z = 0.0f;
        maxs->x = maxs->y = ENT_BOX_CLIP_XY;
        maxs->z = kind == ENT_ITEM_EXPLOBOX ? ENT_BOX_CLIP_Z : 56.0f;
        return;
    }
    if ( crate <= ENT_BACKPACK_CRATE ) return;

    switch ( kind ) {
    case ENT_ITEM_QUAD:
    case ENT_ITEM_SUIT:
    case ENT_ITEM_KEY1:
    case ENT_ITEM_KEY2:
    case ENT_ITEM_PENT:
    case ENT_ITEM_SIGIL:
    case ENT_ITEM_INVIS:
        mins->z = -24.0f;
        maxs->z = 32.0f;
        break;
    }
}

/* CheckPowerups: once, three seconds before a powerup runs out */
static void near item_notice_add( ItemNotice notice, void far *ctx, const char *text )
{
    if ( notice ) notice( ctx, text );
}

static void near item_notice_take( Fight *fight, ItemEnt far *it,
                                   ItemNotice notice, void far *ctx )
{
    char text[64];

    if ( it->crate <= ENT_BACKPACK_CRATE ) {
        sprintf( text, "You get %d %s", (int) it->amount,
                 it->kind == ENT_ITEM_SHELLS ? "shells" : "rockets" );
        item_notice_add( notice, ctx, text );
        return;
    }
    switch ( it->kind ) {
    case ENT_ITEM_HEALTH:
        sprintf( text, "You receive %d health", (int) it->amount );
        item_notice_add( notice, ctx, text ); return;
    case ENT_ITEM_SHELLS: item_notice_add( notice, ctx, "You got the shells" ); return;
    case ENT_ITEM_NAILS: item_notice_add( notice, ctx, "You got the nails" ); return;
    case ENT_ITEM_ROCKETS: item_notice_add( notice, ctx, "You got the rockets" ); return;
    case ENT_ITEM_CELLS: item_notice_add( notice, ctx, "You got the cells" ); return;
    case ENT_ITEM_ARMOR1:
    case ENT_ITEM_ARMOR2:
    case ENT_ITEM_ARMOR3: item_notice_add( notice, ctx, "You got armor" ); return;
    case ENT_ITEM_SSG: item_notice_add( notice, ctx, "You got the Double-barrelled Shotgun" ); return;
    case ENT_ITEM_NAILGUN: item_notice_add( notice, ctx, "You got the nailgun" ); return;
    case ENT_ITEM_GL: item_notice_add( notice, ctx, "You got the Grenade Launcher" ); return;
    case ENT_ITEM_SNG: item_notice_add( notice, ctx, "You got the Super Nailgun" ); return;
    case ENT_ITEM_RL: item_notice_add( notice, ctx, "You got the Rocket Launcher" ); return;
    case ENT_ITEM_LIGHTNING: item_notice_add( notice, ctx, "You got the Thunderbolt" ); return;
    case ENT_ITEM_QUAD: item_notice_add( notice, ctx, "You got the Quad Damage" ); return;
    case ENT_ITEM_SUIT: item_notice_add( notice, ctx, "You got the Biosuit" ); return;
    case ENT_ITEM_PENT: item_notice_add( notice, ctx, "You got the Pentagram of Protection" ); return;
    case ENT_ITEM_INVIS: item_notice_add( notice, ctx, "You got the Ring of Shadows" ); return;
    case ENT_ITEM_KEY1:
    case ENT_ITEM_KEY2:
        item_notice_add( notice, ctx, pl_key_name( fight->worldtype, it->kind ) ); return;
    }
}

static void pl_powerup_warn( Player *player, Sound *snd, Fight *fight,
                             Renderer *rdr, float until, float *warned, short id,
                             const char *text, ItemNotice notice, void far *ctx )
{
    if ( until <= rdr->anim_time || rdr->anim_time < until - 3.0f ) return;
    if ( *warned < 0.0f ) {
        snd_self( snd, player, CHAN_AUTO, id );
        item_notice_add( notice, ctx, text );
        *warned = rdr->anim_time;
    }
    if ( rdr->anim_time >= *warned ) {
        fight->bonus_pct = PL_BONUS_SHIFT;
        *warned += 1.0f;
    }
}

void pl_items_touch( World *world, Player *player, Fight *fight, Renderer *rdr,
                     ItemNotice notice, void far *notice_ctx )
{
    short i, snd;
    BspVec3 mins, maxs;

    if ( fight->health <= 0 ) return;

    for ( i = 0; i < world->item_count; i++ ) {
        ItemEnt far *it = &world->item[i];

        if ( it->gone ) continue;

        if ( it->kind < 0 || it->kind >= ENT_ITEM_KINDS ) continue;
        item_bounds( it->kind, it->crate, &mins, &maxs );
        /* SV_LinkEdict expands FL_ITEM by 15 horizontally and the moving
           player's linked box by one on every axis. Touching faces count. */
        if ( player->pos.x + 17.0f < it->pos.x + mins.x - 15.0f ||
             player->pos.x - 17.0f > it->pos.x + maxs.x + 15.0f ||
             player->pos.y + 17.0f < it->pos.y + mins.y - 15.0f ||
             player->pos.y - 17.0f > it->pos.y + maxs.y + 15.0f ||
             player->pos.z + 33.0f < it->pos.z + mins.z ||
             player->pos.z - PL_FEET - 1.0f > it->pos.z + maxs.z ) continue;
        if ( it->crate <= ENT_BACKPACK_CRATE )
            snd = item_take_backpack( fight, it, &item_info[it->kind] );
        else
            snd = item_info[it->kind].take( fight, rdr, it, &item_info[it->kind] );
        if ( snd < 0 ) continue;

        item_notice_take( fight, it, notice, notice_ctx );
        it->gone = -1;
        fight->bonus_pct = PL_BONUS_SHIFT;
        snd_self( &world->snd, player,
                  (short) ( it->kind == ENT_ITEM_QUAD || it->kind == ENT_ITEM_SUIT ||
                            it->kind == ENT_ITEM_PENT || it->kind == ENT_ITEM_INVIS
                            ? CHAN_VOICE : CHAN_ITEM ), snd );
        /* SUB_UseTargets: every touch fires the item's target */
        ent_use_targets( world, player, fight, rdr, it->target );
    }

    /* PlayerPostThink runs CheckPowerups after touches. A replacement
       powerup therefore cannot report the old one's stale warning. */
    pl_powerup_warn( player, &world->snd, fight, rdr, fight->quad_until, &fight->quad_warn,
                     SND_QUAD_END, "Quad Damage is wearing off", notice, notice_ctx );
    pl_powerup_warn( player, &world->snd, fight, rdr, fight->suit_until, &fight->suit_warn,
                     SND_SUIT_END, "Air supply in Biosuit expiring", notice, notice_ctx );
    pl_powerup_warn( player, &world->snd, fight, rdr, fight->pent_until, &fight->pent_warn,
                     SND_PENT_END, "Protection is almost burned out", notice, notice_ctx );
    pl_powerup_warn( player, &world->snd, fight, rdr, fight->invis_until, &fight->invis_warn,
                     SND_INV_END, "Ring of Shadows magic is fading", notice, notice_ctx );

    /* CheckPowerups, after the touches as PlayerPostThink is: the ring
       whispers from the tick it is taken, 1..4 seconds apart. PF_sound
       truncates QC's 0.5 * 255 to 127. */
    if ( rdr->anim_time < fight->invis_until && rdr->anim_time >= fight->invis_snd_at ) {
        snd_start_volume( &world->snd, player, SND_ENT_PLAYER, CHAN_AUTO,
                          SND_INV_IDLE, &player->pos, ATTN_IDLE, 127 );
        fight->invis_snd_at = rdr->anim_time + 1.0f + rng_float( &world->rng ) * 3.0f;
    }
}

void pl_items_tick( World *world, Fight *fight, Renderer *rdr, float dt )
{
    short i;

    /* Every collected megahealth retains its own item_megahealth_rot
       thinker. Two collected close together therefore decay twice a second. */
    for ( i = 0; i < world->item_count; i++ ) {
        ItemEnt far *it = &world->item[i];
        if ( !it->gone || it->kind != ENT_ITEM_HEALTH ||
             it->amount != ENT_ITEM_MEGA || it->rot_at <= 0.0f ||
             rdr->anim_time < it->rot_at ) continue;
        if ( fight->health > PL_HEALTH ) {
            fight->health--;
            it->rot_at += 1.0f;
        } else
            it->rot_at = 0.0f;
    }

    i = (short) ( world->item_max - world->mon_count );

    /* Map pickups never move. The tail alone is DropBackpack's, so a map
       with no dead ammo-carrying monster pays one compare per tick. */
    for ( ; i < world->item_count; i++ ) {
        ItemEnt far *it = &world->item[i];
        short source;
        MdlEnt far *mon;
        BspVec3 org, fin;
        TraceResult tr;
        float backoff;

        if ( it->gone || it->crate > ENT_BACKPACK_CRATE ) continue;
        source = (short) ( ENT_BACKPACK_CRATE - it->crate );
        if ( source < 0 || source >= world->mon_count ) { it->gone = -1; continue; }
        mon = &world->mon[source];
        if ( rdr->anim_time >= mon->idle_at ) { it->gone = -1; continue; }

        mon->vel.z -= fight->gravity * dt;
        org = it->pos;
        org.z += PL_FEET;
        fin.x = org.x + mon->vel.x * dt;
        fin.y = org.y + mon->vel.y * dt;
        fin.z = org.z + mon->vel.z * dt;
        /* BackpackTouch's -16..16, 0..56 box is hull 1 shifted up by
           PL_FEET: the player's -16..16, -24..32 becomes it exactly. */
        pl_trace( world, &org, &fin, &tr );
        it->pos = tr.end_pos;
        it->pos.z -= PL_FEET;
        if ( tr.frac >= 1.0f ) continue;

        backoff = mon->vel.x * tr.norm.x + mon->vel.y * tr.norm.y + mon->vel.z * tr.norm.z;
        mon->vel.x -= tr.norm.x * backoff;
        mon->vel.y -= tr.norm.y * backoff;
        mon->vel.z -= tr.norm.z * backoff;
        if ( tr.norm.z > 0.7f && mon->vel.z < 60.0f )
            mon->vel.x = mon->vel.y = mon->vel.z = 0.0f;
    }
}

short item_taken( World *world )
{
    short i, n = 0;
    for ( i = 0; i < world->item_count; i++ ) if ( world->item[i].gone ) n++;
    return n;
}

void pl_backpack_add( World *world, MdlEnt far *owner, short kind,
                      short amount, float now )
{
    ItemEnt far *it;
    short source, i;

    if ( world->item_count >= world->item_max ) return;
    for ( source = 0; source < world->mon_count; source++ )
        if ( &world->mon[source] == owner ) break;
    if ( source >= world->mon_count ) return;
    for ( i = 0; i < world->item_count; i++ )
        if ( world->item[i].crate == ENT_BACKPACK_CRATE - source ) return;

    it = &world->item[ world->item_count ];
    it->kind = kind;
    it->amount = amount;
    it->target = 0;
    it->crate = (short) ( ENT_BACKPACK_CRATE - source );
    it->order = owner->order;
    it->pos = owner->pos;
    it->pos.z -= 24.0f;
    it->gone = 0;
    it->rot_at = 0.0f;
    owner->vel.x = rng_float( &world->rng ) * 200.0f - 100.0f;
    owner->vel.y = rng_float( &world->rng ) * 200.0f - 100.0f;
    owner->vel.z = 300.0f;
    owner->idle_at = now + 120.0f;
    world->item_mdl_slot[world->item_count] = (signed char) world->backpack_mdl;
    world->item_skin[world->item_count] = 0;
    world->item_count++;
}
