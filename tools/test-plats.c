#include <assert.h>
#include <string.h>

#include "ent.h"
#include "pl_trace.h"
#include "plats.h"

static short trace_solid;
static float trace_z;

void ent_brush_sound( Player *player, World *world, short model, short sound )
{
    (void) player; (void) world; (void) model; (void) sound;
}

void pl_trace( World *world, BspVec3 *start, BspVec3 *fin, TraceResult *tr )
{
    (void) world; (void) fin;
    memset( tr, 0, sizeof(*tr) );
    tr->all_solid = trace_solid;
    trace_z = start->z;
}

static void fixture( World *world, Player *player, PlatEnt *plat,
                     BrushModel *brush, PathCorner *corner )
{
    memset( world, 0, sizeof(*world) );
    memset( player, 0, sizeof(*player) );
    memset( plat, 0, sizeof(*plat) );
    memset( brush, 0, 2 * sizeof(*brush) );
    memset( corner, 0, sizeof(*corner) );
    world->plat_count = 1;
    world->plat = plat;
    world->brush = brush;
    world->corner_count = 1;
    world->corner = corner;
    plat->model = 1;
    plat->speed = 16.0f;
    plat->travel = 16.0f;
    plat->mins.x = plat->mins.y = -32.0f;
    plat->mins.z = -16.0f;
    plat->maxs.x = plat->maxs.y = 32.0f;
    plat->maxs.z = 0.0f;
}

int main( void )
{
    World world;
    Player player;
    PlatEnt plat;
    BrushModel brush[2];
    PathCorner corner;

    fixture( &world, &player, &plat, brush, &corner );
    player.pos.z = 64.0f;
    ent_move_plats( &world, &player, 1.0f, ENT_RIDER_DEAD );
    assert( player.pos.z == 64.0f );

    fixture( &world, &player, &plat, brush, &corner );
    player.pos.z = PL_FEET;
    player.on_ground = 1;
    ent_move_plats( &world, &player, 1.0f, ENT_RIDER_DEAD );
    assert( player.pos.z == 8.0f && !player.on_ground );

    fixture( &world, &player, &plat, brush, &corner );
    brush[1].ofs.z = -16.0f;
    player.pos.z = 8.0f;
    player.on_ground = 1;
    ent_move_plats( &world, &player, 1.0f, ENT_RIDER_DEAD );
    assert( player.pos.z == 8.0f && player.on_ground );

    fixture( &world, &player, &plat, brush, &corner );
    plat.kind = ENT_PLAT_KIND_TRAIN;
    plat.state = ENT_TRAIN_MOVE;
    corner.org.x = plat.mins.x + 16.0f;
    corner.org.y = plat.mins.y;
    corner.org.z = plat.mins.z;
    player.pos.z = 0.0f;
    trace_solid = 0;
    ent_move_trains( &world, &player, 1.0f, ENT_RIDER_DEAD );
    assert( player.pos.x == 0.0f );

    fixture( &world, &player, &plat, brush, &corner );
    plat.kind = ENT_PLAT_KIND_TRAIN;
    plat.state = ENT_TRAIN_MOVE;
    corner.org.x = plat.mins.x + 16.0f;
    corner.org.y = plat.mins.y;
    corner.org.z = plat.mins.z;
    player.pos.z = 0.0f;
    trace_solid = 1;
    ent_move_trains( &world, &player, 1.0f, ENT_RIDER_HEAD );
    assert( player.pos.x == 16.0f && trace_z == PL_FEET );
    return 0;
}
