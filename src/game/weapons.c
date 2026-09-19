/*
 * weapons.c -- weapons.qc's W_Fire*, everything in flight, and the
 * monster's side of taking a hit. The shotguns are hitscan and land
 * their damage per monster once every pellet is in (ApplyMultiDamage);
 * the other four leave a Spike that pl_spikes_tick walks a step at a
 * time.
 *
 * A projectile is traced through hull 1, the player's, so a wall stops
 * it 16 units early -- which is why everything that can be shot is
 * tested to bt + PL_HALF rather than to where the trace stopped. A
 * hostile spike is the exception: it leaves from a point inside hull
 * 1's grown solid, so it walks as a point through hull 0.
 */

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "weapons.h"
#include "part.h"
#include "d_sprite.h"
#include "snd.h"
#include "mdl_ai.h"
#include "pl_trace.h"
#include "item.h"
#include "doors.h"
#include "triggers.h"
#include "dl.h"
#include "combat.h"
#include "d_beam.h"

#define DEG2RAD 0.017453293f

short pl_best_weapon( const Fight *fight )
{
    if ( fight->cells >= 1 && ( fight->items & PL_IT_LIGHTNING ) ) return PL_IT_LIGHTNING;
    if ( fight->nails >= 2 && ( fight->items & PL_IT_SNG ) ) return PL_IT_SNG;
    if ( fight->shells >= 2 && ( fight->items & PL_IT_SSG ) ) return PL_IT_SSG;
    if ( fight->nails >= 1 && ( fight->items & PL_IT_NAILGUN ) ) return PL_IT_NAILGUN;
    if ( fight->shells >= 1 && ( fight->items & PL_IT_SHOTGUN ) ) return PL_IT_SHOTGUN;
    return PL_IT_AXE;
}

/* One axis of the ray-box test: narrows tn..tf to the slab. */
static short mdl_slab( float lo, float hi, float o, float d,
                        float *tn, float *tf )
{
    float t0, t1, sw;

    if ( fabs( d ) < 0.0001 ) return ( o >= lo && o <= hi ) ? -1 : 0;
    t0 = ( lo - o ) / d;
    t1 = ( hi - o ) / d;
    if ( t0 > t1 ) { sw = t0; t0 = t1; t1 = sw; }
    if ( t0 > *tn ) *tn = t0;
    if ( t1 < *tf ) *tf = t1;
    return (short) ( *tn <= *tf );
}

float pl_ray_box( BspVec3 *mins, BspVec3 *maxs, BspVec3 *org,
                   BspVec3 *dir, float maxt )
{
    float tn = 0.0f, tf = maxt;

    if ( !mdl_slab( mins->x, maxs->x, org->x, dir->x, &tn, &tf ) ) return -1.0f;
    if ( !mdl_slab( mins->y, maxs->y, org->y, dir->y, &tn, &tf ) ) return -1.0f;
    if ( !mdl_slab( mins->z, maxs->z, org->z, dir->z, &tn, &tf ) ) return -1.0f;
    return tn;
}

float mdl_ray_player( BspVec3 *pl, BspVec3 *org, BspVec3 *dir, float maxt )
{
    BspVec3 mins, maxs;

    mins.x = pl->x - PL_HALF; maxs.x = pl->x + PL_HALF;
    mins.y = pl->y - PL_HALF; maxs.y = pl->y + PL_HALF;
    mins.z = pl->z + PL_ZLO;  maxs.z = pl->z + PL_ZHI;
    return pl_ray_box( &mins, &maxs, org, dir, maxt );
}

/* a monster's box, the one the model is culled with */
static float mdl_ray_monster( MdlEnt far *ent, BspVec3 *org,
                              BspVec3 *dir, float maxt )
{
    BspVec3 mins, maxs;
    float half = MDL_MON_HALF( ent->kind );

    mins.x = ent->pos.x - half; maxs.x = ent->pos.x + half;
    mins.y = ent->pos.y - half; maxs.y = ent->pos.y + half;
    mins.z = ent->pos.z + MDL_ZLO;
    maxs.z = ent->pos.z + MDL_MON_ZHI( ent->kind );
    return pl_ray_box( &mins, &maxs, org, dir, maxt );
}

static short pl_trace_boxes( World *world, BspVec3 *start, BspVec3 *fin,
                             TraceResult *tr )
{
    BspVec3 point = { 0.0f, 0.0f, 0.0f };
    short hit = -1;

    pl_trace_items( world, start, fin, &point, &point, -1, tr, &hit );
    return hit;
}

static void pl_impact_at( BspVec3 *out, BspVec3 *org, BspVec3 *dir, float t )
{
    out->x = org->x + dir->x * ( t - 4.0f );
    out->y = org->y + dir->y * ( t - 4.0f );
    out->z = org->z + dir->z * ( t - 4.0f );
}

void pl_spread_dir( Rng *rng, BspVec3 *dir, float sx, float sy, BspVec3 *out )
{
    float rx = dir->y, ry = -dir->x, rl;
    float ux, uy, uz, a, b;

    rl = (float) sqrt( rx*rx + ry*ry );
    if ( rl < 0.001f ) { rx = 1.0f; ry = 0.0f; rl = 1.0f; }
    rx /= rl; ry /= rl;
    ux =  ry * dir->z;
    uy = -rx * dir->z;
    uz =  rx * dir->y - ry * dir->x;
    a = ( 2.0f * rng_float( rng ) - 1.0f ) * sx;
    b = ( 2.0f * rng_float( rng ) - 1.0f ) * sy;
    out->x = dir->x + a * rx + b * ux;
    out->y = dir->y + a * ry + b * uy;
    out->z = dir->z + b * uz;
}

/* Literal v1.01 LightningDamage semantics: normalize returns a vector,
   so the unassigned call leaves f long; f_y then reads the overwritten f_x. */
static void near pl_lightning_side( BspVec3 *p1, BspVec3 *p2, BspVec3 *f )
{
    f->x = p2->x - p1->x;
    f->y = p2->y - p1->y;
    f->z = p2->z - p1->z;
    f->x = -f->y;
    f->y = f->x;
    f->z = 0.0f;
    f->x *= 16.0f;
    f->y *= 16.0f;
}

short pl_nail_free( Fight *fight )
{
    short n;

    for ( n = 0; n < PL_NAILS_MAX; n++ )
        if ( !fight->nail[n].alive ) {
            fight->nail[n].hostile = 0;
            fight->nail[n].grenade = 0;
            fight->nail[n].gib     = 0;
            fight->nail[n].rocket  = 0;
            fight->nail[n].toss    = 0;
            fight->nail[n].wiz     = 0;
            fight->nail[n].water   = 0;
            fight->nail[n].owner   = COMBAT_BY_PLAYER;
            fight->nail[n].ignore  = COMBAT_NO_IGNORE;
            return n;
        }
    return -1;
}

