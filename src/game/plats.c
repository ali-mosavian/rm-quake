/*
 * plats.c -- func_plat and func_train, which share the plat array.
 */

#include "plats.h"
#include <math.h>
#include "ent.h"
#include "mathlib.h"
#include "pl_move.h"
#include "pl_trace.h"
#include "combat.h"
#include "snd.h"

/*
 * name: ent_plat_touched
 * desc: True when the player is standing on a plat, or in the column
 *       above it. Quake builds a trigger brush around the plat for
 *       this; the box is close enough and needs nothing from the
 *       compiler.
 */
static short near ent_mover_xy( Player *player, PlatEnt far *plt,
                                BrushModel far *br )
{
    if ( player->pos.x + 16.0f < plt->mins.x + br->ofs.x ) return 0;
    if ( player->pos.x - 16.0f > plt->maxs.x + br->ofs.x ) return 0;
    if ( player->pos.y + 16.0f < plt->mins.y + br->ofs.y ) return 0;
    if ( player->pos.y - 16.0f > plt->maxs.y + br->ofs.y ) return 0;
    return 1;
}

/* The plat trigger is a column above the brush. It starts the lift, but
   is not evidence that a TOSS/BOUNCE body is supported by the lift. */
static short near ent_plat_activated( Player *player, PlatEnt far *plt )
{
    BspVec3 mins, maxs;

    /* Mod_LoadSubmodels spreads the model by one before QC sees mins/maxs. */
    mins.x = plt->mins.x - 1.0f + 25.0f;
    maxs.x = plt->maxs.x + 1.0f - 25.0f;
    mins.y = plt->mins.y - 1.0f + 25.0f;
    maxs.y = plt->maxs.y + 1.0f - 25.0f;
    if ( plt->maxs.x - plt->mins.x + 2.0f <= 50.0f ) {
        mins.x = ( plt->mins.x + plt->maxs.x ) * 0.5f;
        maxs.x = mins.x + 1.0f;
    }
    if ( plt->maxs.y - plt->mins.y + 2.0f <= 50.0f ) {
        mins.y = ( plt->mins.y + plt->maxs.y ) * 0.5f;
        maxs.y = mins.y + 1.0f;
    }

    maxs.z = plt->maxs.z + 1.0f + 8.0f;
    mins.z = maxs.z - ( plt->time.travel + 8.0f );
    if ( plt->start.low_trigger ) maxs.z = mins.z + 8.0f;

    return ent_box_touched( player, &mins, &maxs, ENT_TOUCH_SLACK );
}

/* We do not retain groundentity, so support is reconstructed from the
   grounded flag and the feet being at the mover's top. */
static short near ent_mover_supported( Player *player, PlatEnt far *plt,
                                       BrushModel far *br, short head )
{
    float bottom, top;

    if ( !player->on_ground || !ent_mover_xy( player, plt, br ) ) return 0;
    bottom = player->pos.z - ( head ? 0.0f : PL_FEET );
    top = plt->maxs.z + br->ofs.z;
    return (short) ( bottom >= top - 8.0f && bottom <= top + 8.0f );
}

/* SV_PushMove also moves an entity caught by the pusher's final box,
   even when it was not standing on it before the move. */
static short near ent_mover_intersects( World *world, Player *player,
                                        PlatEnt far *plt, BrushModel far *br,
                                        short head )
{
    float bottom, top;
    BspVec3 org;
    TraceResult tr;

    if ( !ent_mover_xy( player, plt, br ) ) return 0;
    bottom = player->pos.z - ( head ? 0.0f : PL_FEET );
    top = player->pos.z + ( head ? 56.0f : 32.0f );
    if ( top < plt->mins.z + br->ofs.z ) return 0;
    if ( bottom > plt->maxs.z + br->ofs.z ) return 0;

    org = player->pos;
    /* HeadPlayer's 0..56 box is hull 1's -24..32 shifted up 24. */
    if ( head ) org.z += PL_FEET;
    pl_trace( world, &org, &org, &tr );
    return tr.all_solid;
}

short ent_plat_touched( Player *player, World *world, short p )
{
    PlatEnt far *plt = &world->plat[p];
    return ent_plat_activated( player, plt );
}

/*
 * name: ent_move_plats
 * desc: Drives every func_plat, and carries whoever is riding one.
 *
 *       A plat rises while the player is on it and returns when they
 *       leave, which is Quake's behaviour without the delay and the
 *       sounds.
 *
 *       The rider is moved by the same delta the plat moved. Quake
 *       does this properly in SV_PushMove, which re-traces everything
 *       the mover touches and telefrags what it cannot push; this
 *       carries the one entity that exists.
 */
