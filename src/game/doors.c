/*
 * doors.c -- doors and secret doors, one tick at a time.
 *
 * Ported from ent.bas's door half and ent_door.c's two sweeps.
 * The BASIC split existed because a touch scan and a state machine over
 * descriptor-addressed arrays cost 0.408ms a tick on e1m6's 47 doors;
 * here World's arrays are far pointers already, so the sweeps are the
 * loops and there is no list to hand back.
 *
 * The ORDER the BASIC ran them in is kept, and it is not incidental:
 * every touch first -- which opens doors, and so changes state -- and
 * only then every step.
 */

#include "doors.h"
#include "triggers.h"
#include "ent.h"
#include "mathlib.h"
#include "mover.h"
#include "snd.h"
#include "qgl.h"

void ent_link_doors( World *world )
{
    short i, j, k, was, touch;
    DoorEnt far *d = world->door;

    for ( i = 0; i < world->door_count; i++ ) {
        for ( j = i + 1; j < world->door_count; j++ ) {
            touch = ( !d[i].secret && !d[j].secret &&
                      d[i].action.nolink == 0 && d[j].action.nolink == 0 );
            /* The fields are the brushes GROWN by the margin, so shrink
               them back before asking whether the brushes touch. */
            if ( d[i].mins.x + ENT_DOOR_FIELD  > d[j].maxs.x - ENT_DOOR_FIELD  ) touch = 0;
            if ( d[i].mins.y + ENT_DOOR_FIELD  > d[j].maxs.y - ENT_DOOR_FIELD  ) touch = 0;
            if ( d[i].mins.z + ENT_DOOR_FIELDZ > d[j].maxs.z - ENT_DOOR_FIELDZ ) touch = 0;
            if ( d[i].maxs.x - ENT_DOOR_FIELD  < d[j].mins.x + ENT_DOOR_FIELD  ) touch = 0;
            if ( d[i].maxs.y - ENT_DOOR_FIELD  < d[j].mins.y + ENT_DOOR_FIELD  ) touch = 0;
            if ( d[i].maxs.z - ENT_DOOR_FIELDZ < d[j].mins.z + ENT_DOOR_FIELDZ ) touch = 0;
            if ( !touch ) continue;

            was = d[j].link;
            for ( k = 0; k < world->door_count; k++ )
                if ( d[k].link == was ) d[k].link = d[i].link;
        }
    }
}

/* leg 0: the stop (a secret door's noise3), 1: a move (noise2), 2: a
   secret door leaving home (noise1). snd 0 is a silent door. The move
   loops until the stop takes its channel. */
static void ent_door_sound( Player *player, World *world, DoorEnt far *d, short leg )
{
    short id;

    if ( d->snd <= 0 ) return;
    id = d->secret ? (short) ( SND_SECRET1 + ( d->snd - 1 ) * 3 + ( 2 - leg ) )
                   : (short) ( SND_DOOR + ( d->snd - 1 ) * 2 + leg );
    ent_brush_sound( player, world, d->model, id );
}

/* door_go_up for a whole linked group: a shut or closing door sets out,
   an open one restarts its hold. */
void ent_door_fire( World *world, Player *player, Fight *fight,
                    Renderer *rdr, short grp )
{
    short k;
    DoorEnt far *d = world->door;

    for ( k = 0; k < world->door_count; k++ ) {
        if ( d[k].link != grp ) continue;

        if ( d[k].secret ) {
            /* fd_secret_use: nothing while it is anywhere but home. Its
               noise1 and then noise2 go out on one channel in one frame,
               so noise2 is what plays. */
            if ( d[k].state == ENT_DOOR_SHUT ) {
                d[k].state = ENT_DOOR_OUT1;
                d[k].msg = 0;
                ent_use_targets( world, player, fight, rdr, d[k].action.target );
                ent_door_sound( player, world, &d[k], 1 );
            }
            continue;
        }
        switch ( d[k].state ) {
        case ENT_DOOR_SHUT:
        case ENT_DOOR_CLOSING:
            d[k].state = ENT_DOOR_OPENING;
            ent_door_sound( player, world, &d[k], 1 );
            break;
        case ENT_DOOR_OPEN:
            d[k].hold_left = d[k].hold;
            break;
        }
    }
}