short mdl_boss_lava( Player *player, Fight *fight, Renderer *rdr,
                      BspVec3 *boss, float yaw, short side )
{
    Spike *s;
    BspVec3 aim;
    float a, cy, sy, dx, dy, dz, len, lead;
    short n = pl_nail_free( fight );

    if ( n < 0 ) return 0;
    s = &fight->nail[n];
    a = yaw * 3.14159265f / 180.0f;
    cy = (float) cos( a ); sy = (float) sin( a );
    s->pos.x = boss->x + cy * 100.0f + sy * side * 100.0f;
    s->pos.y = boss->y + sy * 100.0f - cy * side * 100.0f;
    s->pos.z = boss->z + 200.0f;
    aim = player->pos;
    dx = aim.x - s->pos.x; dy = aim.y - s->pos.y; dz = aim.z - s->pos.z;
    len = (float) sqrt( dx*dx + dy*dy + dz*dz );
    if ( fight->skill > 1 && len > 0.001f ) {
        lead = len / 300.0f;
        aim.x += player->vel.x * lead;
        aim.y += player->vel.y * lead;
        dx = aim.x - s->pos.x; dy = aim.y - s->pos.y; dz = aim.z - s->pos.z;
        len = (float) sqrt( dx*dx + dy*dy + dz*dz );
    }
    if ( len < 0.001f ) len = 1.0f;
    s->vel.x = dx / len * 300.0f;
    s->vel.y = dy / len * 300.0f;
    s->vel.z = dz / len * 300.0f;
    s->die_at = rdr->anim_time + 5.0f;
    s->hostile = -1;
    s->toss = 2;
    s->dmg = PL_FIREBALL_DMG;
    s->alive = -1;
    return -1;
}

/* barrel_explode: T_RadiusDamage's 160 from the box's centre */
/* GrenadeExplode: T_RadiusDamage from where it lies */
static void pl_grenade_explode( World *world, Player *player, Fight *fight,
                                 Renderer *rdr, Spike *s )
{
    s->alive = 0;
    snd_play( &world->snd, player, SND_BOOM, &s->pos );
    dl_explosion( rdr, &s->pos );
    part_explosion( rdr, &s->pos );
    d_sprite_explosion( rdr, &s->pos );
    combat_radius( world, player, fight, rdr, s->pos, (float) s->dmg,
                   s->owner, s->ignore );
}

#define PL_CONTACT_NONE  0
#define PL_CONTACT_BOX   1
#define PL_CONTACT_BRUSH 2

static short near pl_grenade_contact( short box, float frac )
{
    if ( box >= 0 ) return PL_CONTACT_BOX;
    if ( frac < 1.0f ) return PL_CONTACT_BRUSH;
    return PL_CONTACT_NONE;
}

static void near pl_grenade_segment( Spike *s, float gravity, float dt,
                                      BspVec3 *fin, BspVec3 *dir, float *reach )
{
    s->vel.z -= gravity * dt;
    fin->x = s->pos.x + s->vel.x * dt;
    fin->y = s->pos.y + s->vel.y * dt;
    fin->z = s->pos.z + s->vel.z * dt;
    dir->x = fin->x - s->pos.x;
    dir->y = fin->y - s->pos.y;
    dir->z = fin->z - s->pos.z;
    *reach = (float) sqrt( dir->x*dir->x + dir->y*dir->y + dir->z*dir->z );
    if ( *reach > 0.0001f ) {
        dir->x /= *reach; dir->y /= *reach; dir->z /= *reach;
    } else
        dir->x = dir->y = dir->z = 0.0f;
}

static short near pl_grenade_monster( MdlEnt far *mon, short count, BspVec3 *pos,
                                       BspVec3 *dir, float *best_t )
{
    short i, best = -1;
    float t;

    for ( i = 0; i < count; i++ ) {
        if ( !MDL_IS_SOLID( &mon[i] ) ) continue;
        t = mdl_ray_monster( &mon[i], pos, dir, *best_t );
        if ( t >= 0.0f && t < *best_t ) { *best_t = t; best = i; }
    }
    return best;
}

static void near pl_lightning_release_values( short weapon, short *phase,
                                               short *sustained, float fire_at,
                                               float *next_fire )
{
    if ( weapon != PL_IT_LIGHTNING || *phase < 0 ) return;
    if ( *sustained ) *next_fire = fire_at + PL_LG_RESTART;
    *phase = -1;
    *sustained = 0;
}

short pl_projectile_selftest( void )
{
    MdlEnt mon[2];
    Spike s;
    BspVec3 fin, dir, p1, p2, side;
    float reach, hit, next;
    short phase, sustained;

    memset( mon, 0, sizeof(mon) );
    memset( &s, 0, sizeof(s) );
    s.vel.x = 100.0f;
    pl_grenade_segment( &s, 800.0f, 0.1f, &fin, &dir, &reach );
    if ( s.vel.z != -80.0f || fin.x != 10.0f || fin.z != -8.0f ||
         reach < 12.80f || reach > 12.81f ) return 0;

    mon[0].pos.x = 100.0f;
    mon[1].pos.x = 50.0f;
    dir.x = 1.0f; dir.y = dir.z = 0.0f;
    hit = 200.0f;
    if ( pl_grenade_monster( mon, 2, &s.pos, &dir, &hit ) != 1 || hit != 34.0f ) return 0;
    if ( pl_grenade_contact( 0, 1.0f ) != PL_CONTACT_BOX ||
         pl_grenade_contact( -1, 1.0f ) != PL_CONTACT_NONE ||
         pl_grenade_contact( -1, 0.5f ) != PL_CONTACT_BRUSH ) return 0;
    p1.x = 1.0f; p1.y = 2.0f; p1.z = 3.0f;
    p2.x = 11.0f; p2.y = 22.0f; p2.z = 33.0f;
    pl_lightning_side( &p1, &p2, &side );
    if ( side.x != -320.0f || side.y != -320.0f || side.z != 0.0f ) return 0;
    memset( &mon[0], 0, sizeof(mon[0]) );
    mon[0].kind = MDL_KIND_DOG;
    mon[0].pos.x = 100.0f;
    p1.x = 0.0f; p1.y = 24.0f; p1.z = 0.0f;
    dir.x = 1.0f; dir.y = dir.z = 0.0f;
    if ( mdl_ray_monster( &mon[0], &p1, &dir, 200.0f ) != 68.0f ) return 0;
    phase = 0; sustained = 0; next = 1.1f;
    pl_lightning_release_values( PL_IT_LIGHTNING, &phase, &sustained, 1.0f, &next );
    if ( phase != -1 || fabs( next - 1.1f ) > 0.001f ) return 0;
    phase = 1; sustained = -1;
    pl_lightning_release_values( PL_IT_LIGHTNING, &phase, &sustained, 1.0f, &next );
    if ( phase != -1 || sustained || fabs( next - 1.2f ) > 0.001f ) return 0;
    pl_lightning_release_values( PL_IT_LIGHTNING, &phase, &sustained, 1.0f, &next );
    if ( fabs( next - 1.2f ) > 0.001f ) return 0;
    return 1;
}

void pl_lightning_release( Fight *fight )
{
    pl_lightning_release_values( fight->weapon, &fight->weapon_phase,
                                 &fight->lg_sustained, fight->fire_at,
                                 &fight->next_fire );
}

