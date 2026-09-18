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
#include "snd.h"
#include "qgl.h"

void ent_link_doors( World *world )
{
    short i, j, k, was, touch;
    DoorEnt far *d = world->door;

    for ( i = 0; i < world->door_count; i++ ) {
        for ( j = i + 1; j < world->door_count; j++ ) {
            touch = ( d[i].nolink == 0 && d[j].nolink == 0 );
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
void ent_door_fire( World *world, Player *player, short grp )
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
    ent_door_fire( world, player, d->link );
}

void ent_move_doors( World *world, Player *player, Fight *fight,
                      Renderer *rdr, float dt )
{
    short k;
    DoorEnt far *d;
    BrushModel far *b;
    BspVec3 was;

    /* Every touch first, then every step -- opening a door changes
       state, and the BASIC's two sweeps ran in that order. */
    for ( k = 0; k < world->door_count; k++ ) {
        d = &world->door[k];
        if ( !ent_box_touched( player, &d->mins, &d->maxs, 0.0f ) ) continue;

        if ( d->key )
            ent_door_key( world, player, fight, rdr, k );
        else if ( !d->targeted && !d->secret )
            ent_door_fire( world, player, d->link );
        else
            ent_talk( player, &world->snd, fight, rdr, ent_msg( world, d->msg ) );
    }

    for ( k = 0; k < world->door_count; k++ ) {
        d = &world->door[k];
        b = &world->brush[ d->model ];
        was = b->ofs;

        switch ( d->state ) {
        case ENT_DOOR_OPENING:
            if ( ent_step_to( &b->ofs, &d->ofs_open, d->speed * dt ) ) {
                d->state = ENT_DOOR_OPEN;
                d->hold_left = d->hold;
                ent_door_sound( player, world, d, 0 );
            }
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
            if ( d->secret ) {
                if ( ent_step_to( &b->ofs, &d->ofs_mid, d->speed * dt ) ) {
                    d->state = ENT_DOOR_PAUSE_BACK;
                    d->pause_left = ENT_DOOR_PAUSE;
                    ent_door_sound( player, world, d, 0 );   /* fd_secret_move5 */
                }
            } else if ( ent_step_to( &b->ofs, &d->ofs_shut, d->speed * dt ) ) {
                d->state = ENT_DOOR_SHUT;
                ent_door_sound( player, world, d, 0 );
            }
            break;

        case ENT_DOOR_OUT1:
            if ( ent_step_to( &b->ofs, &d->ofs_mid, d->speed * dt ) ) {
                d->state = ENT_DOOR_PAUSE_OUT;
                d->pause_left = ENT_DOOR_PAUSE;
                ent_door_sound( player, world, d, 0 );       /* fd_secret_move1 */
            }
            break;

        case ENT_DOOR_PAUSE_OUT:
            d->pause_left -= dt;
            if ( d->pause_left <= 0.0f ) {
                d->state = ENT_DOOR_OPENING;
                ent_door_sound( player, world, d, 1 );
            }
            break;

        case ENT_DOOR_PAUSE_BACK:
            d->pause_left -= dt;
            if ( d->pause_left <= 0.0f ) {
                d->state = ENT_DOOR_BACK2;
                ent_door_sound( player, world, d, 1 );
            }
            break;

        case ENT_DOOR_BACK2:
            if ( ent_step_to( &b->ofs, &d->ofs_shut, d->speed * dt ) ) {
                d->state = ENT_DOOR_SHUT;
                ent_door_sound( player, world, d, 0 );
            }
            break;
        }

        if ( b->ofs.x != was.x || b->ofs.y != was.y || b->ofs.z != was.z )
            b->node = ENT_NODE_DIRTY;
    }
}