void ent_move_plats( World *world, Player *player, float dt, short rider_kind )
{
    short p, snd;

    for ( p = 0; p < world->plat_count; p++ ) {
        PlatEnt far *plt = &world->plat[p];
        BrushModel far *br = &world->brush[plt->model];
        short head = (short) ( rider_kind == ENT_RIDER_HEAD );
        short activated = ent_plat_activated( player, plt );
        short riding = ent_mover_supported( player, plt, br, head );
        float goal, step_z, moved, was;

        if ( plt->kind != ENT_PLAT_KIND_PLAT ) continue;   /* a train is ent_move_trains' */

        if ( plt->state == ENT_PLAT_BOTTOM ) {
            if ( rider_kind == ENT_RIDER_LIVE && activated ) plt->state = ENT_PLAT_UP;
        } else if ( plt->state == ENT_PLAT_TOP ) {
            if ( rider_kind == ENT_RIDER_LIVE && activated ) plt->wait_left = 1.0f;
            else {
                plt->wait_left -= dt;
                if ( plt->wait_left <= 0.0f ) plt->state = ENT_PLAT_DOWN;
            }
        }
        goal = ( plt->state == ENT_PLAT_UP || plt->state == ENT_PLAT_TOP )
             ? 0.0f : -plt->time.travel;

        was = br->ofs.z;
        if ( plt->state == ENT_PLAT_UP && br->ofs.z < goal ) {
            step_z = plt->speed * dt;
            br->ofs.z += step_z;
            if ( br->ofs.z > goal ) br->ofs.z = goal;
        } else if ( plt->state == ENT_PLAT_DOWN && br->ofs.z > goal ) {
            step_z = plt->speed * dt;
            br->ofs.z -= step_z;
            if ( br->ofs.z < goal ) br->ofs.z = goal;
        }
        moved = br->ofs.z - was;
        if ( moved != 0.0f ) br->node = ENT_NODE_DIRTY;
        if ( moved != 0.0f ) {
            /* plat_go_up and _down: the move, looping; plat_hit_top and
               _bottom: the stop, on the same channel */
            snd = (short) ( SND_PLAT + ( plt->snd - 1 ) * 2 );
            if ( !plt->mode.moving ) ent_brush_sound( player, world, plt->model, snd );
            plt->mode.moving = (short) ( br->ofs.z != goal );
            if ( !plt->mode.moving ) ent_brush_sound( player, world, plt->model, (short) ( snd + 1 ) );
        }

        if ( plt->state == ENT_PLAT_UP && br->ofs.z == 0.0f ) {
            plt->state = ENT_PLAT_TOP;
            plt->wait_left = 3.0f;
        } else if ( plt->state == ENT_PLAT_DOWN && br->ofs.z == -plt->time.travel ) {
            plt->state = ENT_PLAT_BOTTOM;
        }

        if ( !riding && moved != 0.0f )
            riding = ent_mover_intersects( world, player, plt, br, head );

        /* A walking rider follows only upward. SV_PushMove also carries
           TOSS/BOUNCE corpses in either direction and clears onground. */
        if ( moved != 0.0f && riding &&
             ( moved > 0.0f || rider_kind != ENT_RIDER_LIVE ) ) {
            player->pos.z += moved;
            if ( rider_kind != ENT_RIDER_LIVE ) player->on_ground = 0;
        }
    }
}

/* func_train_find: the brush's mins to its first corner. One with a
   targetname waits there for its trigger; the rest go on at once. */
void ent_train_init( World *world, PlatEnt far *p )
{
    BrushModel far *b = &world->brush[ p->model ];

    p->corner = p->first;
    b->ofs.x = world->corner[ p->first ].org.x - p->mins.x;
    b->ofs.y = world->corner[ p->first ].org.y - p->mins.y;
    b->ofs.z = world->corner[ p->first ].org.z - p->mins.z;
    b->node = ENT_NODE_DIRTY;
    p->wait_left = 0.0f;
    p->state = p->start.targeted ? ENT_TRAIN_IDLE : ENT_TRAIN_WAIT;
}

