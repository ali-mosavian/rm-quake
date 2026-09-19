#include <assert.h>
#include <string.h>

#include "doors.h"
#include "ent.h"
#include "mover.h"
#include "pl_trace.h"
#include "triggers.h"

static short target_calls;
static short target_id;
static short damage_calls;
static float damage_value;
static short trace_seq[8];
static short trace_model_seq[8];
static short trace_len, trace_pos;

void ent_use_targets( World *world, Player *player, Fight *fight,
                      Renderer *rdr, short id )
{
    (void) world; (void) player; (void) fight; (void) rdr;
    target_calls++;
    target_id = id;
}

short ent_box_touched( Player *player, BspVec3 *mins, BspVec3 *maxs, float slack )
{
    if ( player->pos.x + PL_HALF + slack < mins->x ) return 0;
    if ( player->pos.x - PL_HALF - slack > maxs->x ) return 0;
    if ( player->pos.y + PL_HALF + slack < mins->y ) return 0;
    if ( player->pos.y - PL_HALF - slack > maxs->y ) return 0;
    if ( player->pos.z + PL_ZHI  + slack < mins->z ) return 0;
    if ( player->pos.z + PL_ZLO  - slack > maxs->z ) return 0;
    return -1;
}

void pl_trace( World *world, BspVec3 *start, BspVec3 *fin, TraceResult *tr )
{
    short pos = trace_pos;
    (void) world; (void) start;
    memset( tr, 0, sizeof(*tr) );
    tr->frac = 1.0f;
    tr->hit_model = -1;
    if ( pos < trace_len ) {
        tr->all_solid = trace_seq[pos];
        tr->hit_model = trace_model_seq[pos];
        if ( tr->hit_model > 0 ) tr->frac = 0.5f;
        trace_pos++;
    }
    tr->end_pos = *fin;
}

void pl_trace_move( World *world, BspVec3 *start, BspVec3 *fin,
                    short ignore_item, TraceResult *tr, short *hit_item )
{
    (void) ignore_item; (void) hit_item;
    pl_trace( world, start, fin, tr );
}

void pl_trace_point( World *world, BspVec3 *start, BspVec3 *fin,
                     TraceResult *tr )
{
    pl_trace( world, start, fin, tr );
}

short r_point_leaf( BspVec3 *p, World *world )
{
    (void) p; (void) world;
    return 0;
}

short r_leaf_contents( short leaf_nr, World *world )
{
    (void) leaf_nr; (void) world;
    return CONTENTS_EMPTY;
}

short rng_int( Rng *rng )
{
    (void) rng;
    return 0;
}

unsigned char *pak_load( Pak *pak, char *member, long bytes )
{
    (void) pak; (void) member; (void) bytes;
    return 0;
}

void snd_self( Sound *snd, Player *player, short chan, short id )
{
    (void) snd; (void) player; (void) chan; (void) id;
}

void ent_brush_sound( Player *player, World *world, short model, short sound )
{
    (void) player; (void) world; (void) model; (void) sound;
}

void ent_talk( Player *player, Sound *snd, Fight *fight, Renderer *rdr,
               char *message )
{
    (void) player; (void) snd; (void) fight; (void) rdr; (void) message;
}

void ent_say( Fight *fight, Renderer *rdr, char *message )
{
    (void) fight; (void) rdr; (void) message;
}

char *ent_msg( World *world, short id )
{
    (void) world; (void) id;
    return 0;
}

void snd_play( Sound *snd, Player *player, short id, BspVec3 *org )
{
    (void) snd; (void) player; (void) id; (void) org;
}

void combat_damage_player( Player *player, World *world, Fight *fight,
                           Renderer *rdr, float damage )
{
    (void) player; (void) world; (void) rdr;
    damage_calls++;
    damage_value = damage;
    fight->health -= (short) damage;
}

