#include <assert.h>
#include <string.h>

#include "ent.h"
#include "pl_trace.h"
#include "plats.h"

void pl_touches_add( PlayerTouches *touches, short model )
{
    short i;
    if ( model <= 0 ) return;
    for ( i = 0; i < touches->count; i++ )
        if ( touches->model[i] == model ) return;
    if ( touches->count < PL_TOUCH_MAX )
        touches->model[touches->count++] = model;
}

void ent_doors_touch( World *world, Player *player, Fight *fight,
                      Renderer *rdr, PlayerTouches *touches )
{
    (void) world; (void) player; (void) fight; (void) rdr;
    touches->count = 0;
}
static short trace_solid;
static short trace_seq[8];
static short trace_len, trace_pos;
static float trace_z;
static short move_calls;
static short move_solid[2];
static short trace_clip;
static short damage_calls;
static float damage_value;

short ent_box_touched( Player *player, BspVec3 *mins, BspVec3 *maxs, float slack )
{
    if ( player->pos.x + PL_HALF + slack < mins->x ) return 0;
    if ( player->pos.x - PL_HALF - slack > maxs->x ) return 0;
    if ( player->pos.y + PL_HALF + slack < mins->y ) return 0;
    if ( player->pos.y - PL_HALF - slack > maxs->y ) return 0;
    if ( player->pos.z + PL_ZHI  + slack < mins->z ) return 0;
    if ( player->pos.z + PL_ZLO  - slack > maxs->z ) return 0;
    return 1;
}

void combat_damage_player( Player *player, World *world, Fight *fight,
                           Renderer *rdr, float damage )
{
    (void) player; (void) world; (void) fight; (void) rdr;
    damage_calls++;
    damage_value = damage;
    fight->health -= (short) damage;
}

void ent_brush_sound( Player *player, World *world, short model, short sound )
{
    (void) player; (void) world; (void) model; (void) sound;
}

void pl_trace( World *world, BspVec3 *start, BspVec3 *fin, TraceResult *tr )
{
    (void) world;
    memset( tr, 0, sizeof(*tr) );
    tr->all_solid = trace_pos < trace_len ? trace_seq[trace_pos++] : trace_solid;
    tr->end_pos = *fin;
    if ( trace_clip && start->x != fin->x )
        tr->end_pos.x = ( start->x + fin->x ) * 0.5f;
    trace_z = start->z;
}

void pl_trace_move( World *world, BspVec3 *start, BspVec3 *fin,
                    short ignore_item, TraceResult *tr, short *hit_item )
{
    (void) ignore_item;
    (void) hit_item;
    if ( move_calls < 2 ) move_solid[move_calls] = world->brush[1].solid;
    move_calls++;
    pl_trace( world, start, fin, tr );
}