/* Shootable triggers and secret doors short of where the shot stopped. */
static void pl_shot_touch( World *world, Player *player, Fight *fight,
                            Renderer *rdr, BspVec3 *org, BspVec3 *dir,
                            float bt, short *stopped )
{
    BspVec3 mins, maxs;
    short i;

    /* copies: pl_ray_box takes near pointers, and a far array element's
       address cast to one reads DGROUP */
    for ( i = 0; i < world->trig_count; i++ ) {
        if ( ( world->trig[i].kind != ENT_TRIG_SHOOT &&
               world->trig[i].kind != ENT_TRIG_SHOOT_BUTTON ) ||
             world->trig[i].state != ENT_TRIG_READY ) continue;
        mins = world->trig[i].mins;
        maxs = world->trig[i].maxs;
        if ( pl_ray_box( &mins, &maxs, org, dir, bt + PL_HALF ) < 0.0f ) continue;
        ent_trig_shot( world, player, fight, rdr, i );
        if ( stopped ) *stopped = -1;
    }

    for ( i = 0; i < world->door_count; i++ ) {
        if ( !world->door[i].shoot || world->door[i].state != ENT_DOOR_SHUT ) continue;
        mins = world->door[i].mins;
        maxs = world->door[i].maxs;
        if ( pl_ray_box( &mins, &maxs, org, dir, bt + PL_HALF ) < 0.0f ) continue;
        ent_door_fire( world, player, fight, rdr, world->door[i].link );
        if ( stopped ) *stopped = -1;
    }
}

#define PL_LHIT_NONE 0
#define PL_LHIT_MON  1
#define PL_LHIT_BOX  2
#define PL_LHIT_TRIG 3
#define PL_LHIT_DOOR 4
typedef struct {
    short kind, index, order;
    float t;
} LightningTarget;

static short near pl_lightning_target( World *world, BspVec3 *start,
                                        BspVec3 *end, LightningTarget *hit )
{
    BspVec3 dir, mins, maxs;
    TraceResult tr;
    float reach, t;
    short i, box, brush_model;

    dir.x = end->x - start->x;
    dir.y = end->y - start->y;
    dir.z = end->z - start->z;
    reach = (float) sqrt( dir.x*dir.x + dir.y*dir.y + dir.z*dir.z );
    if ( reach < 0.001f ) return 0;
    dir.x /= reach; dir.y /= reach; dir.z /= reach;

    pl_trace_point( world, start, end, &tr );
    brush_model = tr.hit_model;
    box = pl_trace_boxes( world, start, end, &tr );
    hit->kind = PL_LHIT_NONE;
    hit->index = hit->order = -1;
    hit->t = reach * tr.frac;
    if ( box >= 0 ) {
        hit->kind = PL_LHIT_BOX;
        hit->index = box;
        hit->order = world->item[box].order;
    }

    if ( box < 0 && brush_model > 0 ) {
        for ( i = 0; i < world->trig_count; i++ ) {
            if ( world->trig[i].kind == ENT_TRIG_SHOOT_BUTTON &&
                 world->trig[i].state == ENT_TRIG_READY &&
                 world->trig[i].model == brush_model ) {
                hit->kind = PL_LHIT_TRIG;
                hit->index = i;
                hit->order = world->trig[i].order;
                break;
            }
        }
        for ( i = 0; i < world->door_count; i++ ) {
            if ( world->door[i].shoot && world->door[i].state == ENT_DOOR_SHUT &&
                 world->door[i].model == brush_model ) {
                hit->kind = PL_LHIT_DOOR;
                hit->index = i;
                hit->order = world->door[i].order;
                break;
            }
        }
    }

    for ( i = 0; i < world->mon_count; i++ ) {
        if ( !MDL_IS_SOLID( &world->mon[i] ) ) continue;
        t = mdl_ray_monster( &world->mon[i], start, &dir, hit->t );
        if ( t >= 0.0f && t < hit->t ) {
            hit->kind = PL_LHIT_MON;
            hit->index = i;
            hit->order = world->mon[i].order;
            hit->t = t;
        }
    }
    for ( i = 0; i < world->trig_count; i++ ) {
        if ( world->trig[i].kind != ENT_TRIG_SHOOT ||
             world->trig[i].state != ENT_TRIG_READY ) continue;
        mins = world->trig[i].mins;
        maxs = world->trig[i].maxs;
        t = pl_ray_box( &mins, &maxs, start, &dir, hit->t );
        if ( t >= 0.0f && t < hit->t ) {
            hit->kind = PL_LHIT_TRIG;
            hit->index = i;
            hit->order = world->trig[i].order;
            hit->t = t;
        }
    }
    return (short) ( hit->kind != PL_LHIT_NONE );
}

static void near pl_lightning_ray( World *world, Player *player, Fight *fight,
                                    Renderer *rdr, BspVec3 *start, BspVec3 *end,
                                    short *seen, short *nseen, float damage )
{
    LightningTarget hit;
    BspVec3 impact, up = { 0.0f, 0.0f, 100.0f };
    float dx, dy, dz, len;
    short i;

    if ( !pl_lightning_target( world, start, end, &hit ) ) return;
    for ( i = 0; i < *nseen; i++ ) if ( seen[i] == hit.order ) return;
    if ( *nseen < 3 ) seen[(*nseen)++] = hit.order;

    dx = end->x - start->x; dy = end->y - start->y; dz = end->z - start->z;
    len = (float) sqrt( dx*dx + dy*dy + dz*dz );
    if ( len > 0.001f ) {
        impact.x = start->x + dx / len * hit.t;
        impact.y = start->y + dy / len * hit.t;
        impact.z = start->z + dz / len * hit.t;
        part_effect( rdr, &impact, &up, 225, PL_LG_DMG * 4 );
    }

    if ( hit.kind == PL_LHIT_MON )
        combat_damage_monster( world, player, fight, rdr, &world->mon[hit.index], damage );
    else if ( hit.kind == PL_LHIT_BOX )
        combat_damage_box( world, player, fight, rdr, hit.index, damage );
    else if ( hit.kind == PL_LHIT_TRIG )
        ent_trig_shot( world, player, fight, rdr, hit.index );
    else if ( hit.kind == PL_LHIT_DOOR )
        ent_door_fire( world, player, fight, rdr, world->door[hit.index].link );
}

static void near pl_lightning_damage( World *world, Player *player, Fight *fight,
                                       Renderer *rdr, BspVec3 *start, BspVec3 *end,
                                       float damage )
{
    BspVec3 side, s, e;
    short seen[3], nseen = 0;

    pl_lightning_side( start, end, &side );
    pl_lightning_ray( world, player, fight, rdr, start, end, seen, &nseen, damage );
    s.x = start->x + side.x; s.y = start->y + side.y; s.z = start->z;
    e.x = end->x + side.x; e.y = end->y + side.y; e.z = end->z;
    pl_lightning_ray( world, player, fight, rdr, &s, &e, seen, &nseen, damage );
    s.x = start->x - side.x; s.y = start->y - side.y;
    e.x = end->x - side.x; e.y = end->y - side.y;
    pl_lightning_ray( world, player, fight, rdr, &s, &e, seen, &nseen, damage );
}

/* EF_MUZZLEFLASH on the player. The relink turns the player entity's
   angles, whose pitch is the view's over -3, through AngleVectors, which
   reads pitch the view's way: so looking down lifts the light. */
