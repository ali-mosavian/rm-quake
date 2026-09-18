/* gib.c -- player.qc's ThrowGib, ThrowHead and gib bounce physics. */

#include <mem.h>
#include <string.h>

#include "gib.h"
#include "mdl_ai.h"
#include "part.h"
#include "pl_trace.h"
#include "qgl.h"

#define GIB_EMS_SLOT 2

typedef char GibPoolFitsFrameStore[
    ( GIB_MAX * sizeof(GibEnt) <= RDR_GIB_BYTES ) ? 1 : -1 ];

GibEnt far *gib_map( Renderer *rdr, short slot )
{
    unsigned char far *base;

    if ( !rdr->frame_dc ) return 0;
    base = (unsigned char far *) qglSfAccessWrEx( rdr->frame_dc, 0, slot );
    return base ? (GibEnt far *) ( base + RDR_GIB_OFS ) : 0;
}

static short near gib_head_model( short kind )
{
    switch ( kind ) {
    case MDL_KIND_ARMY:    return MDL_FX_HGUARD;
    case MDL_KIND_KNIGHT:  return MDL_FX_HKNIGHT;
    case MDL_KIND_DOG:     return MDL_FX_HDOG;
    case MDL_KIND_OGRE:    return MDL_FX_HOGRE;
    case MDL_KIND_DEMON:   return MDL_FX_HDEMON;
    case MDL_KIND_ZOMBIE:  return MDL_FX_HZOMBIE;
    case MDL_KIND_WIZARD:  return MDL_FX_HWIZARD;
    default:               return MDL_FX_HSHAMS;
    }
}

static GibEnt far *near gib_slot( Renderer *rdr, GibEnt far *pool )
{
    short i;

    for ( i = 0; i < rdr->gib_count; i++ ) if ( !pool[i].model ) return &pool[i];
    if ( rdr->gib_count >= GIB_MAX ) return 0;
    return &pool[rdr->gib_count++];
}

static float near gib_crandom( Rng *rng )
{
    return rng_float( rng ) * 2.0f - 1.0f;
}

static void near gib_velocity( Rng *rng, BspVec3 far *vel, short damage )
{
    float scale;

    vel->x = 100.0f * gib_crandom( rng );
    vel->y = 100.0f * gib_crandom( rng );
    vel->z = 200.0f + 100.0f * rng_float( rng );
    scale = damage > -50 ? 0.7f : damage > -200 ? 2.0f : 10.0f;
    vel->x *= scale; vel->y *= scale; vel->z *= scale;
}

static void near gib_player_head( Rng *rng, Player *player, short damage )
{
    player->pos.z -= 24.0f;
    gib_velocity( rng, &player->vel, damage );
    (void) gib_crandom( rng ); /* ThrowHead's angular yaw */
}

short gib_selftest( void )
{
    Rng got, want;
    Player player;
    BspVec3 vel;

    memset( &player, 0, sizeof(player) );
    player.pos.z = 100.0f;
    rng_init( &got );
    rng_init( &want );
    gib_velocity( &want, &vel, -99 );
    (void) gib_crandom( &want );
    gib_player_head( &got, &player, -99 );
    return (short) ( player.pos.z == 76.0f && player.vel.x == vel.x &&
                     player.vel.y == vel.y && player.vel.z == vel.z &&
                     got.seed == want.seed );
}

static void near gib_one( World *world, Renderer *rdr, GibEnt far *pool,
                           BspVec3 far *org, short model, short damage,
                           float now, short head )
{
    GibEnt far *g = gib_slot( rdr, pool );

    if ( !g ) return;
    _fmemset( g, 0, sizeof(*g) );
    g->pos = *org;
    g->model = (unsigned char) model;
    g->flags = head ? GIB_HEAD : 0;
    gib_velocity( &world->rng, &g->vel, damage );
    if ( head ) {
        g->pos.z -= 24.0f;
        g->avel[1] = (short) ( gib_crandom( &world->rng ) * 600.0f );
        g->die_at = 0.0f;
    } else {
        g->avel[0] = (short) ( rng_float( &world->rng ) * 600.0f );
        g->avel[1] = (short) ( rng_float( &world->rng ) * 600.0f );
        g->avel[2] = (short) ( rng_float( &world->rng ) * 600.0f );
        g->die_at = now + 10.0f + rng_float( &world->rng ) * 10.0f;
    }
}

