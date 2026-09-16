/*
 * triggers.c -- triggers, buttons, relays, spike shooters, fireballs and
 * Chthon's lightning, one tick at a time.
 */

#include <string.h>

#include "triggers.h"
#include "doors.h"
#include "lights.h"
#include "ent.h"
#include "mathlib.h"
#include "client.h"
#include "snd.h"
#include "qgl.h"
#include "mdl_ai.h"

void ent_say( Fight *fight, Renderer *rdr, char far *msg )
{
    short i, last = -1;

    if ( !msg ) return;
    for ( i = 0; i < ENT_MSG_LEN; i++ )
        if ( msg[i] != ' ' && msg[i] != '\0' ) last = i;
    if ( last < 0 ) return;             /* blank is not a message */

    for ( i = 0; i <= last; i++ ) fight->msg[i] = msg[i];
    fight->msg[last + 1] = '\0';
    fight->msg_until = rdr->anim_time + ENT_MSG_TIME;
}

/* SUB_UseTargets and door_touch: the message, and misc/talk with it */
void ent_talk( Player *player, Sound *snd, Fight *fight, Renderer *rdr, char far *msg )
{
    if ( !msg || !msg[0] ) return;
    ent_say( fight, rdr, msg );
    snd_self( snd, player, CHAN_VOICE, SND_TALK );
}

/* SUB_UseTargets' remove(): every trigger named id is gone until the
   level restarts. A door or a monster by that name stays. */
static void ent_kill_targets( World *world, short id )
{
    short k;

    if ( !id ) return;
    for ( k = 0; k < world->trig_count; k++ )
        if ( world->trig[k].name == id ) world->trig[k].state = ENT_TRIG_DONE;
}

/* lightning_use: both electrode doors at one end of their travel, then a
   point off an awake Chthon while the first is up. id's STATE_TOP is a
   door OPEN here. wait_left is lightning_end. */
static void ent_lightning( World *world, Player *player, Renderer *rdr, short k )
{
    TrigEnt far *t = &world->trig[k];
    TrigEnt far *boss;
    short a = (short) t->ofs_out.x, b = (short) t->ofs_out.y, s1, s2, j;
    float now = rdr->anim_time;
    BspVec3 org;

    if ( t->wait_left >= now + 1.0f ) return;
    if ( a < 0 || b < 0 || a >= world->door_count || b >= world->door_count ) return;
    s1 = world->door[a].state;
    s2 = world->door[b].state;
    if ( ( s1 != ENT_DOOR_OPEN && s1 != ENT_DOOR_SHUT ) ||
         ( s2 != ENT_DOOR_OPEN && s2 != ENT_DOOR_SHUT ) || s1 != s2 ) return;
    t->wait_left = now + 1.0f;
    org = t->mins;
    snd_start( &world->snd, player, (short) ( SND_ENT_TRIG + k ), CHAN_VOICE, SND_POWER, &org, ATTN_NORM );

    for ( j = 0; j < world->trig_count; j++ ) {
        boss = &world->trig[j];
        if ( boss->kind != ENT_TRIG_BOSS ) continue;
        if ( s1 != ENT_DOOR_OPEN || boss->state != ENT_TRIG_ARMED || boss->left <= 0 ) return;
        org = boss->mins;
        snd_start( &world->snd, player, (short) ( SND_ENT_TRIG + j ), CHAN_VOICE, SND_BOSS_PAIN, &org, ATTN_NORM );
        boss->left--;
        if ( boss->left > 0 ) return;
        boss->state = ENT_TRIG_DONE;
        boss->delay_left = ENT_BOSS_DYING;
        return;
    }
}

