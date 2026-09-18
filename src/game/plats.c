/*
 * plats.c -- func_plat and func_train, which share the plat array.
 */

#include "plats.h"
#include "ent.h"
#include "mathlib.h"
#include "pl_move.h"
#include "pl_trace.h"
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
static short near ent_plat_activated( Player *player, PlatEnt far *plt,
                                      BrushModel far *br )
{
    float top;

    if ( !ent_mover_xy( player, plt, br ) ) return 0;

    /* Above its surface and within a body's height of it. Anything
       higher is someone on a walkway over the shaft, not a passenger. */
    top = plt->maxs.z + br->ofs.z;

    if ( player->pos.z - PL_FEET < top - 8.0f  ) return 0;
    if ( player->pos.z - PL_FEET > top + 64.0f ) return 0;

    return 1;
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
    return ent_plat_activated( player, plt, &world->brush[plt->model] );
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
        short activated = ent_plat_activated( player, plt, br );
        short riding = ent_mover_supported( player, plt, br, head );
        float goal, step_z, moved, was;

        if ( plt->kind != ENT_PLAT_KIND_PLAT ) continue;   /* a train is ent_move_trains' */

        plt->state = rider_kind == ENT_RIDER_LIVE && activated ? ENT_PLAT_UP : ENT_PLAT_DOWN;
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
    p->state = p->targeted ? ENT_TRAIN_IDLE : ENT_TRAIN_WAIT;
}

void ent_move_trains( World *world, Player *player, float dt, short rider_kind )
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

        riding = ent_mover_supported( player, p, b,
                                      (short) ( rider_kind == ENT_RIDER_HEAD ) );
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

        if ( !riding && ( b->ofs.x != was.x || b->ofs.y != was.y ||
                           b->ofs.z != was.z ) )
            riding = ent_mover_intersects( world, player, p, b,
                                           (short) ( rider_kind == ENT_RIDER_HEAD ) );
        if ( !riding || ( b->ofs.x == was.x && b->ofs.y == was.y &&
                           b->ofs.z == was.z ) ) continue;
        player->pos.x += b->ofs.x - was.x;
        player->pos.y += b->ofs.y - was.y;
        /* Walking riders drop from a descending mover; TOSS/BOUNCE
           corpses receive the pusher's complete displacement. */
        if ( b->ofs.z > was.z || rider_kind != ENT_RIDER_LIVE )
            player->pos.z += b->ofs.z - was.z;
        if ( rider_kind != ENT_RIDER_LIVE ) player->on_ground = 0;
    }
}