static void fixture( World *world, Player *player, PlatEnt *plat,
                     BrushModel *brush, PathCorner *corner )
{
    memset( world, 0, sizeof(*world) );
    memset( player, 0, sizeof(*player) );
    memset( plat, 0, sizeof(*plat) );
    memset( brush, 0, 2 * sizeof(*brush) );
    memset( corner, 0, sizeof(*corner) );
    trace_solid = trace_len = trace_pos = move_calls = trace_clip = 0;
    world->plat_count = 1;
    world->plat = plat;
    world->brush = brush;
    world->corner_count = 1;
    world->corner = corner;
    plat->model = 1;
    plat->speed = 16.0f;
    plat->time.travel = 16.0f;
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
    PlatEnt trains[2];
    BrushModel train_brush[3];
    PathCorner train_corner[2];
    Fight fight;
    Renderer rdr;
    PlayerTouches touches;

    memset( &fight, 0, sizeof(fight) );
    memset( &rdr, 0, sizeof(rdr) );
    memset( &touches, 0, sizeof(touches) );

    /* dm3ish's lift: id's trigger is inset 25 units from the brush.
       At x=-17 the player only touches the solid side and must get time
       to step onto it; at x=-42 their box reaches the inset trigger. */
    fixture( &world, &player, &plat, brush, &corner );
    plat.mins.x = -127.0f; plat.maxs.x = -33.0f;
    plat.mins.y = 545.0f;  plat.maxs.y = 639.0f;
    plat.mins.z = -15.0f;  plat.maxs.z = 303.0f;
    plat.time.travel = 288.0f;
    brush[1].ofs.z = -288.0f;
    player.pos.x = -17.0f; player.pos.y = 608.0f; player.pos.z = 39.0f;
    assert( !ent_plat_touched( &player, &world, 0 ) );
    player.pos.x = -40.0f;
    assert( ent_plat_touched( &player, &world, 0 ) );

    plat.start.low_trigger = 1;
    player.pos.z = 300.0f;
    assert( !ent_plat_touched( &player, &world, 0 ) );
    player.pos.z = 39.0f;
    assert( ent_plat_touched( &player, &world, 0 ) );

    /* Leaving the inset trigger while the lift rises must not reverse it.
       The player can cross the trigger before the platform reaches the
       upper doorway, as in the reported dm3ish approach. */
    fixture( &world, &player, &plat, brush, &corner );
    brush[1].ofs.z = -16.0f;
    plat.state = ENT_PLAT_BOTTOM;
    player.pos.z = 8.0f;
    ent_move_plats( &world, &player, 0.25f, ENT_RIDER_LIVE );
    assert( plat.state == ENT_PLAT_UP && brush[1].ofs.z == -12.0f );
    player.pos.x = 100.0f;
    ent_move_plats( &world, &player, 0.25f, ENT_RIDER_LIVE );
    assert( plat.state == ENT_PLAT_UP && brush[1].ofs.z == -8.0f );

    fixture( &world, &player, &plat, brush, &corner );
    brush[1].ofs.z = -16.0f;
    plat.state = ENT_PLAT_BOTTOM;
    player.pos.z = 8.0f;
    ent_move_plats( &world, &player, 1.0f, ENT_RIDER_LIVE );
    assert( plat.state == ENT_PLAT_TOP && plat.wait_left == 3.0f );
    player.pos.x = 100.0f;
    ent_move_plats( &world, &player, 1.0f, ENT_RIDER_LIVE );
    ent_move_plats( &world, &player, 1.0f, ENT_RIDER_LIVE );
    assert( plat.state == ENT_PLAT_TOP && brush[1].ofs.z == 0.0f );
    ent_move_plats( &world, &player, 1.0f, ENT_RIDER_LIVE );
    assert( plat.state == ENT_PLAT_BOTTOM && brush[1].ofs.z == -16.0f );

    fixture( &world, &player, &plat, brush, &corner );
    brush[1].ofs.z = -16.0f;
    plat.state = ENT_PLAT_BOTTOM;
    player.pos.z = 8.0f;
    ent_move_plats( &world, &player, 1.0f, ENT_RIDER_LIVE );
    ent_move_plats( &world, &player, 0.25f, ENT_RIDER_LIVE );
    assert( plat.state == ENT_PLAT_TOP && plat.wait_left == 1.0f );
    player.pos.x = 100.0f;
    ent_move_plats( &world, &player, 0.5f, ENT_RIDER_LIVE );
    assert( plat.state == ENT_PLAT_TOP && plat.wait_left == 0.5f );
    ent_move_plats( &world, &player, 0.5f, ENT_RIDER_LIVE );
    assert( plat.state == ENT_PLAT_DOWN && brush[1].ofs.z == -8.0f );

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
    plat.time.block_at = 0.0f;
    plat.state = ENT_TRAIN_MOVE;
    corner.org.x = plat.mins.x + 16.0f;
    corner.org.y = plat.mins.y;
    corner.org.z = plat.mins.z;
    player.pos.z = 0.0f;
    trace_solid = 0;
    ent_move_trains( &world, &player, &fight, &rdr, &touches,
                     1.0f, ENT_RIDER_DEAD );
    assert( player.pos.x == 0.0f );

    fixture( &world, &player, &plat, brush, &corner );
    plat.kind = ENT_PLAT_KIND_TRAIN;
    plat.time.block_at = 0.0f;
    plat.state = ENT_TRAIN_MOVE;
    corner.org.x = plat.mins.x + 16.0f;
    corner.org.y = plat.mins.y;
    corner.org.z = plat.mins.z;
    player.pos.z = 0.0f;
    trace_len = 3; trace_pos = 0;
    trace_seq[0] = 1; trace_seq[1] = 0; trace_seq[2] = 0;
    ent_move_trains( &world, &player, &fight, &rdr, &touches,
                     1.0f, ENT_RIDER_HEAD );
    assert( player.pos.x == 16.0f && trace_z == PL_FEET );

    fixture( &world, &player, &plat, brush, &corner );
    plat.kind = ENT_PLAT_KIND_TRAIN;
    plat.time.block_at = 0.0f;
    plat.state = ENT_TRAIN_MOVE;
    corner.org.x = plat.mins.x + 16.0f;
    corner.org.y = plat.mins.y;
    corner.org.z = plat.mins.z;
    plat.mode.dmg = 75;
    trace_len = 3; trace_pos = 0;
    trace_seq[0] = 1; trace_seq[1] = 0; trace_seq[2] = 1;
    damage_calls = 0;
    fight.health = 100;
    rdr.anim_time = 1.0f;
    ent_move_trains( &world, &player, &fight, &rdr, &touches,
                     1.0f, ENT_RIDER_LIVE );
    assert( move_calls == 2 );
    assert( damage_calls == 1 && damage_value == 75.0f );
    assert( brush[1].ofs.x == 0.0f && player.pos.x == 0.0f );
    assert( plat.state == ENT_TRAIN_MOVE && plat.time.block_at == 1.5f );

    trace_pos = 0;
    rdr.anim_time = 1.1f;
    ent_move_trains( &world, &player, &fight, &rdr, &touches,
                     1.0f, ENT_RIDER_LIVE );
    assert( damage_calls == 1 );
    trace_pos = 0;
    rdr.anim_time = 1.5f;
    ent_move_trains( &world, &player, &fight, &rdr, &touches,
                     1.0f, ENT_RIDER_LIVE );
    assert( damage_calls == 2 );

    fixture( &world, &player, &plat, brush, &corner );
    plat.kind = ENT_PLAT_KIND_TRAIN;
    plat.state = ENT_TRAIN_MOVE;
    brush[1].solid = 1;
    corner.org.x = plat.mins.x + 16.0f;
    corner.org.y = plat.mins.y;
    corner.org.z = plat.mins.z;
    player.pos.z = PL_FEET;
    player.on_ground = 1;
    fight.health = 100;
    trace_clip = 1;
    ent_move_trains( &world, &player, &fight, &rdr, &touches,
                     1.0f, ENT_RIDER_LIVE );
    assert( player.pos.x == 8.0f && brush[1].ofs.x == 16.0f );
    assert( move_calls == 2 && move_solid[0] == 0 && move_solid[1] == 1 );

    fixture( &world, &player, &plat, brush, &corner );
    plat.kind = ENT_PLAT_KIND_TRAIN;
    plat.time.block_at = 0.0f;
    plat.mode.dmg = 75;
    plat.state = ENT_TRAIN_MOVE;
    corner.org.x = plat.mins.x + 16.0f;
    corner.org.y = plat.mins.y;
    corner.org.z = plat.mins.z;
    trace_pos = 0;
    damage_calls = 0;
    ent_move_trains( &world, &player, &fight, &rdr, &touches,
                     1.0f, ENT_RIDER_DEAD );
    assert( damage_calls == 0 && brush[1].ofs.x == 16.0f );

    fixture( &world, &player, &plat, brush, &corner );
    plat.kind = ENT_PLAT_KIND_TRAIN;
    plat.state = ENT_TRAIN_MOVE;
    corner.org.x = plat.mins.x + 16.0f;
    corner.org.y = plat.mins.y;
    corner.org.z = plat.mins.z;
    player.pos.z = PL_FEET;
    player.on_ground = 1;
    trace_len = 2;
    trace_seq[0] = 0;
    trace_seq[1] = 1;
    ent_move_trains( &world, &player, &fight, &rdr, &touches,
                     1.0f, ENT_RIDER_DEAD );
    assert( !player.on_ground );

    fixture( &world, &player, &plat, brush, &corner );
    plat.kind = ENT_PLAT_KIND_TRAIN;
    plat.time.block_at = 0.0f;
    plat.mode.dmg = 75;
    plat.state = ENT_TRAIN_MOVE;
    corner.org.x = plat.mins.x + 16.0f;
    corner.org.y = plat.mins.y;
    corner.org.z = plat.mins.z;
    player.no_clip = 1;
    damage_calls = 0;
    ent_move_trains( &world, &player, &fight, &rdr, &touches,
                     1.0f, ENT_RIDER_LIVE );
    assert( damage_calls == 0 && brush[1].ofs.x == 16.0f && player.pos.x == 0.0f );

    fixture( &world, &player, &trains[0], train_brush, &train_corner[0] );
    memset( &train_brush[2], 0, sizeof(train_brush[2]) );
    trains[0].kind = ENT_PLAT_KIND_TRAIN;
    trains[0].state = ENT_TRAIN_MOVE;
    trains[0].time.block_at = 0.0f;
    trains[0].mode.dmg = 200;
    train_corner[0].org.x = trains[0].mins.x + 16.0f;
    train_corner[0].org.y = trains[0].mins.y;
    train_corner[0].org.z = trains[0].mins.z;
    trains[1] = trains[0];
    trains[1].model = 2;
    trains[1].first = trains[1].corner = 1;
    train_brush[2] = train_brush[1];
    train_corner[1] = train_corner[0];
    world.plat_count = world.corner_count = 2;
    fight.health = 100;
    damage_calls = 0;
    rdr.anim_time = 1.0f;
    trace_len = 6; trace_pos = 0;
    trace_seq[0] = 1; trace_seq[1] = 0; trace_seq[2] = 1;
    trace_seq[3] = 1; trace_seq[4] = 0; trace_seq[5] = 1;
    ent_move_trains( &world, &player, &fight, &rdr, &touches,
                     1.0f, ENT_RIDER_LIVE );
    assert( damage_calls == 1 );
    assert( fight.health == -100 );
    assert( train_brush[1].ofs.x == 0.0f && train_brush[2].ofs.x == 16.0f );
    return 0;
}