void ent_use_targets( World *world, Player *player, Fight *fight,
                       Renderer *rdr, short id )
{
    short k;
    BspVec3 org;

    if ( !id ) return;

    for ( k = 0; k < world->door_count; k++ )
        if ( world->door[k].targeted == id ) ent_door_fire( world, player, world->door[k].link );

    /* train_use: once */
    for ( k = 0; k < world->plat_count; k++ )
        if ( world->plat[k].kind == ENT_PLAT_KIND_TRAIN &&
             world->plat[k].targeted == id &&
             world->plat[k].state == ENT_TRAIN_IDLE )
            world->plat[k].state = ENT_TRAIN_WAIT;

    ent_lights_use( world->light, world->light_count, &world->light_clock, id );

    for ( k = 0; k < world->tele_count; k++ )
        if ( world->tele[k].name == id ) world->tele[k].until = rdr->anim_time + ENT_FOG_DELAY;

    for ( k = 0; k < world->trig_count; k++ ) {
        if ( world->trig[k].name != id ) continue;

        switch ( world->trig[k].kind ) {
        case ENT_TRIG_COUNTER:
            if ( world->trig[k].state != ENT_TRIG_DONE ) {
                world->trig[k].left--;
                if ( world->trig[k].left <= 0 ) ent_trig_fire( world, player, fight, rdr, k );
            }
            break;
        case ENT_TRIG_ONCE:
        case ENT_TRIG_MULTI:
            if ( world->trig[k].state == ENT_TRIG_READY ) ent_trig_fire( world, player, fight, rdr, k );
            break;
        case ENT_TRIG_RELAY:
            ent_trig_fire( world, player, fight, rdr, k );
            break;
        case ENT_TRIG_BOLT:
            ent_lightning( world, player, rdr, k );
            break;
        case ENT_TRIG_BOSS:
            /* boss_awake's rise: out1 and sight1 from where he stands */
            if ( world->trig[k].state == ENT_TRIG_READY ) {
                org = world->trig[k].mins;
                snd_start( &world->snd, player, (short) ( SND_ENT_TRIG + k ), CHAN_WEAPON, SND_BOSS_OUT, &org, ATTN_NORM );
                snd_start( &world->snd, player, (short) ( SND_ENT_TRIG + k ), CHAN_VOICE, SND_BOSS_SIGHT, &org, ATTN_NORM );
            }
            /* fall through */
        case ENT_TRIG_SHOOTER:
            if ( world->trig[k].state == ENT_TRIG_READY ) world->trig[k].state = ENT_TRIG_ARMED;
            break;
        }
    }
}

/* multi_trigger: the message, then the targets; a once and a counter
   are done, a multiple re-arms after wait. */
void ent_trig_fire( World *world, Player *player, Fight *fight,
                     Renderer *rdr, short k )
{
    TrigEnt far *t = &world->trig[k];
    BspVec3 c;
    static short noise[3] = { SND_SECRET, SND_TALK, SND_TRIGGER };

    if ( t->kind == ENT_TRIG_SECRET ) fight->secrets++;
    /* multi_trigger's noise from the volume's middle; the message's
       talk only when there is none */
    if ( t->snd >= 1 && t->snd <= 3 ) {
        c.x = ( t->mins.x + t->maxs.x ) * 0.5f;
        c.y = ( t->mins.y + t->maxs.y ) * 0.5f;
        c.z = ( t->mins.z + t->maxs.z ) * 0.5f;
        snd_start( &world->snd, player, (short) ( SND_ENT_BRUSH + t->model ), CHAN_VOICE,
                   noise[t->snd - 1], &c, ATTN_NORM );
        ent_say( fight, rdr, ent_msg( world, t->msg ) );
    } else {
        ent_talk( player, &world->snd, fight, rdr, ent_msg( world, t->msg ) );
    }

    if ( t->kind == ENT_TRIG_COUNTER || t->wait < 0.0f ) {
        t->state = ENT_TRIG_DONE;
    } else {
        t->state = ENT_TRIG_HELD;
        t->wait_left = t->wait;
    }

    /* SUB_UseTargets' delay: the kill and the fire wait in ent_move_trigs */
    if ( t->delay > 0.0f ) {
        t->delay_left = t->delay;
        return;
    }
    ent_kill_targets( world, t->kill );
    ent_use_targets( world, player, fight, rdr, t->target );
}

static short ent_shot_button( TrigEnt far *t )
{
    if ( t->kind != ENT_TRIG_SHOOT_BUTTON ) return 0;
    t->state = ENT_TRIG_GOING;
    return -1;
}

short ent_trig_selftest( void )
{
    TrigEnt t;
    t.kind = ENT_TRIG_SHOOT_BUTTON;
    t.state = ENT_TRIG_READY;
    return (short) ( ent_shot_button( &t ) && t.state == ENT_TRIG_GOING );
}