static void pl_muzzle_light( Player *player, Camera *cam, Renderer *rdr )
{
    BspVec3 fwd;
    float ax = cam->look_at.x - cam->pos.x;
    float ay = cam->look_at.z - cam->pos.z;   /* renderer z is bsp y */
    float az = cam->look_at.y - cam->pos.y;
    float h  = (float) sqrt( ax*ax + ay*ay );
    float p  = (float) atan2( az, h ) / 3.0f;

    if ( h < 0.001f ) { ax = 1.0f; ay = 0.0f; h = 1.0f; }
    fwd.x = ax / h * (float) cos( p );
    fwd.y = ay / h * (float) cos( p );
    fwd.z = (float) -sin( p );
    dl_muzzle( rdr, DL_KEY_PLAYER, &player->pos, &fwd );
}

/* CL_ParseTEnt's TE_SPIKE: a tink, one in five a ricochet */
static void pl_spike_hit( Player *player, Rng *rng, Sound *snd, BspVec3 *at )
{
    short r;

    if ( rng_int( rng ) % 5 ) { snd_play( snd, player, SND_TINK, at ); return; }
    r = (short) ( rng_int( rng ) & 3 );
    snd_play( snd, player, (short) ( r == 1 ? SND_RIC1 : r == 2 ? SND_RIC1 + 1 : SND_RIC1 + 2 ), at );
}

static void pl_fire_nail( Player *player, Sound *snd, Camera *cam, Fight *fight, Renderer *rdr )
{
    BspVec3 aim;
    float rx, ry, rl;
    short i, super;

    if ( fight->nails <= 0 ) return;
    i = pl_nail_free( fight );
    if ( i < 0 ) return;

    /* W_FireSuperSpikes with two nails: both from the middle for 18.
       With one left the super nailgun fires as the nailgun. */
    super = (short) ( fight->weapon == PL_IT_SNG && fight->nails >= 2 );
    fight->next_fire = rdr->anim_time + PL_NG_RATE;
    fight->fire_at = rdr->anim_time;
    fight->show_hostile = rdr->anim_time + 1.0f;
    fight->flash_until = rdr->anim_time + 0.1f;
    pl_muzzle_light( player, cam, rdr );
    fight->nails =(short) ( fight->nails - ( super ? 2 : 1 ) );
    fight->weapon_phase = (short) ( ( fight->weapon_phase + 1 ) & 1 );
    snd_self( snd, player, CHAN_WEAPON, (short) ( super ? SND_SPIKE2 : SND_NAIL ) );

    aim.x = cam->look_at.x - cam->pos.x;
    aim.y = cam->look_at.z - cam->pos.z;   /* renderer z is bsp y */
    aim.z = cam->look_at.y - cam->pos.y;

    rx = aim.y; ry = -aim.x;
    rl = (float) sqrt( rx*rx + ry*ry );
    if ( rl < 0.001f ) { rx = 1.0f; ry = 0.0f; rl = 1.0f; }
    rx = rx / rl * PL_NG_OX;
    ry = ry / rl * PL_NG_OX;
    if ( fight->weapon_phase ) { rx = -rx; ry = -ry; }
    if ( super ) { rx = 0.0f; ry = 0.0f; }

    fight->nail[i].pos.x = player->pos.x + rx;
    fight->nail[i].pos.y = player->pos.y + ry;
    fight->nail[i].pos.z = player->pos.z + PL_NG_UP;
    fight->nail[i].vel.x = aim.x * PL_NG_SPEED;
    fight->nail[i].vel.y = aim.y * PL_NG_SPEED;
    fight->nail[i].vel.z = aim.z * PL_NG_SPEED;
    fight->nail[i].die_at = rdr->anim_time + PL_NG_LIFE;
    fight->nail[i].dmg = (float) ( super ? PL_SNG_DMG : PL_NG_DMG );
    fight->nail[i].alive = -1;
}

static void pl_fire_grenade( Player *player, Sound *snd, Camera *cam, Fight *fight, Renderer *rdr )
{
    short i;

    if ( fight->rockets <= 0 ) return;
    i = pl_nail_free( fight );
    if ( i < 0 ) return;

    fight->next_fire = rdr->anim_time + PL_GL_RATE;
    fight->fire_at = rdr->anim_time;
    fight->show_hostile = rdr->anim_time + 1.0f;
    fight->rockets--;
    snd_self( snd, player, CHAN_WEAPON, SND_GRENADE );
    pl_muzzle_light( player, cam, rdr );   /* player_rocket1, as the launcher's */

    fight->nail[i].pos = player->pos;
    fight->nail[i].vel.x = ( cam->look_at.x - cam->pos.x ) * PL_GL_SPEED;
    fight->nail[i].vel.y = ( cam->look_at.z - cam->pos.z ) * PL_GL_SPEED;
    fight->nail[i].vel.z = ( cam->look_at.y - cam->pos.y ) * PL_GL_SPEED + PL_GL_UP;
    fight->nail[i].die_at = rdr->anim_time + PL_GL_FUSE;
    fight->nail[i].grenade = -1;
    fight->nail[i].dmg = PL_GL_DMG;
    fight->nail[i].alive = -1;
}

/* W_FireRocket, from the origin rather than id's 8 forward: aimed at
   the floor, those 8 units start it inside hull 1's grown solid and the
   trace then carries it through unstopped. */
static void pl_fire_rocket( Player *player, Rng *rng, Sound *snd, Camera *cam, Fight *fight, Renderer *rdr )
{
    BspVec3 aim;
    short i;

    if ( fight->rockets <= 0 ) return;
    i = pl_nail_free( fight );
    if ( i < 0 ) return;

    fight->next_fire = rdr->anim_time + PL_RL_RATE;
    fight->fire_at = rdr->anim_time;
    fight->show_hostile = rdr->anim_time + 1.0f;
    fight->flash_until = rdr->anim_time + 0.1f;
    pl_muzzle_light( player, cam, rdr );
    fight->rockets--;
    snd_self( snd, player, CHAN_WEAPON, SND_ROCKET );

    aim.x = cam->look_at.x - cam->pos.x;
    aim.y = cam->look_at.z - cam->pos.z;
    aim.z = cam->look_at.y - cam->pos.y;

    fight->nail[i].pos = player->pos;
    fight->nail[i].vel.x = aim.x * PL_RL_SPEED;
    fight->nail[i].vel.y = aim.y * PL_RL_SPEED;
    fight->nail[i].vel.z = aim.z * PL_RL_SPEED;
    fight->nail[i].die_at = rdr->anim_time + PL_RL_LIFE;
    fight->nail[i].rocket = -1;
    fight->nail[i].dmg = PL_RL_HIT + rng_float( rng ) * PL_RL_HIT_RND;
    fight->nail[i].alive = -1;
}

/* W_FireAxe: a hull-0 64-unit line from sixteen units above the player.
   The view's eye is higher, but QuakeC uses self.origin + '0 0 16'. */