static void near gib_three( World *world, Renderer *rdr, GibEnt far *pool,
                             BspVec3 far *org, short a, short b, short c,
                             short damage, float now )
{
    gib_one( world, rdr, pool, org, a, damage, now, 0 );
    gib_one( world, rdr, pool, org, b, damage, now, 0 );
    gib_one( world, rdr, pool, org, c, damage, now, 0 );
}

void gib_spawn_monster( World *world, Renderer *rdr, MdlEnt far *ent, float now )
{
    short a = MDL_FX_GIB1, b = MDL_FX_GIB2, c = MDL_FX_GIB3;
    GibEnt far *pool = gib_map( rdr, GIB_EMS_SLOT );

    if ( !pool ) return;

    if ( ent->kind == MDL_KIND_DOG || ent->kind == MDL_KIND_OGRE )
        a = b = c = MDL_FX_GIB3;
    else if ( ent->kind == MDL_KIND_DEMON )
        a = b = c = MDL_FX_GIB1;
    else if ( ent->kind == MDL_KIND_WIZARD )
        a = b = c = MDL_FX_GIB2;

    gib_one( world, rdr, pool, &ent->pos, gib_head_model( ent->kind ), ent->health, now, -1 );
    gib_three( world, rdr, pool, &ent->pos, a, b, c, ent->health, now );
    ent->state = MDL_ST_GIBBED;
    ent->anim_frame = 0;
}

void gib_spawn_player( World *world, Renderer *rdr, Player *player,
                       Fight *fight, float now )
{
    GibEnt far *pool;

    /* ThrowHead turns the player itself into h_player. The local player is
       not rendered, but its bouncing origin remains the camera carrier. */
    gib_player_head( &world->rng, player, fight->health );
    pool = gib_map( rdr, GIB_EMS_SLOT );
    if ( !pool ) return;
    gib_three( world, rdr, pool, &player->pos, MDL_FX_GIB1,
                MDL_FX_GIB2, MDL_FX_GIB3, fight->health, now );
}

void gib_tick( World *world, Renderer *rdr, float dt )
{
    short i;
    GibEnt far *pool;

    if ( !rdr->gib_count ) return;
    pool = gib_map( rdr, GIB_EMS_SLOT );
    if ( !pool ) return;
    for ( i = 0; i < rdr->gib_count; i++ ) {
        GibEnt far *g = &pool[i];
        BspVec3 old, fin;
        TraceResult tr;
        float backoff;

        if ( !g->model ) continue;
        if ( g->die_at > 0.0f && rdr->anim_time >= g->die_at ) {
            g->model = 0;
            continue;
        }
        if ( g->flags & GIB_GROUND ) continue;
        old = g->pos;
        g->vel.z -= 800.0f * dt;
        g->ang.x += g->avel[0] * dt;
        g->ang.y += g->avel[1] * dt;
        g->ang.z += g->avel[2] * dt;
        if ( g->flags & GIB_HEAD ) old.z += PL_FEET;
        fin.x = old.x + g->vel.x * dt;
        fin.y = old.y + g->vel.y * dt;
        fin.z = old.z + g->vel.z * dt;
        if ( g->flags & GIB_HEAD ) pl_trace( world, &old, &fin, &tr );
        else pl_trace_point( world, &old, &fin, &tr );
        g->pos = tr.end_pos;
        if ( g->flags & GIB_HEAD ) g->pos.z -= PL_FEET;
        if ( tr.frac < 1.0f ) {
            backoff = ( g->vel.x * tr.norm.x + g->vel.y * tr.norm.y +
                        g->vel.z * tr.norm.z ) * PL_BOUNCE;
            g->vel.x -= tr.norm.x * backoff;
            g->vel.y -= tr.norm.y * backoff;
            g->vel.z -= tr.norm.z * backoff;
            if ( tr.norm.z > 0.7f && g->vel.z < 60.0f ) {
                g->vel.x = g->vel.y = g->vel.z = 0.0f;
                g->avel[0] = g->avel[1] = g->avel[2] = 0;
                g->flags |= GIB_GROUND;
            }
        }
        if ( !( g->flags & GIB_HEAD ) ) {
            fin = g->pos;
            part_trail( rdr, &old, &fin, 2 );
        }
    }
}

void gib_clear( Renderer *rdr )
{
    rdr->gib_count = 0;
}