void ent_trig_shot( World *world, Player *player, Fight *fight,
                    Renderer *rdr, short k )
{
    TrigEnt far *t = &world->trig[k];

    if ( t->state != ENT_TRIG_READY ) return;
    if ( !ent_shot_button( t ) ) ent_trig_fire( world, player, fight, rdr, k );
}

void ent_move_trigs( World *world, Player *player, Camera *cam, Fight *fight,
                      Renderer *rdr, float dt, short scr_x_res, short scr_y_res )
{
    short k;
    TrigEnt far *t;
    BrushModel far *b;
    BspVec3 was, home, at;
    float before;

    home.x = home.y = home.z = 0.0f;

    for ( k = 0; k < world->trig_count; k++ ) {
        t = &world->trig[k];
        b = &world->brush[ t->model ];
        was = b->ofs;
        before = t->delay_left;

        if ( t->delay_left > 0.0f ) {
            t->delay_left -= dt;
            if ( t->delay_left <= 0.0f ) {
                ent_kill_targets( world, t->kill );
                ent_use_targets( world, player, fight, rdr, t->target );
            }
        }

        /* Chthon's end: boss_death1's death.wav after shockc, death9's
           out1, death10's kill with the targets above */
        if ( t->kind == ENT_TRIG_BOSS && t->state == ENT_TRIG_DONE && before > 0.0f ) {
            at = t->mins;
            if ( before > ENT_BOSS_DYING - 1.0f && t->delay_left <= ENT_BOSS_DYING - 1.0f )
                snd_start( &world->snd, player, (short) ( SND_ENT_TRIG + k ), CHAN_VOICE, SND_BOSS_DEATH, &at, ATTN_NORM );
            if ( before > 0.1f && t->delay_left <= 0.1f )
                snd_start( &world->snd, player, (short) ( SND_ENT_TRIG + k ), CHAN_BODY, SND_BOSS_OUT, &at, ATTN_NORM );
            if ( t->delay_left <= 0.0f ) fight->kills++;
        }

        switch ( t->kind ) {
        case ENT_TRIG_BUTTON:
        case ENT_TRIG_SHOOT_BUTTON:
            switch ( t->state ) {
            case ENT_TRIG_READY:
                if ( t->kind == ENT_TRIG_BUTTON &&
                     ent_box_touched( player, &t->mins, &t->maxs, ENT_TOUCH_SLACK ) )
                    t->state = ENT_TRIG_GOING;
                break;
            case ENT_TRIG_GOING:
                if ( ent_step_to( &b->ofs, &t->ofs_out, t->speed * dt ) ) {
                    t->state = ENT_TRIG_HELD;
                    t->wait_left = t->wait;
                    at = t->mins;
                    snd_play( &world->snd, player, (short) ( SND_BUTTON + t->snd ), &at );
                    ent_say( fight, rdr, ent_msg( world, t->msg ) );
                    ent_use_targets( world, player, fight, rdr, t->target );
                }
                break;
            case ENT_TRIG_HELD:
                if ( t->wait >= 0.0f ) {
                    t->wait_left -= dt;
                    if ( t->wait_left <= 0.0f ) t->state = ENT_TRIG_BACK;
                }
                break;
            case ENT_TRIG_BACK:
                /* button_blocked without the push: wait for the player
                   to step off rather than move the brush through them. */
                if ( !ent_box_touched( player, &t->mins, &t->maxs, 0.0f ) ) {
                    if ( ent_step_to( &b->ofs, &home, t->speed * dt ) ) t->state = ENT_TRIG_READY;
                }
                break;
            }
            break;

        case ENT_TRIG_EXIT:
            /* the level's end: the tally, with the map's own title
               held up until the next map takes over -- an hour, not
               ENT_MSG_TIME, since nothing else is going to be said */
            if ( t->state == ENT_TRIG_READY &&
                 ent_box_touched( player, &t->mins, &t->maxs, 0.0f ) ) {
                ent_say( fight, rdr, ent_msg( world, t->msg ) );
                /* changelevel_touch: nextmap = self.map */
                if ( ent_msg( world, t->count ) ) ent_copy_map( fight->next_map, ent_msg( world, t->count ) );
                fight->msg_until  = rdr->anim_time + 3600.0f;
                fight->state      = GS_EXIT;
                fight->exit_time  = rdr->anim_time;
                pl_intermission( player, cam, fight, scr_x_res, scr_y_res );
                t->state = ENT_TRIG_DONE;
            }
            break;

        case ENT_TRIG_ONCE:
        case ENT_TRIG_MULTI:
        case ENT_TRIG_SHOOT:
        case ENT_TRIG_SECRET:
            switch ( t->state ) {
            case ENT_TRIG_READY:
                /* A shootable trigger is fired by a pellet, never by
                   a touch -- pl_shot_touch does that. */
                if ( t->kind != ENT_TRIG_SHOOT &&
                     ent_box_touched( player, &t->mins, &t->maxs, 0.0f ) )
                    ent_trig_fire( world, player, fight, rdr, k );
                break;
            case ENT_TRIG_HELD:
                t->wait_left -= dt;
                if ( t->wait_left <= 0.0f ) t->state = ENT_TRIG_READY;
                break;
            }
            break;

        case ENT_TRIG_SKILL:
            /* trigger_skill_touch: every touch, and the next map loads by it */
            if ( ent_box_touched( player, &t->mins, &t->maxs, 0.0f ) ) fight->skill = t->count;
            break;

        case ENT_TRIG_FIREBALL:
            /* fire_fly: a ball, and the next in 3..8 s */
            t->wait_left -= dt;
            if ( t->wait_left <= 0.0f ) {
                t->state = ENT_TRIG_ARMED;
                t->wait_left = rng_float( &world->rng ) * 5.0f + 3.0f;
            }
            break;
        }

        if ( b->ofs.x != was.x || b->ofs.y != was.y || b->ofs.z != was.z )
            b->node = ENT_NODE_DIRTY;
    }
}