static void pl_fire_axe( World *world, Player *player, Camera *cam,
                         Fight *fight, Renderer *rdr )
{
    BspVec3 org, fin, aim, hit_at, no_dir = { 0.0f, 0.0f, 0.0f };
    TraceResult tr;
    float t, bt;
    short i, best, box, dmg, stopped;

    fight->next_fire = rdr->anim_time + PL_AXE_RATE;
    fight->fire_at = rdr->anim_time;
    fight->show_hostile = rdr->anim_time + 1.0f;
    snd_self( &world->snd, player, CHAN_WEAPON, SND_AX_SWING );

    aim.x = cam->look_at.x - cam->pos.x;
    aim.y = cam->look_at.z - cam->pos.z;
    aim.z = cam->look_at.y - cam->pos.y;
    org = player->pos;
    org.z += 16.0f;
    fin.x = org.x + aim.x * PL_AXE_RANGE;
    fin.y = org.y + aim.y * PL_AXE_RANGE;
    fin.z = org.z + aim.z * PL_AXE_RANGE;
    pl_trace_point( world, &org, &fin, &tr );
    box = pl_trace_boxes( world, &org, &fin, &tr );

    best = -1;
    bt = PL_AXE_RANGE * tr.frac;
    for ( i = 0; i < world->mon_count && i < MDL_MAX_ENTS; i++ ) {
        if ( !MDL_IS_SOLID( &world->mon[i] ) ) continue;
        t = mdl_ray_monster( &world->mon[i], &org, &aim, bt );
        if ( t >= 0.0f && t < bt ) { bt = t; best = i; }
    }

    dmg = (short) ( rdr->anim_time < fight->quad_until ? PL_AXE_DMG * PL_QUAD_MUL : PL_AXE_DMG );
    if ( best >= 0 ) {
        pl_impact_at( &hit_at, &org, &aim, bt );
        snd_play( &world->snd, player, SND_AX_MEAT, &hit_at );
        part_effect( rdr, &hit_at, &no_dir, 73, PL_AXE_DMG * 4 );
        combat_damage_monster( world, player, fight, rdr, &world->mon[best], dmg );
        return;
    }

    stopped = 0;
    if ( box >= 0 ) {
        combat_damage_box( world, player, fight, rdr, box, (float) dmg );
        stopped = -1;
    } else
        pl_shot_touch( world, player, fight, rdr, &org, &aim, bt, &stopped );
    if ( tr.frac < 1.0f || stopped )
        snd_play( &world->snd, player, SND_AX_WALL, &tr.end_pos );
}

static void near pl_fire_lightning( World *world, Player *player, Camera *cam,
                                    Fight *fight, Renderer *rdr )
{
    BspVec3 aim, org, fin, damage_end;
    TraceResult tr;
    float len, damage;
    short cells;

    if ( fight->cells < 1 ) {
        fight->weapon = pl_best_weapon( fight );
        return;
    }
    fight->next_fire = rdr->anim_time + PL_LG_RATE;
    fight->fire_at = rdr->anim_time;
    fight->show_hostile = rdr->anim_time + 1.0f;
    fight->flash_until = rdr->anim_time + 0.1f;
    pl_muzzle_light( player, cam, rdr );

    if ( !fight->fire_prev || fight->weapon_phase < 0 ) {
        snd_self( &world->snd, player, CHAN_AUTO, SND_LSTART );
        fight->weapon_phase = -1;
        fight->lg_sustained = 0;
    } else fight->lg_sustained = -1;
    if ( player->water_level > 1 ) {
        cells = fight->cells;
        fight->cells = 0;
        combat_radius( world, player, fight, rdr, player->pos,
                       35.0f * cells, COMBAT_BY_PLAYER, COMBAT_NO_IGNORE );
        return;
    }
    if ( rdr->anim_time >= fight->lg_hit_at ) {
        snd_self( &world->snd, player, CHAN_WEAPON, SND_LHIT );
        fight->lg_hit_at = rdr->anim_time + 0.6f;
    }
    fight->weapon_phase = (short) ( ( fight->weapon_phase + 1 ) & 3 );
    fight->punch_pitch = -2.0f;
    fight->cells--;

    aim.x = cam->look_at.x - cam->pos.x;
    aim.y = cam->look_at.z - cam->pos.z;
    aim.z = cam->look_at.y - cam->pos.y;
    len = (float) sqrt( aim.x*aim.x + aim.y*aim.y + aim.z*aim.z );
    if ( len < 0.001f ) len = 1.0f;
    aim.x /= len; aim.y /= len; aim.z /= len;
    org = player->pos;
    org.z += 16.0f;
    fin.x = org.x + aim.x * PL_LG_RANGE;
    fin.y = org.y + aim.y * PL_LG_RANGE;
    fin.z = org.z + aim.z * PL_LG_RANGE;
    pl_trace_point( world, &org, &fin, &tr );
    d_beam_spawn( rdr, BEAM_PLAYER_KEY, MDL_FX_BOLT2, &org, &tr.end_pos );

    damage_end.x = tr.end_pos.x + aim.x * 4.0f;
    damage_end.y = tr.end_pos.y + aim.y * 4.0f;
    damage_end.z = tr.end_pos.z + aim.z * 4.0f;
    damage = (float) ( rdr->anim_time < fight->quad_until
                         ? PL_LG_DMG * PL_QUAD_MUL : PL_LG_DMG );
    pl_lightning_damage( world, player, fight, rdr, &player->pos, &damage_end, damage );
}

