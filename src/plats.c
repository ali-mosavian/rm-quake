/*
 * plats.c -- func_plat and func_train, which share the plat array.
 */

#include "plats.h"
#include "ent.h"
#include "mathlib.h"
#include "pl_move.h"
#include "snd.h"

/*
 * name: ent_plat_touched
 * desc: True when the player is standing on a plat, or in the column
 *       above it. Quake builds a trigger brush around the plat for
 *       this; the box is close enough and needs nothing from the
 *       compiler.
 */
short ent_plat_touched( Player *player, World *world, short p )
{
    PlatEnt far *plt = &world->plat[p];
    float top;

    if ( player->pos.x + 16.0f < plt->mins.x ) return 0;
    if ( player->pos.x - 16.0f > plt->maxs.x ) return 0;
    if ( player->pos.y + 16.0f < plt->mins.y ) return 0;
    if ( player->pos.y - 16.0f > plt->maxs.y ) return 0;

    /* Above its surface and within a body's height of it. Anything
       higher is someone on a walkway over the shaft, not a passenger. */
    top = plt->maxs.z + world->brush[plt->model].ofs.z;

    if ( player->pos.z - PL_FEET < top - 8.0f  ) return 0;
    if ( player->pos.z - PL_FEET > top + 64.0f ) return 0;

    return 1;
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
void ent_move_plats( World *world, Player *player, float dt )
{
    short p, snd;

    for ( p = 0; p < world->plat_count; p++ ) {
        PlatEnt far *plt = &world->plat[p];
        BrushModel far *br = &world->brush[plt->model];
        short riding = ent_plat_touched( player, world, p );
        float goal, step_z, moved, was;

        if ( plt->kind != ENT_PLAT_KIND_PLAT ) continue;   /* a train is ent_move_trains' */

        plt->state = riding ? ENT_PLAT_UP : ENT_PLAT_DOWN;
        goal = ( plt->state == ENT_PLAT_UP ) ? 0.0f : -plt->travel;

        was = br->ofs.z;
        if ( br->ofs.z < goal ) {
            step_z = plt->speed * dt;
            br->ofs.z += step_z;
            if ( br->ofs.z > goal ) br->ofs.z = goal;
        } else if ( br->ofs.z > goal ) {
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
            if ( !plt->moving ) ent_brush_sound( player, world, plt->model, snd );
            plt->moving = (short) ( br->ofs.z != goal );
            if ( !plt->moving ) ent_brush_sound( player, world, plt->model, (short) ( snd + 1 ) );
        }

        /* Carry the rider. Only upward: a descending plat drops out
           from under the player and gravity does the rest, which is
           what it looks like in Quake. Pushing them down would shove
           them through the floor of the shaft on the last step of the
           descent. */
        if ( riding && moved > 0.0f ) {
            player->pos.z += moved;
        }
    }
}

/* the player standing on a mover's brush where it is now */
static short ent_mover_ridden( Player *player, World *world, PlatEnt far *p )
{
    BrushModel far *b = &world->brush[ p->model ];
    float top;

    if ( player->pos.x + 16.0f < p->mins.x + b->ofs.x ) return 0;
    if ( player->pos.x - 16.0f > p->maxs.x + b->ofs.x ) return 0;
    if ( player->pos.y + 16.0f < p->mins.y + b->ofs.y ) return 0;
    if ( player->pos.y - 16.0f > p->maxs.y + b->ofs.y ) return 0;
    top = p->maxs.z + b->ofs.z;
    if ( player->pos.z - PL_FEET < top - 8.0f  ) return 0;
    if ( player->pos.z - PL_FEET > top + 64.0f ) return 0;
    return 1;
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
    p->state = p->targeted ? ENT_TRAIN_IDLE : ENT_TRAIN_WAIT;
}

void ent_move_trains( World *world, Player *player, float dt )
{
    short i, riding;
    PlatEnt far *p;
    BrushModel far *b;
    BspVec3 goal, was;

    for ( i = 0; i < world->plat_count; i++ ) {
        p = &world->plat[i];
        if ( p->kind != ENT_PLAT_KIND_TRAIN ) continue;
        if ( p->state == ENT_TRAIN_IDLE ) continue;
        if ( p->first < 0 || p->first >= world->corner_count ) continue;

        if ( p->state == ENT_TRAIN_WAIT ) {
            p->wait_left -= dt;
            if ( p->wait_left > 0.0f ) continue;
            if ( world->corner[ p->corner ].nxt < 0 ) continue;
            p->corner = world->corner[ p->corner ].nxt;
            p->state = ENT_TRAIN_MOVE;
            /* train_next: noise1, the move, from every corner */
            if ( p->snd ) ent_brush_sound( player, world, p->model, SND_TRAIN + 1 );
        }

        b = &world->brush[ p->model ];
        goal.x = world->corner[ p->corner ].org.x - p->mins.x;
        goal.y = world->corner[ p->corner ].org.y - p->mins.y;
        goal.z = world->corner[ p->corner ].org.z - p->mins.z;

        riding = ent_mover_ridden( player, world, p );
        was = b->ofs;

        if ( ent_step_to( &b->ofs, &goal, p->speed * dt ) ) {
            p->state = ENT_TRAIN_WAIT;
            p->wait_left = world->corner[ p->corner ].wait;
            /* train_wait: noise, the stop, only at a corner that waits */
            if ( p->snd && p->wait_left != 0.0f ) ent_brush_sound( player, world, p->model, SND_TRAIN );
            /* id's -1 is a wait of nothing, not a wait for ever */
            if ( p->wait_left <= 0.0f ) p->wait_left = 0.1f;
        }

        if ( b->ofs.x != was.x || b->ofs.y != was.y || b->ofs.z != was.z )
            b->node = ENT_NODE_DIRTY;

        if ( !riding ) continue;
        player->pos.x += b->ofs.x - was.x;
        player->pos.y += b->ofs.y - was.y;
        /* carried up, never pushed down: a descending mover is left to
           drop away under the player, as Quake does */
        if ( b->ofs.z > was.z ) player->pos.z += b->ofs.z - was.z;
    }
}