void ent_fireball_spawn( World *world, TrigEnt far *t )
{
    t->wait_left = rng_float( &world->rng ) * 5.0f;
}

/* setsize: the ogre, demon and shambler 32 wide and 64 up, the dog 32
   and 40, the rest monster_army's 16 and 40 */
static float near ent_mon_half( short kind )
{
    return kind == MDL_KIND_OGRE || kind == MDL_KIND_DEMON || kind == MDL_KIND_SHAMBLER ||
           kind == MDL_KIND_DOG ? 32.0f : MDL_HALF;
}

static float near ent_mon_zhi( short kind )
{
    return kind == MDL_KIND_OGRE || kind == MDL_KIND_DEMON || kind == MDL_KIND_SHAMBLER ? 64.0f : MDL_ZHI;
}

void ent_monster_jump( World *world, MdlEnt far *e )
{
    short k;
    float half, zhi;

    /* flags & (FL_MONSTER|FL_FLY|FL_SWIM) != FL_MONSTER: qcc binds & tighter
       than !=, so the flyer is left alone */
    if ( mon_facts[ e->kind ].flyer ) return;
    half = ent_mon_half( e->kind ) + 1.0f;
    zhi  = ent_mon_zhi( e->kind ) + 1.0f;
    for ( k = 0; k < world->trig_count; k++ ) {
        TrigEnt far *t = &world->trig[k];

        if ( t->kind != ENT_TRIG_MJUMP ) continue;
        /* SV_TouchLinks: the boxes each grown by 1 */
        if ( e->pos.x + half < t->mins.x || e->pos.x - half > t->maxs.x ||
             e->pos.y + half < t->mins.y || e->pos.y - half > t->maxs.y ||
             e->pos.z + zhi  < t->mins.z || e->pos.z + MDL_ZLO - 1.0f > t->maxs.z ) continue;
        e->vel.x = t->ofs_out.x;
        e->vel.y = t->ofs_out.y;
        if ( e->state == MDL_ST_LEAP ) continue;   /* off the ground: xy alone */
        e->vel.z = t->ofs_out.z;
        e->state = MDL_ST_LEAP;
        e->leapt = MDL_THROWN;
        e->anim_frame = 0;
    }
}

short ent_fireballs_sent( World *world )
{
    short k, n = 0;

    for ( k = 0; k < world->trig_count; k++ )
        if ( world->trig[k].kind == ENT_TRIG_FIREBALL ) n += world->trig[k].left;
    return n;
}
