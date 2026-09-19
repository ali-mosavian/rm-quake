/*
 * plats.c -- func_plat and func_train, which share the plat array.
 */

#include "plats.h"
#include "ent.h"
#include "mathlib.h"
#include "pl_move.h"
#include "snd.h"
#include "doors.h"

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
        short riding = ent_mover_supported( player, &plt->mins, &plt->maxs,
                                             br, head );
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
            riding = ent_mover_intersects( world, player, &plt->mins,
                                            &plt->maxs, br, head );

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

void ent_move_trains( World *world, Player *player, Fight *fight,
                      Renderer *rdr, PlayerTouches far *touches,
                      float dt, short rider_kind )
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

        riding = ent_mover_supported( player, &p->mins, &p->maxs, b,
                                      (short) ( kind == ENT_RIDER_HEAD ) );
        was = b->ofs;

        arrived = ent_step_to( &b->ofs, &goal, p->speed * dt );

        if ( b->ofs.x != was.x || b->ofs.y != was.y || b->ofs.z != was.z ) {
            short pushed = ent_mover_push( world, player, &p->mins,
                                           &p->maxs, b, &was, riding, kind,
                                           touches );
            ent_doors_touch( world, player, fight, rdr, touches );
            if ( !pushed ) {
                b->ofs = was;
                if ( kind == ENT_RIDER_LIVE &&
                     rdr->anim_time >= p->time.block_at ) {
                    p->time.block_at = rdr->anim_time + 0.5f;
                    ent_mover_damage( world, player, fight, rdr, &p->mins,
                                      &p->maxs, b, p->mode.dmg );
                }
                continue;
            }
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