void pl_fire( World *world, Player *player, Camera *cam, Fight *fight,
               Renderer *rdr )
{
    BspVec3 org, fin, aim, dir, impact, no_dir = { 0.0f, 0.0f, 0.0f };
    TraceResult tr;
    short hit[MDL_MAX_ENTS];
    short i, p, best, box, npellet, pdmg;
    float t, bt, sx, sy, rate;

    if ( rdr->anim_time < fight->next_fire ) return;
    /* W_WeaponFrame's SuperDamageSound, a second apart */
    if ( rdr->anim_time < fight->quad_until && rdr->anim_time >= fight->quad_snd_at ) {
        fight->quad_snd_at = rdr->anim_time + 1.0f;
        snd_self( &world->snd, player, CHAN_BODY, SND_QUAD_SHOT );
    }
    if ( fight->weapon == PL_IT_AXE ) {
        pl_fire_axe( world, player, cam, fight, rdr );
        return;
    }
    if ( fight->weapon == PL_IT_NAILGUN || fight->weapon == PL_IT_SNG ) {
        pl_fire_nail( player, &world->snd, cam, fight, rdr );
        return;
    }
    if ( fight->weapon == PL_IT_GL ) { pl_fire_grenade( player, &world->snd, cam, fight, rdr ); return; }
    if ( fight->weapon == PL_IT_RL ) { pl_fire_rocket( player, &world->rng, &world->snd, cam, fight, rdr ); return; }
    if ( fight->weapon == PL_IT_LIGHTNING ) {
        pl_fire_lightning( world, player, cam, fight, rdr );
        return;
    }
    if ( fight->shells <= 0 ) return;

    npellet = PL_PELLETS; sx = PL_SPREAD; sy = PL_SPREAD; rate = PL_FIRE_RATE;
    if ( fight->weapon == PL_IT_SSG ) {
        rate = PL_SSG_RATE;
        if ( fight->shells >= 2 ) {
            npellet = PL_SSG_PELLETS;
            sx = PL_SSG_SPREAD_X;
            sy = PL_SSG_SPREAD_Y;
            fight->shells--;
        }
    }
    fight->next_fire = rdr->anim_time + rate;
    fight->fire_at = rdr->anim_time;
    fight->show_hostile = rdr->anim_time + 1.0f;
    fight->flash_until = rdr->anim_time + 0.1f;
    pl_muzzle_light( player, cam, rdr );
    fight->shells--;
    snd_self( &world->snd, player, CHAN_WEAPON, (short) ( npellet == PL_SSG_PELLETS ? SND_SSG : SND_SHOTGUN ) );

    /* cam->look_at is the POINT the eye looks at by now -- a direction
       only inside v_update_camera -- one unit away in renderer space,
       y up: bsp y is renderer z and bsp z is renderer y. */
    aim.x = cam->look_at.x - cam->pos.x;
    aim.y = cam->look_at.z - cam->pos.z;
    aim.z = cam->look_at.y - cam->pos.y;
    org = player->pos;
    org.z += PL_EYE;

    for ( i = 0; i < world->mon_count && i < MDL_MAX_ENTS; i++ ) hit[i] = 0;

    for ( p = 0; p < npellet; p++ ) {
        pl_spread_dir( &world->rng, &aim, sx, sy, &dir );
        fin.x = org.x + dir.x * PL_SHOT_RANGE;
        fin.y = org.y + dir.y * PL_SHOT_RANGE;
        fin.z = org.z + dir.z * PL_SHOT_RANGE;
        pl_trace( world, &org, &fin, &tr );
        box = pl_trace_boxes( world, &org, &fin, &tr );

        best = -1;
        bt = PL_SHOT_RANGE * tr.frac;
        for ( i = 0; i < world->mon_count && i < MDL_MAX_ENTS; i++ ) {
            if ( !MDL_IS_SOLID( &world->mon[i] ) ) continue;
            t = mdl_ray_monster( &world->mon[i], &org, &dir, bt );
            if ( t >= 0.0f && t < bt ) { bt = t; best = i; }
        }
        pdmg = (short) ( rdr->anim_time < fight->quad_until
                          ? PL_PELLET_DMG * PL_QUAD_MUL : PL_PELLET_DMG );
        if ( best >= 0 ) {
            hit[best] = (short) ( hit[best] + pdmg );
            pl_impact_at( &impact, &org, &dir, bt );
            part_effect( rdr, &impact, &no_dir, 73, pdmg * 4 );
        } else if ( box >= 0 ) {
            pl_impact_at( &impact, &org, &dir, bt );
            part_effect( rdr, &impact, &no_dir, 0, 20 );
            combat_damage_box( world, player, fight, rdr, box, (float) pdmg );
        } else if ( tr.frac < 1.0f ) {
            pl_impact_at( &impact, &org, &dir, bt );
            part_effect( rdr, &impact, &no_dir, 0, 20 );
        }

        if ( best < 0 && box < 0 )
            pl_shot_touch( world, player, fight, rdr, &org, &dir, bt, (short *) 0 );
    }

    for ( i = 0; i < world->mon_count && i < MDL_MAX_ENTS; i++ )
        if ( hit[i] > 0 )
            combat_damage_monster( world, player, fight, rdr, &world->mon[i], hit[i] );
}

void pl_select_weapon( Fight *fight, short impulse )
{
    static short cycle[] = { PL_IT_AXE, PL_IT_SHOTGUN, PL_IT_SSG,
                              PL_IT_NAILGUN, PL_IT_SNG, PL_IT_GL, PL_IT_RL,
                              PL_IT_LIGHTNING };
    short i, remaining;

    if ( impulse == 10 ) {
        for ( i = 0; i < 8 && cycle[i] != fight->weapon; i++ ) {}
        if ( i == 8 ) i = 0;
        for ( remaining = 0; remaining < 8; remaining++ ) {
            i = (short) ( ( i + 1 ) % 8 );
            if ( ( fight->items & cycle[i] ) &&
                 ( cycle[i] == PL_IT_AXE ||
                   ( cycle[i] == PL_IT_SHOTGUN || cycle[i] == PL_IT_SSG ? fight->shells > 0
                     : cycle[i] == PL_IT_NAILGUN || cycle[i] == PL_IT_SNG ? fight->nails > 0
                     : cycle[i] == PL_IT_LIGHTNING ? fight->cells > 0
                     : fight->rockets > 0 ) ) ) {
                fight->weapon = cycle[i];
                if ( fight->weapon == PL_IT_LIGHTNING ) fight->weapon_phase = -1;
                return;
            }
        }
        return;
    }
    if ( impulse == 1 && ( fight->items & PL_IT_AXE ) ) fight->weapon = PL_IT_AXE;
    if ( impulse == 2 ) fight->weapon = PL_IT_SHOTGUN;
    if ( impulse == 3 && ( fight->items & PL_IT_SSG ) ) fight->weapon = PL_IT_SSG;
    if ( impulse == 4 && ( fight->items & PL_IT_NAILGUN ) ) fight->weapon = PL_IT_NAILGUN;
    if ( impulse == 5 && ( fight->items & PL_IT_SNG ) ) fight->weapon = PL_IT_SNG;
    if ( impulse == 6 && ( fight->items & PL_IT_GL ) ) fight->weapon = PL_IT_GL;
    if ( impulse == 7 && ( fight->items & PL_IT_RL ) ) fight->weapon = PL_IT_RL;
    if ( impulse == 8 && ( fight->items & PL_IT_LIGHTNING ) && fight->cells > 0 ) {
        fight->weapon = PL_IT_LIGHTNING;
        fight->weapon_phase = -1;
    }
}

/* MOVETYPE_BOUNCE, either side's: what it can hurt over the step blows
   it up (GrenadeTouch); else gravity, a hull-1 trace, the velocity off
   what it hits at ClipVelocity's 1.5, and SV_Physics_Toss lays it still
   on a floor under 60 up. */