void ent_door_close( World *world, Player *player, short k )
{
    DoorEnt far *d;

    if ( k < 0 || k >= world->door_count ) return;
    d = &world->door[k];
    if ( d->state != ENT_DOOR_OPEN ) return;
    d->state = ENT_DOOR_CLOSING;
    ent_door_sound( player, world, d, 1 );
}

/* door_touch's key half: without the key the message, two seconds
   apart; with it the key is spent and the group goes. */
static void ent_door_key( World *world, Player *player, Fight *fight,
                           Renderer *rdr, short k )
{
    DoorEnt far *d = &world->door[k];
    long bit;
    short wt;

    if ( d->state != ENT_DOOR_SHUT ) return;
    bit = ( d->key == 2 ) ? PL_IT_KEY2 : PL_IT_KEY1;

    /* the worldtype's key sounds: medieval's two, then the rune pair */
    wt = (short) ( fight->worldtype > 1 ? 1 : fight->worldtype );

    if ( ( fight->items & bit ) == 0 ) {
        if ( rdr->anim_time < d->say_at ) return;
        d->say_at = rdr->anim_time + 2.0f;
        ent_say( fight, rdr, ent_msg( world, d->msg ) );
        snd_play( &world->snd, player, (short) ( SND_KEYTRY + wt * 2 ), &player->pos );
        return;
    }
    fight->items &= ~bit;
    snd_play( &world->snd, player, (short) ( SND_KEYTRY + 1 + wt * 2 ), &player->pos );
    ent_door_fire( world, player, fight, rdr, d->link );
}

void ent_doors_touch( World *world, Player *player, Fight *fight,
                      Renderer *rdr, PlayerTouches far *touches )
{
    short i, k;

    for ( i = 0; fight->health > 0 && i < touches->count; i++ ) {
        for ( k = 0; k < world->door_count; k++ ) {
            DoorEnt far *d = &world->door[k];

            if ( d->model != touches->model[i] ) continue;
            if ( d->key ) {
                ent_door_key( world, player, fight, rdr, k );
            } else if ( ( d->targeted || d->secret ) &&
                        rdr->anim_time >= d->say_at ) {
                d->say_at = rdr->anim_time + 2.0f;
                if ( d->msg )
                    ent_talk( player, &world->snd, fight, rdr,
                              ent_msg( world, d->msg ) );
            }
            break;
        }
    }
    touches->count = 0;
}