int main( void )
{
    World world;
    Player player;
    Fight fight;
    Renderer rdr;
    Camera cam;
    DoorEnt door;
    DoorEnt pair[2];
    BrushModel brush[2];
    BrushModel pair_brush[3];
    Submodel model[2];
    Submodel pair_model[3];
    PlayerTouches touches;

    memset( &world, 0, sizeof(world) );
    memset( &player, 0, sizeof(player) );
    memset( &fight, 0, sizeof(fight) );
    memset( &rdr, 0, sizeof(rdr) );
    memset( &cam, 0, sizeof(cam) );
    memset( &door, 0, sizeof(door) );
    memset( brush, 0, sizeof(brush) );
    memset( model, 0, sizeof(model) );
    memset( &touches, 0, sizeof(touches) );
    world.door_count = 1;
    world.door = &door;
    world.brush = brush;
    world.models = model;
    world.model_count = 2;
    door.model = 1;
    door.targeted = 1;
    door.secret = 1;
    door.state = ENT_DOOR_SHUT;
    door.link = 0;
    door.action.target = 37;
    door.msg = 9;

    ent_door_fire( &world, &player, &fight, &rdr, 0 );
    assert( door.state == ENT_DOOR_OUT1 );
    assert( door.msg == 0 );
    assert( target_calls == 1 && target_id == 37 );

    ent_door_fire( &world, &player, &fight, &rdr, 0 );
    assert( target_calls == 1 );

    /* door_blocked: a closing door that cannot move the player restores
       its own move, deals its configured damage and heads back up. */
    memset( &door, 0, sizeof(door) );
    memset( brush, 0, sizeof(brush) );
    fight.health = 100;
    door.model = 1;
    door.targeted = 1;
    door.state = ENT_DOOR_CLOSING;
    door.speed = 10.0f;
    door.hold = 3.0f;
    door.dmg = 2;
    brush[1].ofs.x = 10.0f;
    brush[1].solid = 1;
    model[1].mins.x = model[1].mins.y = model[1].mins.z = -4.0f;
    model[1].maxs.x = model[1].maxs.y = model[1].maxs.z = 4.0f;
    door.ofs_shut.x = 0.0f;
    damage_calls = 0;
    trace_len = 3; trace_pos = 0;
    trace_seq[0] = 1; trace_seq[1] = 0; trace_seq[2] = 1;
    ent_move_doors( &world, &player, &fight, &rdr, &touches, 1.0f, ENT_RIDER_LIVE );
    assert( brush[1].ofs.x == 10.0f );
    assert( door.state == ENT_DOOR_OPENING );
    assert( damage_calls == 1 && damage_value == 2.0f && fight.health == 98 );
    assert( player.vel.x < 0.0f );

    /* A negative-wait door keeps crushing instead of reversing. */
    door.state = ENT_DOOR_CLOSING;
    door.hold = -1.0f;
    trace_pos = 0;
    ent_move_doors( &world, &player, &fight, &rdr, &touches, 1.0f, ENT_RIDER_LIVE );
    assert( brush[1].ofs.x == 10.0f && door.state == ENT_DOOR_CLOSING );
    assert( damage_calls == 2 && fight.health == 96 );

    /* door_trigger_touch is gated for a second, so a blocked automatic
       door gets a tick to retreat instead of being re-fired in place. */
    memset( &door, 0, sizeof(door) );
    memset( &player, 0, sizeof(player) );
    memset( brush, 0, sizeof(brush) );
    door.model = 1;
    door.state = ENT_DOOR_OPENING;
    door.speed = 10.0f;
    door.hold = 3.0f;
    door.dmg = 2;
    door.ofs_open.x = 20.0f;
    door.mins.x = door.mins.y = door.mins.z = -64.0f;
    door.maxs.x = door.maxs.y = door.maxs.z = 64.0f;
    brush[1].ofs.x = 10.0f;
    brush[1].solid = 1;
    fight.health = 100;
    damage_calls = 0;
    rdr.anim_time = 10.0f;
    trace_len = 3; trace_pos = 0;
    ent_move_doors( &world, &player, &fight, &rdr, &touches, 1.0f, ENT_RIDER_LIVE );
    assert( door.state == ENT_DOOR_CLOSING && brush[1].ofs.x == 10.0f );
    assert( door.wait.touch_at == 11.0f && damage_calls == 1 );
    trace_len = trace_pos = 0;
    rdr.anim_time = 10.1f;
    ent_move_doors( &world, &player, &fight, &rdr, &touches, 1.0f, ENT_RIDER_LIVE );
    assert( door.state == ENT_DOOR_SHUT && brush[1].ofs.x == 0.0f );
    assert( damage_calls == 1 );
    rdr.anim_time = 10.2f;
    ent_move_doors( &world, &player, &fight, &rdr, &touches, 1.0f, ENT_RIDER_LIVE );
    assert( door.state == ENT_DOOR_SHUT && brush[1].ofs.x == 0.0f );

    /* Proximity is not a brush touch. An actual impact with a moving
       secret door owns the same attack_finished clock as secret_blocked. */
    memset( &door, 0, sizeof(door) );
    memset( &player, 0, sizeof(player) );
    memset( brush, 0, sizeof(brush) );
    door.model = 1;
    door.secret = 1;
    door.state = ENT_DOOR_SHUT;
    door.dmg = 3;
    door.speed = 50.0f;
    brush[1].solid = 1;
    damage_calls = 0;
    rdr.anim_time = 2.0f;
    touches.count = 0;
    ent_move_doors( &world, &player, &fight, &rdr, &touches, 0.0f, ENT_RIDER_LIVE );
    assert( door.say_at == 0.0f );
    door.state = ENT_DOOR_CLOSING;
    brush[1].ofs.x = 5.0f;
    touches.model[0] = 1;
    touches.count = 1;
    ent_move_doors( &world, &player, &fight, &rdr, &touches, 0.0f, ENT_RIDER_LIVE );
    assert( door.say_at == 4.0f );
    touches.count = 0;
    trace_len = 3; trace_pos = 0;
    rdr.anim_time = 2.1f;
    ent_move_doors( &world, &player, &fight, &rdr, &touches, 1.0f / 60.0f, ENT_RIDER_LIVE );
    assert( damage_calls == 0 && door.state == ENT_DOOR_CLOSING );
    assert( brush[1].ofs.x == 5.0f && door.say_at == 4.0f );
    trace_pos = 0;
    rdr.anim_time = 4.0f;
    ent_move_doors( &world, &player, &fight, &rdr, &touches, 1.0f / 60.0f, ENT_RIDER_LIVE );
    assert( damage_calls == 1 && damage_value == 3.0f && door.say_at == 4.5f );

    /* Touches are dispatched in collision order, not door-array order:
       one silver key must open the first door actually struck. */
    memset( pair, 0, sizeof(pair) );
    world.door = pair;
    world.door_count = 2;
    pair[0].model = 1; pair[1].model = 2;
    pair[0].key = pair[1].key = 1;
    pair[0].link = 0; pair[1].link = 1;
    pair[0].state = pair[1].state = ENT_DOOR_SHUT;
    fight.health = 100;
    fight.items = PL_IT_KEY1;
    touches.model[0] = 2;
    touches.model[1] = 1;
    touches.count = 2;
    ent_doors_touch( &world, &player, &fight, &rdr, &touches );
    assert( pair[0].state == ENT_DOOR_SHUT );
    assert( pair[1].state == ENT_DOOR_OPENING );

    /* SV_PushEntity impacts are touches too: a door carrying the player
       into a moving secret door refreshes its shared cooldown first. */
    memset( pair, 0, sizeof(pair) );
    memset( pair_brush, 0, sizeof(pair_brush) );
    memset( pair_model, 0, sizeof(pair_model) );
    memset( &player, 0, sizeof(player) );
    world.door = pair;
    world.door_count = 2;
    world.brush = pair_brush;
    world.models = pair_model;
    world.model_count = 3;
    pair[0].model = 1;
    pair[0].targeted = 1;
    pair[0].state = ENT_DOOR_OPENING;
    pair[0].speed = 10.0f;
    pair[0].ofs_open.x = 10.0f;
    pair[1].model = 2;
    pair[1].secret = 1;
    pair[1].state = ENT_DOOR_SHUT;
    pair[1].msg = 9;
    pair_model[1].mins.x = pair_model[1].mins.y = -4.0f;
    pair_model[1].mins.z = -4.0f;
    pair_model[1].maxs.x = pair_model[1].maxs.y = 4.0f;
    pair_model[1].maxs.z = 0.0f;
    pair_brush[1].solid = pair_brush[2].solid = 1;
    player.pos.z = PL_FEET;
    player.on_ground = 1;
    touches.count = 0;
    trace_len = 2; trace_pos = 0;
    trace_seq[0] = trace_seq[1] = 0;
    trace_model_seq[0] = 2;
    trace_model_seq[1] = -1;
    rdr.anim_time = 7.0f;
    ent_move_doors( &world, &player, &fight, &rdr, &touches,
                    1.0f, ENT_RIDER_LIVE );
    assert( pair[1].say_at == 9.0f );

    /* The real movement path, not a proximity box or injected event,
       carries a slide impact through to a moving secret door touch. */
    memset( &door, 0, sizeof(door) );
    memset( &player, 0, sizeof(player) );
    memset( &fight, 0, sizeof(fight) );
    memset( &cam, 0, sizeof(cam) );
    world.door = &door;
    world.door_count = 1;
    door.model = 1;
    door.secret = 1;
    door.state = ENT_DOOR_CLOSING;
    door.msg = 9;
    fight.health = 100;
    fight.gravity = PL_FALLACC;
    rdr.anim_time = 11.0f;
    touches.count = 0;
    trace_len = 2; trace_pos = 0;
    trace_seq[0] = trace_seq[1] = 0;
    trace_model_seq[0] = -1;
    trace_model_seq[1] = 1;
    pl_move( &world, &player, &cam, &fight, &rdr, &touches,
             1.0f, 0.0f, 1.0f, 0.0f, 0, 1.0f / 60.0f );
    assert( touches.count == 1 && touches.model[0] == 1 );
    ent_doors_touch( &world, &player, &fight, &rdr, &touches );
    assert( door.say_at == 13.0f );

    /* A lethal first pusher changes later movers in the same tick to
       corpse transport; a second door must not damage the dead player. */
    memset( pair, 0, sizeof(pair) );
    memset( pair_brush, 0, sizeof(pair_brush) );
    memset( pair_model, 0, sizeof(pair_model) );
    memset( &player, 0, sizeof(player) );
    world.door = pair;
    world.door_count = 2;
    world.brush = pair_brush;
    world.models = pair_model;
    world.model_count = 3;
    pair[0].model = 1; pair[1].model = 2;
    pair[0].targeted = pair[1].targeted = 1;
    pair[0].state = pair[1].state = ENT_DOOR_CLOSING;
    pair[0].speed = pair[1].speed = 10.0f;
    pair[0].hold = pair[1].hold = -1.0f;
    pair[0].dmg = pair[1].dmg = 100;
    pair_brush[1].ofs.x = pair_brush[2].ofs.x = 10.0f;
    pair_brush[1].solid = pair_brush[2].solid = 1;
    pair_model[1].mins.x = pair_model[1].mins.y = pair_model[1].mins.z = -4.0f;
    pair_model[1].maxs.x = pair_model[1].maxs.y = pair_model[1].maxs.z = 4.0f;
    pair_model[2] = pair_model[1];
    fight.health = 50;
    damage_calls = 0;
    trace_len = 6; trace_pos = 0;
    trace_seq[0] = 1; trace_seq[1] = 0; trace_seq[2] = 1;
    trace_seq[3] = 1; trace_seq[4] = 0; trace_seq[5] = 1;
    ent_move_doors( &world, &player, &fight, &rdr, &touches, 1.0f, ENT_RIDER_LIVE );
    assert( damage_calls == 1 && fight.health == -50 );
    assert( pair_brush[1].ofs.x == 10.0f && pair_brush[2].ofs.x == 0.0f );
    return 0;
}