static void pl_grenade_tick( World *world, Player *player, Fight *fight,
                              Renderer *rdr, Spike *s, float dt )
{
    BspVec3 fin, dir;
    TraceResult tr;
    float t, best_t, backoff, reach;
    short best, box, contact;

    pl_grenade_segment( s, fight->gravity, dt, &fin, &dir, &reach );
    pl_trace( world, &s->pos, &fin, &tr );
    box = pl_trace_boxes( world, &s->pos, &fin, &tr );

    if ( s->hostile ) {
        if ( fight->health > 0 ) {
            t = mdl_ray_player( &player->pos, &s->pos, &dir, reach * tr.frac );
            if ( t >= 0.0f && s->gib ) {
                /* ZombieGrenadeTouch: z_hit on what bleeds; a spent one
                   (SUB_Remove) just goes */
                if ( s->gib < 0 ) {
                    combat_damage_player( player, world, fight, rdr, s->dmg );
                    snd_play( &world->snd, player, SND_Z_HIT, &s->pos );
                }
                s->alive = 0;
                return;
            }
            if ( t >= 0.0f ) {
                s->pos.x += dir.x * t;
                s->pos.y += dir.y * t;
                s->pos.z += dir.z * t;
                pl_grenade_explode( world, player, fight, rdr, s );
                return;
            }
        }
    } else {
        best_t = reach * tr.frac;
        best = pl_grenade_monster( world->mon, world->mon_count,
                                   &s->pos, &dir, &best_t );
        if ( best >= 0 ) {
            s->pos.x += dir.x * best_t;
            s->pos.y += dir.y * best_t;
            s->pos.z += dir.z * best_t;
            pl_grenade_explode( world, player, fight, rdr, s );
            return;
        }
    }

    s->pos = tr.end_pos;
    contact = pl_grenade_contact( box, tr.frac );
    if ( contact == PL_CONTACT_BOX ) {
        if ( s->gib ) {
            if ( s->gib < 0 )
                combat_damage_box( world, player, fight, rdr, box, (float) s->dmg );
            s->alive = 0;
        } else
            pl_grenade_explode( world, player, fight, rdr, s );
        return;
    }
    if ( contact == PL_CONTACT_NONE ) return;
    if ( s->gib ) {
        /* the miss's z_miss, and its touch is SUB_Remove from then */
        if ( s->gib > 0 ) { s->alive = 0; return; }
        snd_play( &world->snd, player, SND_Z_MISS, &s->pos );
        s->gib = 1;
        s->vel.x = s->vel.y = s->vel.z = 0.0f;
        return;
    }

    snd_play( &world->snd, player, SND_BOUNCE, &s->pos );
    backoff = ( s->vel.x * tr.norm.x + s->vel.y * tr.norm.y +
                s->vel.z * tr.norm.z ) * PL_BOUNCE;
    s->vel.x -= tr.norm.x * backoff;
    s->vel.y -= tr.norm.y * backoff;
    s->vel.z -= tr.norm.z * backoff;
    if ( tr.norm.z > 0.7f && s->vel.z < 60.0f )
        s->vel.x = s->vel.y = s->vel.z = 0.0f;
}

void pl_spikes_tick( World *world, Player *player, Fight *fight,
                      Renderer *rdr, float dt )
{
    BspVec3 fin, dir, impact, old, no_dir = { 0.0f, 0.0f, 0.0f };
    TraceResult tr;
    float t, bt, reach, spd;
    short n, i, best, box, stopped;
    float ndmg;

    for ( n = 0; n < PL_NAILS_MAX; n++ ) {
        Spike *s = &fight->nail[n];

        if ( !s->alive ) continue;
        old = s->pos;
        /* SV_CheckWaterTransition, sky counting as a liquid as id's does */
        i = (short) ( pl_point_contents( &s->pos, world ) <= CONTENTS_WATER ? 1 : -1 );
        if ( s->water && s->water != i ) snd_play( &world->snd, player, SND_H2OHIT, &s->pos );
        s->water = i;
        if ( rdr->anim_time >= s->die_at ) {
            s->alive = 0;
            /* a grenade goes off on its fuse; a gib just stops */
            if ( s->grenade && !s->gib )
                pl_grenade_explode( world, player, fight, rdr, s );
            continue;
        }

        fin.x = s->pos.x + s->vel.x * dt;
        fin.y = s->pos.y + s->vel.y * dt;
        fin.z = s->pos.z + s->vel.z * dt;
        spd = (float) sqrt( s->vel.x*s->vel.x + s->vel.y*s->vel.y + s->vel.z*s->vel.z );
        if ( spd < 1.0f ) spd = 1.0f;
        reach = spd * dt;
        dir.x = s->vel.x / spd;
        dir.y = s->vel.y / spd;
        dir.z = s->vel.z / spd;

        if ( s->grenade ) {
            pl_grenade_tick( world, player, fight, rdr, s, dt );
            part_trail( rdr, &old, &s->pos, 1 );
            continue;
        }

        if ( s->hostile ) {
            /* spike_touch. A trap's spike leaves from a point 8 units
               off its wall, inside hull 1's grown solid, so it walks as
               a point through hull 0: a step into solid ends it. */
            if ( s->toss == 1 ) s->vel.z -= fight->gravity * dt;
            pl_trace_point( world, &s->pos, &fin, &tr );
            box = pl_trace_boxes( world, &s->pos, &fin, &tr );
            t = fight->health > 0
                ? mdl_ray_player( &player->pos, &s->pos, &dir, reach * tr.frac )
                : -1.0f;
            if ( t >= 0.0f ) {
                part_effect( rdr, &s->pos, &no_dir, 73, (short) ( s->dmg * 4.0f ) );
                combat_damage_player( player, world, fight, rdr, s->dmg );
                s->alive = 0;
            } else if ( box >= 0 ) {
                combat_damage_box( world, player, fight, rdr, box, (float) s->dmg );
                s->alive = 0;
            } else if ( tr.frac < 1.0f ) {
                s->alive = 0;
                /* TE_WIZSPIKE or TE_SPIKE; a lava ball says nothing */
                if ( s->wiz ) {
                    part_effect( rdr, &s->pos, &no_dir, 20, 30 );
                    snd_play( &world->snd, player, SND_WIZ_HIT, &s->pos );
                } else if ( !s->toss ) {
                    part_effect( rdr, &s->pos, &no_dir, 0, 10 );
                    pl_spike_hit( player, &world->rng, &world->snd, &s->pos );
                }
            } else {
                s->pos = tr.end_pos;
            }
            if ( s->gib ) part_trail( rdr, &old, &s->pos, 2 );
            continue;
        }

        pl_trace( world, &s->pos, &fin, &tr );
        box = pl_trace_boxes( world, &s->pos, &fin, &tr );
        bt = reach * tr.frac;
        best = -1;
        for ( i = 0; i < world->mon_count; i++ ) {
            if ( !MDL_IS_SOLID( &world->mon[i] ) ) continue;
            t = mdl_ray_monster( &world->mon[i], &s->pos, &dir, bt );
            if ( t >= 0.0f && t < bt ) { bt = t; best = i; }
        }
        ndmg = (float) ( rdr->anim_time < fight->quad_until
                          ? s->dmg * PL_QUAD_MUL : s->dmg );
        if ( best >= 0 ) {
            if ( s->rocket ) s->ignore = world->mon[best].order;
            if ( s->rocket && world->mon[best].kind == MDL_KIND_SHAMBLER ) ndmg *= 0.5f;
            pl_impact_at( &impact, &s->pos, &dir, bt );
            part_effect( rdr, &impact, &no_dir, 73, (short) ( ndmg * 4.0f ) );
            combat_damage_monster( world, player, fight, rdr, &world->mon[best], ndmg );
            s->alive = 0;
        } else if ( box >= 0 ) {
            combat_damage_box( world, player, fight, rdr, box, ndmg );
            s->alive = 0;
        }

        stopped = 0;
        if ( s->alive )
            pl_shot_touch( world, player, fight, rdr, &s->pos, &dir, bt, &stopped );
        if ( stopped ) s->alive = 0;
        if ( tr.frac < 1.0f ) {
            if ( s->alive && !s->rocket ) {
                part_effect( rdr, &tr.end_pos, &no_dir, 0,
                             (short) ( s->dmg >= PL_SNG_DMG ? 20 : 10 ) );
                pl_spike_hit( player, &world->rng, &world->snd, &tr.end_pos );
            }
            s->alive = 0;
        }

        if ( s->rocket && !s->alive ) {
            /* T_MissileTouch: the direct hit landed above; the blast is
               from where it stopped -- the monster met short of the
               step, or the wall. */
            s->pos.x += dir.x * bt;
            s->pos.y += dir.y * bt;
            s->pos.z += dir.z * bt;
            s->dmg = PL_RL_DMG;
            pl_grenade_explode( world, player, fight, rdr, s );
            part_trail( rdr, &old, &s->pos, 0 );
            continue;
        }
        if ( s->alive ) {
            s->pos = tr.end_pos;
            if ( s->rocket ) part_trail( rdr, &old, &s->pos, 0 );
        }
    }
}

