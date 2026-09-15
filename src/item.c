/*
 * item.c -- the pickups: the table of what each kind is when touched, and
 * pl_items_touch.
 *
 * Nothing here traces: the touch is a box test against the item's,
 * exactly as Quake's own touch is.
 *
 * Not ported with the rest: pl_boxes_sync -- an exploding box is solid in the BASIC branch
 * because pl_trace keeps a table of solid boxes, which cport's
 * pl_trace does not have. So an explobox here is drawn and walked
 * through rather than walked around, and nothing can shoot it.
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
#include "qgl.h"

/* items.qc's netname for a key: silver or gold, then the worldtype's word */
static void pl_key_name( char *out, short wt, short kind )
{
    strcpy( out, "You got the " );
    strcat( out, kind == ENT_ITEM_KEY2 ? "gold " : "silver " );
    strcat( out, wt == 1 ? "runekey" : ( wt == 2 ? "keycard" : "key" ) );
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

/* ammo_touch: refused at the cap, capped after */
static short near item_take_ammo( Fight *fight, Renderer *rdr, ItemEnt far *it, const ItemInfo far *inf )
{
    short *v = FIGHT_SHORT( fight, inf->ofs );

    (void) rdr;
    if ( *v >= inf->cap ) return -1;
    *v = (short) ( *v + it->amount );
    if ( *v > inf->cap ) *v = inf->cap;
    return inf->snd;
}

/* ammo_touch's cells: refused at 200, not at the 100 bound_other_ammo caps to */
static short near item_take_cells( Fight *fight, Renderer *rdr, ItemEnt far *it, const ItemInfo far *inf )
{
    (void) rdr;
    if ( fight->cells >= PL_CELLS_TAKE ) return -1;
    fight->cells = (short) ( fight->cells + it->amount );
    if ( fight->cells > inf->cap ) fight->cells = inf->cap;
    return inf->snd;
}

/* weapon_touch: the weapon, its ammo, and it is the one in hand */
static short near item_take_weapon( Fight *fight, Renderer *rdr, ItemEnt far *it, const ItemInfo far *inf )
{
    short *v = FIGHT_SHORT( fight, inf->ofs );

    (void) rdr;
    fight->items |= inf->bit;
    fight->weapon = inf->bit;
    *v = (short) ( *v + it->amount );
    if ( *v > inf->cap ) *v = inf->cap;
    return inf->snd;
}

/* powerup_touch: the item's own seconds from the pickup */
static short near item_take_powerup( Fight *fight, Renderer *rdr, ItemEnt far *it, const ItemInfo far *inf )
{
    *FIGHT_FLOAT( fight, inf->ofs ) = rdr->anim_time + it->amount;
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
    char line[ENT_MSG_LEN + 1];

    if ( fight->items & inf->bit ) return -1;
    fight->items |= inf->bit;
    memset( line, 0, sizeof(line) );
    pl_key_name( line, fight->worldtype, it->kind );
    ent_say( fight, rdr, (char far *) line );
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
    if ( fight->health > PL_HEALTH ) fight->rot_at = rdr->anim_time + PL_ROT_DELAY;
    if ( it->amount == ENT_ITEM_MEGA ) return SND_HEALTH_MEGA;
    return (short) ( it->amount < 15 ? SND_HEALTH_ROT : SND_HEALTH );
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
    { item_take_powerup, ENT_ITEM_INVIS, offsetof(Fight, invis_until), 0,            0,             0.0f,           SND_INV,    B,             Y }    /* INVIS */
};
#undef B
#undef Y
#undef W

/* no designated initializers here: a row short is a zero row, and its take is address 0 */
typedef char item_rows_ok[ sizeof(item_info) / sizeof(item_info[0]) == ENT_ITEM_KINDS ? 1 : -1 ];

short item_selftest( void )
{
    short k;

    for ( k = 0; k < ENT_ITEM_KINDS; k++ )
        if ( item_info[k].kind != k || !item_info[k].take ) return 0;
    return 1;
}

void item_box_colours( short kind, short *side, short *top )
{
    const ItemInfo far *inf = &item_info[ kind >= 0 && kind < ENT_ITEM_KINDS ? kind : ENT_ITEM_EXPLOBOX ];

    *side = inf->side;
    *top  = inf->top;
}

/* CheckPowerups: once, three seconds before a powerup runs out */
static void pl_powerup_warn( Player *player, Sound *snd, Renderer *rdr, float until, float *warned, short id )
{
    if ( until <= rdr->anim_time || rdr->anim_time < until - 3.0f || *warned == until ) return;
    *warned = until;
    snd_self( snd, player, CHAN_AUTO, id );
}

void pl_items_touch( World *world, Player *player, Fight *fight, Renderer *rdr )
{
    short i, snd;
    float dz;

    /* item_megahealth_rot: over 100, a point a second after five */
    if ( fight->health > PL_HEALTH && rdr->anim_time >= fight->rot_at ) {
        fight->health--;
        fight->rot_at = rdr->anim_time + 1.0f;
    }

    pl_powerup_warn( player, &world->snd, rdr, fight->quad_until, &fight->quad_warn, SND_QUAD_END );
    pl_powerup_warn( player, &world->snd, rdr, fight->suit_until, &fight->suit_warn, SND_SUIT_END );
    pl_powerup_warn( player, &world->snd, rdr, fight->pent_until, &fight->pent_warn, SND_PENT_END );
    pl_powerup_warn( player, &world->snd, rdr, fight->invis_until, &fight->invis_warn, SND_INV_END );

    for ( i = 0; i < world->item_count; i++ ) {
        ItemEnt far *it = &world->item[i];

        if ( it->gone ) continue;

        dz = player->pos.z - it->pos.z;
        if ( (float) fabs( player->pos.x - it->pos.x ) >= ENT_ITEM_REACH ) continue;
        if ( (float) fabs( player->pos.y - it->pos.y ) >= ENT_ITEM_REACH ) continue;
        if ( dz <= -ENT_ITEM_TOP - PL_FEET || dz >= ENT_ITEM_TOP + PL_FEET ) continue;

        if ( it->kind < 0 || it->kind >= ENT_ITEM_KINDS ) continue;
        snd = item_info[it->kind].take( fight, rdr, it, &item_info[it->kind] );
        if ( snd < 0 ) continue;

        it->gone = -1;
        fight->bonus_pct = PL_BONUS_SHIFT;
        snd_self( &world->snd, player, CHAN_ITEM, snd );
        /* SUB_UseTargets: every touch fires the item's target */
        ent_use_targets( world, player, fight, rdr, it->target );
    }

    /* CheckPowerups, after the touches as PlayerPostThink is: the ring
       whispers from the tick it is taken, 1..4 seconds apart. id's is at
       half volume, which the mixer has no argument for */
    if ( rdr->anim_time < fight->invis_until && rdr->anim_time >= fight->invis_snd_at ) {
        snd_start( &world->snd, player, SND_ENT_PLAYER, CHAN_AUTO, SND_INV_IDLE, &player->pos, ATTN_IDLE );
        fight->invis_snd_at = rdr->anim_time + 1.0f + rng_float( &world->rng ) * 3.0f;
    }
}

short item_taken( World *world )
{
    short i, n = 0;
    for ( i = 0; i < world->item_count; i++ ) if ( world->item[i].gone ) n++;
    return n;
}

void pl_item_add( World *world, short kind, short amount, BspVec3 far *org )
{
    ItemEnt far *it;

    if ( world->item_count >= world->item_max ) return;
    it = &world->item[ world->item_count ];
    it->kind = kind;
    it->amount = amount;
    it->target = 0;
    it->crate = -1;
    it->pos = *org;
    it->gone = 0;
    world->item_count++;
}