void ent_move_doors( World *world, Player *player, Fight *fight,
                      Renderer *rdr, PlayerTouches far *touches,
                      float dt, short rider_kind )
{
    short k, arrived, moving, riding, head;
    DoorEnt far *d;
    BrushModel far *b;
    Submodel far *model;
    BspVec3 was;

    /* Impacts happen in trace order. Automatic doors have a separate
       linked trigger volume, matching door_trigger_touch. */
    ent_doors_touch( world, player, fight, rdr, touches );
    for ( k = 0; fight->health > 0 && k < world->door_count; k++ ) {
        d = &world->door[k];
        if ( !d->targeted && !d->secret && !d->key &&
             ent_box_touched( player, &d->mins, &d->maxs, 0.0f ) ) {
            DoorEnt far *master = &world->door[d->link];
            if ( rdr->anim_time >= master->wait.touch_at ) {
                master->wait.touch_at = rdr->anim_time + 1.0f;
                ent_door_fire( world, player, fight, rdr, d->link );
            }
        }
    }

    for ( k = 0; k < world->door_count; k++ ) {
        short kind;

        d = &world->door[k];
        b = &world->brush[ d->model ];
        model = &world->models[ d->model ];
        was = b->ofs;
        kind = rider_kind;
        if ( kind == ENT_RIDER_LIVE && fight->health <= 0 ) kind = ENT_RIDER_DEAD;
        head = (short) ( kind == ENT_RIDER_HEAD );
        moving = (short) ( d->state == ENT_DOOR_OPENING ||
                           d->state == ENT_DOOR_CLOSING ||
                           d->state == ENT_DOOR_OUT1 ||
                           d->state == ENT_DOOR_BACK2 );
        riding = moving ? ent_mover_supported( player, &model->mins,
                                               &model->maxs, b, head ) : 0;
        arrived = 0;

        switch ( d->state ) {
        case ENT_DOOR_OPENING:
            arrived = ent_step_to( &b->ofs, &d->ofs_open, d->speed * dt );
            break;

        case ENT_DOOR_OPEN:
            if ( d->hold >= 0.0f ) {
                d->hold_left -= dt;
                if ( d->hold_left <= 0.0f ) {
                    d->state = ENT_DOOR_CLOSING;
                    ent_door_sound( player, world, d, 1 );
                }
            }
            break;

        case ENT_DOOR_CLOSING:
            arrived = ent_step_to( &b->ofs,
                                   d->secret ? &d->ofs_mid : &d->ofs_shut,
                                   d->speed * dt );
            break;

        case ENT_DOOR_OUT1:
            arrived = ent_step_to( &b->ofs, &d->ofs_mid, d->speed * dt );
            break;

        case ENT_DOOR_PAUSE_OUT:
            d->wait.pause_left -= dt;
            if ( d->wait.pause_left <= 0.0f ) {
                d->state = ENT_DOOR_OPENING;
                ent_door_sound( player, world, d, 1 );
            }
            break;

        case ENT_DOOR_PAUSE_BACK:
            d->wait.pause_left -= dt;
            if ( d->wait.pause_left <= 0.0f ) {
                d->state = ENT_DOOR_BACK2;
                ent_door_sound( player, world, d, 1 );
            }
            break;

        case ENT_DOOR_BACK2:
            arrived = ent_step_to( &b->ofs, &d->ofs_shut, d->speed * dt );
            break;
        }

        if ( !moving ) continue;
        if ( b->ofs.x != was.x || b->ofs.y != was.y || b->ofs.z != was.z ) {
            short pushed = ent_mover_push( world, player, &model->mins,
                                           &model->maxs, b, &was, riding,
                                           kind, touches );
            ent_doors_touch( world, player, fight, rdr, touches );
            if ( !pushed ) {
                b->ofs = was;
                if ( kind == ENT_RIDER_LIVE ) {
                    if ( d->secret ) {
                        if ( rdr->anim_time >= d->say_at ) {
                            d->say_at = rdr->anim_time + 0.5f;
                            ent_mover_damage( world, player, fight, rdr,
                                              &model->mins, &model->maxs,
                                              b, d->dmg );
                        }
                    } else {
                        ent_mover_damage( world, player, fight, rdr,
                                          &model->mins, &model->maxs,
                                          b, d->dmg );
                        if ( d->hold >= 0.0f ) {
                            d->state = d->state == ENT_DOOR_CLOSING
                                     ? ENT_DOOR_OPENING : ENT_DOOR_CLOSING;
                            ent_door_sound( player, world, d, 1 );
                        }
                    }
                }
                continue;
            }
            b->node = ENT_NODE_DIRTY;
        }
        if ( !arrived ) continue;

        switch ( d->state ) {
        case ENT_DOOR_OPENING:
            d->state = ENT_DOOR_OPEN;
            d->hold_left = d->hold;
            ent_door_sound( player, world, d, 0 );
            break;
        case ENT_DOOR_CLOSING:
            d->state = d->secret ? ENT_DOOR_PAUSE_BACK : ENT_DOOR_SHUT;
            if ( d->secret ) d->wait.pause_left = ENT_DOOR_PAUSE;
            ent_door_sound( player, world, d, 0 );
            break;
        case ENT_DOOR_OUT1:
            d->state = ENT_DOOR_PAUSE_OUT;
            d->wait.pause_left = ENT_DOOR_PAUSE;
            ent_door_sound( player, world, d, 0 );
            break;
        case ENT_DOOR_BACK2:
            d->state = ENT_DOOR_SHUT;
            ent_door_sound( player, world, d, 0 );
            break;
        }
    }
}
