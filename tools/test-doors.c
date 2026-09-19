#include <assert.h>
#include <string.h>

#include "doors.h"
#include "ent.h"
#include "triggers.h"

static short target_calls;
static short target_id;

void ent_use_targets( World *world, Player *player, Fight *fight,
                      Renderer *rdr, short id )
{
    (void) world; (void) player; (void) fight; (void) rdr;
    target_calls++;
    target_id = id;
}

short ent_box_touched( Player *player, BspVec3 *mins, BspVec3 *maxs, float slack )
{
    (void) player; (void) mins; (void) maxs; (void) slack;
    return 0;
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

int main( void )
{
    World world;
    Player player;
    Fight fight;
    Renderer rdr;
    DoorEnt door;
    BrushModel brush[2];

    memset( &world, 0, sizeof(world) );
    memset( &player, 0, sizeof(player) );
    memset( &fight, 0, sizeof(fight) );
    memset( &rdr, 0, sizeof(rdr) );
    memset( &door, 0, sizeof(door) );
    memset( brush, 0, sizeof(brush) );
    world.door_count = 1;
    world.door = &door;
    world.brush = brush;
    door.model = 1;
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
    return 0;
}