void pl_traps_tick( World *world, Player *player, Fight *fight, Renderer *rdr )
{
    short k, n;

    for ( k = 0; k < world->trig_count; k++ ) {
        TrigEnt far *t = &world->trig[k];

        if ( t->kind == ENT_TRIG_SHOOTER && t->state == ENT_TRIG_ARMED ) {
            t->state = ENT_TRIG_READY;
            n = pl_nail_free( fight );
            if ( n < 0 ) continue;
            fight->nail[n].pos = t->mins;
            fight->nail[n].vel.x = t->ofs_out.x * t->speed;
            fight->nail[n].vel.y = t->ofs_out.y * t->speed;
            fight->nail[n].vel.z = t->ofs_out.z * t->speed;
            fight->nail[n].die_at = rdr->anim_time + PL_NG_LIFE;
            fight->nail[n].hostile = -1;
            fight->nail[n].dmg = t->count;
            fight->nail[n].alive = -1;
            snd_play( &world->snd, player, SND_SPIKE2, &fight->nail[n].pos );
        }
        if ( t->kind == ENT_TRIG_FIREBALL && t->state == ENT_TRIG_ARMED ) {
            /* fire_fly: up at speed plus up to 200, 50 either way
               across, five seconds; fire_touch bites 20 and is gone */
            t->state = ENT_TRIG_READY;
            n = pl_nail_free( fight );
            if ( n < 0 ) continue;
            fight->nail[n].pos = t->mins;
            fight->nail[n].vel.x = rng_float( &world->rng ) * 100.0f - 50.0f;
            fight->nail[n].vel.y = rng_float( &world->rng ) * 100.0f - 50.0f;
            fight->nail[n].vel.z = t->speed + rng_float( &world->rng ) * 200.0f;
            fight->nail[n].die_at = rdr->anim_time + 5.0f;
            fight->nail[n].hostile = -1;
            fight->nail[n].toss = 1;
            fight->nail[n].dmg = PL_FIREBALL_DMG;
            fight->nail[n].alive = -1;
            t->left++;
        }
    }
}

/* Wiz_FastFire: a spike from 30 up, straight at the player at 600. */
void mdl_spike( World *world, Player *player, Fight *fight, Renderer *rdr,
                 MdlEnt far *ent )
{
    BspVec3 d;
    float l;
    short n;

    (void) world;
    n = pl_nail_free( fight );
    if ( n < 0 ) return;
    d.x = player->pos.x - ent->pos.x;
    d.y = player->pos.y - ent->pos.y;
    d.z = player->pos.z - ( ent->pos.z + WIZARD_SPIKE_Z );
    l = (float) sqrt( d.x*d.x + d.y*d.y + d.z*d.z );
    if ( l < 1.0f ) return;

    fight->nail[n].pos = ent->pos;
    fight->nail[n].pos.z += WIZARD_SPIKE_Z;
    fight->nail[n].vel.x = d.x / l * WIZARD_SPIKE_SPEED;
    fight->nail[n].vel.y = d.y / l * WIZARD_SPIKE_SPEED;
    fight->nail[n].vel.z = d.z / l * WIZARD_SPIKE_SPEED;
    fight->nail[n].die_at = rdr->anim_time + PL_NG_LIFE;
    fight->nail[n].hostile = -1;
    fight->nail[n].owner = ent->order;
    fight->nail[n].wiz = -1;
    fight->nail[n].dmg = WIZARD_SPIKE_DMG;
    fight->nail[n].alive = -1;
}

/* OgreFireGrenade: toward the player at 600 with 200 up. */
void mdl_grenade( Player *player, Sound *snd, Fight *fight, Renderer *rdr, MdlEnt far *ent )
{
    BspVec3 d;
    float l;
    short n;

    n = pl_nail_free( fight );
    if ( n < 0 ) return;
    d.x = player->pos.x - ent->pos.x;
    d.y = player->pos.y - ent->pos.y;
    d.z = player->pos.z - ent->pos.z;
    l = (float) sqrt( d.x*d.x + d.y*d.y + d.z*d.z );
    if ( l < 1.0f ) return;

    fight->nail[n].pos = ent->pos;
    fight->nail[n].vel.x = d.x / l * OGRE_GREN_SPEED;
    fight->nail[n].vel.y = d.y / l * OGRE_GREN_SPEED;
    fight->nail[n].vel.z = OGRE_GREN_UP;
    fight->nail[n].die_at = rdr->anim_time + OGRE_GREN_FUSE;
    fight->nail[n].hostile = -1;
    fight->nail[n].grenade = -1;
    fight->nail[n].owner = ent->order;
    fight->nail[n].dmg = OGRE_GREN_DMG;
    fight->nail[n].alive = -1;
    mdl_voice( player, snd, ent, CHAN_WEAPON, SND_GRENADE, ATTN_NORM );
}

/* ZombieFireGrenade: 600 toward the player with 200 up, biting 10 where
   it lands and going out with no blast. */
void mdl_gib( Player *player, Sound *snd, Fight *fight, Renderer *rdr, MdlEnt far *ent )
{
    BspVec3 d;
    float l;
    short n;

    n = pl_nail_free( fight );
    if ( n < 0 ) return;
    d.x = player->pos.x - ent->pos.x;
    d.y = player->pos.y - ent->pos.y;
    d.z = player->pos.z - ( ent->pos.z + ZOMBIE_GIB_Z );
    l = (float) sqrt( d.x*d.x + d.y*d.y + d.z*d.z );
    if ( l < 1.0f ) return;

    fight->nail[n].pos = ent->pos;
    fight->nail[n].pos.z += ZOMBIE_GIB_Z;
    fight->nail[n].vel.x = d.x / l * ZOMBIE_GIB_SPEED;
    fight->nail[n].vel.y = d.y / l * ZOMBIE_GIB_SPEED;
    fight->nail[n].vel.z = ZOMBIE_GIB_UP;
    fight->nail[n].die_at = rdr->anim_time + ZOMBIE_GIB_LIFE;
    fight->nail[n].hostile = -1;
    fight->nail[n].grenade = -1;
    fight->nail[n].owner = ent->order;
    fight->nail[n].gib = -1;
    fight->nail[n].dmg = ZOMBIE_GIB_DMG;
    fight->nail[n].alive = -1;
    mdl_say( player, snd, ent, 1 );
}