static short near ent_train_push( World *world, Player *player,
                                  PlatEnt far *p, BrushModel far *b,
                                  BspVec3 *was, short riding, short rider_kind )
{
    BspVec3 saved, start, fin;
    TraceResult tr;
    short head = (short) ( rider_kind == ENT_RIDER_HEAD );
    short solid;

    if ( player->no_clip ) return -1;
    if ( !riding ) riding = ent_mover_intersects( world, player, p, b, head );
    if ( !riding ) return -1;
    if ( rider_kind != ENT_RIDER_LIVE ) player->on_ground = 0;

    saved = player->pos;
    start = player->pos;
    if ( head ) start.z += PL_FEET;
    fin.x = start.x + b->ofs.x - was->x;
    fin.y = start.y + b->ofs.y - was->y;
    fin.z = start.z + b->ofs.z - was->z;
    solid = b->solid;
    b->solid = 0;
    if ( rider_kind == ENT_RIDER_LIVE )
        pl_trace_move( world, &start, &fin, -1, &tr, (short *) 0 );
    else
        pl_trace( world, &start, &fin, &tr );
    b->solid = solid;
    player->pos = tr.end_pos;
    if ( head ) player->pos.z -= PL_FEET;
    start = player->pos;
    if ( head ) start.z += PL_FEET;
    if ( rider_kind == ENT_RIDER_LIVE )
        pl_trace_move( world, &start, &start, -1, &tr, (short *) 0 );
    else
        pl_trace( world, &start, &start, &tr );
    if ( tr.all_solid ) {
        if ( rider_kind != ENT_RIDER_LIVE ) return -1;
        player->pos = saved;
        return 0;
    }
    return -1;
}

void ent_move_trains( World *world, Player *player, Fight *fight,
                      Renderer *rdr, float dt, short rider_kind )
{
    short i, riding, arrived, kind;
    PlatEnt far *p;
    BrushModel far *b;
    BspVec3 goal, was;

    for ( i = 0; i < world->plat_count; i++ ) {
        p = &world->plat[i];
        if ( p->kind != ENT_PLAT_KIND_TRAIN ) continue;
        if ( p->state == ENT_TRAIN_IDLE ) continue;
        if ( p->first < 0 || p->first >= world->corner_count ) continue;
        kind = rider_kind;
        if ( kind == ENT_RIDER_LIVE && fight->health <= 0 ) kind = ENT_RIDER_DEAD;

        if ( p->state == ENT_TRAIN_WAIT ) {
            p->wait_left -= dt;
            if ( p->wait_left > 0.0f ) continue;
            if ( world->corner[ p->corner ].nxt < 0 ) continue;
            p->corner = world->corner[ p->corner ].nxt;
            p->state = ENT_TRAIN_MOVE;
            p->wait_left = 0.0f;
            /* train_next: noise1, the move, from every corner */
            if ( p->snd ) ent_brush_sound( player, world, p->model, SND_TRAIN + 1 );
        }

        b = &world->brush[ p->model ];
        goal.x = world->corner[ p->corner ].org.x - p->mins.x;
        goal.y = world->corner[ p->corner ].org.y - p->mins.y;
        goal.z = world->corner[ p->corner ].org.z - p->mins.z;

        riding = ent_mover_supported( player, p, b,
                                      (short) ( kind == ENT_RIDER_HEAD ) );
        was = b->ofs;

        arrived = ent_step_to( &b->ofs, &goal, p->speed * dt );

        if ( ( b->ofs.x != was.x || b->ofs.y != was.y || b->ofs.z != was.z ) &&
             !ent_train_push( world, player, p, b, &was, riding, kind ) ) {
            b->ofs = was;
            if ( kind == ENT_RIDER_LIVE && rdr->anim_time >= p->time.block_at ) {
                float dx = player->pos.x - ( p->mins.x + p->maxs.x ) * 0.5f - was.x;
                float dy = player->pos.y - ( p->mins.y + p->maxs.y ) * 0.5f - was.y;
                float dz = player->pos.z - ( p->mins.z + p->maxs.z ) * 0.5f - was.z;
                float len = (float) sqrt( dx*dx + dy*dy + dz*dz );
                float kick = p->mode.dmg * 8.0f;
                p->time.block_at = rdr->anim_time + 0.5f;
                if ( len > 0.0f ) {
                    player->vel.x += dx / len * kick;
                    player->vel.y += dy / len * kick;
                    player->vel.z += dz / len * kick;
                }
                combat_damage_player( player, world, fight, rdr, p->mode.dmg );
            }
            continue;
        }

        if ( b->ofs.x != was.x || b->ofs.y != was.y || b->ofs.z != was.z )
            b->node = ENT_NODE_DIRTY;

        if ( arrived ) {
            p->state = ENT_TRAIN_WAIT;
            p->wait_left = world->corner[ p->corner ].wait;
            /* train_wait: noise, the stop, only at a corner that waits */
            if ( p->snd && p->wait_left != 0.0f ) ent_brush_sound( player, world, p->model, SND_TRAIN );
            /* id's -1 is a wait of nothing, not a wait for ever */
            if ( p->wait_left <= 0.0f ) p->wait_left = 0.1f;
        }
    }
}
