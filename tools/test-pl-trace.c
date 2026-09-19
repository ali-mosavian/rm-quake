#include <assert.h>
#include <math.h>
#include <string.h>

#include "item.h"
#include "mdl_ai.h"
#include "pl_trace.h"

static void reset_trace( TraceResult *tr, BspVec3 *fin )
{
    memset( tr, 0, sizeof(*tr) );
    tr->frac = 1.0f;
    tr->end_pos = *fin;
}

static int same_float( float a, float b )
{
    return fabs( a - b ) < 0.0001f;
}

int main( void )
{
    World world;
    ItemEnt item[2];
    Submodel model[3];
    BrushModel brush[3];
    ClipNode clip[2];
    Plane plane[2];
    TraceResult tr;
    MdlEnt mon[3];
    Player player;
    BspVec3 start = { -64.0f, 16.0f, 24.0f };
    BspVec3 fin = { 64.0f, 16.0f, 24.0f };
    BspVec3 mins = { PL_ZLO + 8.0f, PL_ZLO + 8.0f, PL_ZLO };
    BspVec3 maxs = { PL_HALF, PL_HALF, PL_ZHI };
    BspVec3 point = { 0.0f, 0.0f, 0.0f };
    short hit;

    memset( &world, 0, sizeof(world) );
    memset( item, 0, sizeof(item) );
    memset( model, 0, sizeof(model) );
    memset( brush, 0, sizeof(brush) );
    memset( clip, 0, sizeof(clip) );
    memset( plane, 0, sizeof(plane) );
    world.item_count = 1;
    world.item = item;
    item[0].kind = ENT_ITEM_EXPLOBOX;

    reset_trace( &tr, &fin );
    hit = -1;
    pl_trace_items( &world, &start, &fin, &mins, &maxs, -1, &tr, &hit );
    assert( tr.end_pos.x == -16.03125f && tr.norm.x == -1.0f );
    assert( hit == 0 );

    start = tr.end_pos;
    fin = start;
    fin.y = 64.0f;
    reset_trace( &tr, &fin );
    hit = -1;
    pl_trace_items( &world, &start, &fin, &mins, &maxs, -1, &tr, &hit );
    assert( tr.frac == 1.0f && !tr.start_solid && !tr.all_solid );

    start.x = 16.0f; start.y = 16.0f; start.z = 100.0f;
    fin = start; fin.z = 80.0f;
    reset_trace( &tr, &fin );
    hit = -1;
    pl_trace_items( &world, &start, &fin, &mins, &maxs, -1, &tr, &hit );
    assert( tr.end_pos.z == 88.03125f && tr.norm.z == 1.0f );
    start = tr.end_pos;
    fin = start; fin.z -= 1.0f;
    reset_trace( &tr, &fin );
    hit = -1;
    pl_trace_items( &world, &start, &fin, &mins, &maxs, -1, &tr, &hit );
    assert( tr.frac == 0.0f && tr.norm.z == 1.0f && !tr.all_solid );

    start.x = -16.01f; start.y = 16.0f; start.z = 24.0f;
    fin = start; fin.x = -15.0f;
    reset_trace( &tr, &fin );
    hit = -1;
    pl_trace_items( &world, &start, &fin, &mins, &maxs, -1, &tr, &hit );
    assert( tr.frac == 0.0f && tr.norm.x == -1.0f && hit == 0 );

    start.x = 16.0f; start.y = 16.0f; start.z = 32.0f;
    fin = start; fin.x = 20.0f;
    reset_trace( &tr, &fin );
    hit = -1;
    pl_trace_items( &world, &start, &fin, &point, &point, -1, &tr, &hit );
    assert( tr.frac == 0.0f && tr.start_solid && tr.all_solid && hit == 0 );

    fin.x = 64.0f;
    reset_trace( &tr, &fin );
    hit = -1;
    pl_trace_items( &world, &start, &fin, &point, &point, -1, &tr, &hit );
    assert( tr.frac == 1.0f && tr.start_solid && !tr.all_solid && hit == 0 );

    start.x = -64.0f; start.y = 16.0f; start.z = 24.0f;
    fin.x = 64.0f; fin.y = 16.0f; fin.z = 24.0f;
    reset_trace( &tr, &fin );
    hit = -1;
    pl_trace_items( &world, &start, &fin, &point, &point, -1, &tr, &hit );
    assert( tr.end_pos.x == -0.03125f && hit == 0 );

    reset_trace( &tr, &fin );
    tr.frac = 0.25f;
    hit = -1;
    pl_trace_items( &world, &start, &fin, &mins, &maxs, -1, &tr, &hit );
    assert( tr.frac == 0.25f && hit == -1 );

    reset_trace( &tr, &fin );
    hit = -1;
    pl_trace_items( &world, &start, &fin, &mins, &maxs, 0, &tr, &hit );
    assert( tr.frac == 1.0f && hit == -1 );

    world.item_count = 2;
    item[1].kind = ENT_ITEM_EXPLOBOX;
    item[1].pos.x = 64.0f;
    start.x = 16.0f; start.y = 16.0f; start.z = 32.0f;
    fin = start; fin.x = 120.0f;
    reset_trace( &tr, &fin );
    hit = -1;
    pl_trace_items( &world, &start, &fin, &point, &point, -1, &tr, &hit );
    assert( tr.end_pos.x == 63.96875f && tr.start_solid && !tr.all_solid && hit == 1 );

    item[0].pos.x = 64.0f;
    item[1].pos.x = 0.0f;
    reset_trace( &tr, &fin );
    hit = -1;
    pl_trace_items( &world, &start, &fin, &point, &point, -1, &tr, &hit );
    assert( tr.end_pos.x == fin.x && tr.start_solid && !tr.all_solid && hit == 1 );

    world.item_count = 1;
    item[0].gone = -1;
    reset_trace( &tr, &fin );
    hit = -1;
    pl_trace_items( &world, &start, &fin, &mins, &maxs, -1, &tr, &hit );
    assert( tr.frac == 1.0f && hit == -1 );

    /* Hull traces identify the brush that actually won. If a nearer
       exploding box wins afterwards, it clears that farther brush hit. */
    world.models = model;
    world.brush = brush;
    world.clip = clip;
    world.planes = plane;
    world.model_count = 2;
    model[0].head_node1 = CONTENTS_EMPTY;
    model[1].head_node1 = 0;
    brush[1].solid = 1;
    clip[0].plane_num = 0;
    clip[0].front = CONTENTS_EMPTY;
    clip[0].back = CONTENTS_SOLID;
    plane[0].norm.x = 1.0f;
    clip[1].plane_num = 1;
    clip[1].front = CONTENTS_EMPTY;
    clip[1].back = CONTENTS_SOLID;
    plane[1].norm.x = -1.0f;
    start.x = 64.0f; start.y = 0.0f; start.z = 0.0f;
    fin.x = -64.0f; fin.y = 0.0f; fin.z = 0.0f;
    pl_trace( &world, &start, &fin, &tr );
    assert( tr.hit_model == 1 && tr.frac < 1.0f );

    start.x = -4.0f;
    fin.x = 4.0f;
    pl_trace( &world, &start, &fin, &tr );
    assert( tr.start_solid && !tr.all_solid );
    assert( tr.frac == 1.0f && tr.hit_model == 1 );

    fin.x = -2.0f;
    pl_trace( &world, &start, &fin, &tr );
    assert( tr.start_solid && tr.all_solid && tr.hit_model == 1 );

    /* A start-inside brush replaces an earlier world hit with fraction
       one, whose endpoint must therefore be the requested finish. */
    start.x = 64.0f;
    fin.x = -64.0f;
    model[0].head_node1 = 0;
    model[1].head_node1 = 1;
    brush[1].ofs.x = 60.0f;
    pl_trace( &world, &start, &fin, &tr );
    assert( tr.start_solid && !tr.all_solid && tr.hit_model == 1 );
    assert( tr.frac == 1.0f && tr.end_pos.x == fin.x );
    model[1].head_node1 = 0;
    brush[1].ofs.x = 0.0f;

    /* Once the accumulated trace is wholly solid, later brushes cannot
       replace it with a partial hit and make the move look possible. */
    start.x = 64.0f;
    fin.x = -64.0f;
    model[0].head_node1 = CONTENTS_SOLID;
    pl_trace( &world, &start, &fin, &tr );
    assert( tr.all_solid && tr.hit_model == 0 );
    world.item_count = 1;
    item[0].gone = 0;
    item[0].kind = ENT_ITEM_EXPLOBOX;
    item[0].pos.x = item[0].pos.y = item[0].pos.z = 0.0f;
    hit = -1;
    pl_trace_move( &world, &start, &fin, -1, &tr, &hit );
    assert( tr.all_solid && tr.hit_model == 0 && hit == -1 );
    world.item_count = 0;

    model[0].head_node1 = CONTENTS_EMPTY;
    model[1].head_node1 = CONTENTS_SOLID;
    model[2].head_node1 = 0;
    brush[2].solid = 1;
    world.model_count = 3;
    pl_trace( &world, &start, &fin, &tr );
    assert( tr.all_solid && tr.hit_model == 1 );

    model[1].head_node1 = 0;
    brush[2].solid = 0;
    world.model_count = 2;

    world.item_count = 1;
    item[0].gone = 0;
    item[0].kind = ENT_ITEM_EXPLOBOX;
    item[0].pos.x = 0.0f;
    item[0].pos.y = 0.0f;
    item[0].pos.z = 0.0f;
    start.x = 64.0f;
    fin.x = -64.0f;
    pl_trace_move( &world, &start, &fin, -1, &tr, &hit );
    assert( hit == 0 && tr.hit_model == -1 );

    /* A live actor owns the nearer hit, ignores itself, and keeps its
       identity separate from BSP and exploding-box identities. */
    memset( mon, 0, sizeof(mon) );
    memset( &player, 0, sizeof(player) );
    world.mon = mon;
    world.mon_count = 1;
    world.item_count = 0;
    world.model_count = 1;
    model[0].head_node1 = CONTENTS_EMPTY;
    mon[0].kind = MDL_KIND_DOG;
    mon[0].state = MDL_ST_RUN;
    start.x = -100.0f; start.y = 0.0f; start.z = 0.0f;
    fin.x = 100.0f; fin.y = 0.0f; fin.z = 0.0f;
    reset_trace( &tr, &fin );
    hit = PL_ACTOR_NONE;
    pl_trace_actors( &world, (Player *) 0, (MdlEnt far *) 0,
                     &start, &fin, &mins, &maxs, &tr, &hit );
    assert( same_float( tr.end_pos.x, -48.03125f ) && tr.norm.x == -1.0f && hit == 0 );

    reset_trace( &tr, &fin );
    hit = PL_ACTOR_NONE;
    pl_trace_actors( &world, (Player *) 0, &mon[0],
                     &start, &fin, &mins, &maxs, &tr, &hit );
    assert( tr.frac == 1.0f && hit == PL_ACTOR_NONE );

    /* Movement solidity follows QuakeC's death frames, without making
       those dying bodies damageable again. */
    mon[0].state = MDL_ST_DEAD;
    mon[0].anim_frame = 0;
    reset_trace( &tr, &fin );
    hit = PL_ACTOR_NONE;
    pl_trace_actors( &world, (Player *) 0, (MdlEnt far *) 0,
                     &start, &fin, &mins, &maxs, &tr, &hit );
    assert( tr.frac == 1.0f && hit == PL_ACTOR_NONE );
    mon[0].kind = MDL_KIND_ARMY;
    reset_trace( &tr, &fin );
    pl_trace_actors( &world, (Player *) 0, (MdlEnt far *) 0,
                     &start, &fin, &mins, &maxs, &tr, &hit );
    assert( same_float( tr.end_pos.x, -32.03125f ) );

    /* Id's six-plane box is min-inclusive and max-exclusive. */
    start.x = fin.x = 32.0f;
    start.y = 0.0f; fin.y = 8.0f;
    reset_trace( &tr, &fin );
    hit = PL_ACTOR_NONE;
    pl_trace_actors( &world, (Player *) 0, (MdlEnt far *) 0,
                     &start, &fin, &mins, &maxs, &tr, &hit );
    assert( tr.frac == 1.0f && !tr.start_solid && !tr.all_solid );
    start.x = 64.0f; fin.x = 32.0f;
    start.y = fin.y = 0.0f;
    reset_trace( &tr, &fin );
    hit = PL_ACTOR_NONE;
    pl_trace_actors( &world, (Player *) 0, (MdlEnt far *) 0,
                     &start, &fin, &mins, &maxs, &tr, &hit );
    assert( tr.frac == 1.0f && !tr.start_solid && !tr.all_solid &&
            hit == PL_ACTOR_NONE );
    start.x = fin.x = -32.0f;
    reset_trace( &tr, &fin );
    hit = PL_ACTOR_NONE;
    pl_trace_actors( &world, (Player *) 0, (MdlEnt far *) 0,
                     &start, &fin, &mins, &maxs, &tr, &hit );
    assert( tr.frac == 0.0f && tr.start_solid && tr.all_solid && hit == 0 );
    fin = start;
    reset_trace( &tr, &fin );
    hit = PL_ACTOR_NONE;
    pl_trace_actors( &world, (Player *) 0, (MdlEnt far *) 0,
                     &start, &fin, &mins, &maxs, &tr, &hit );
    assert( tr.frac == 0.0f && tr.start_solid && tr.all_solid && hit == 0 );
    start.x = -100.0f; start.y = 0.0f;
    fin.x = 100.0f; fin.y = 0.0f;

    mon[0].anim_frame = 2;
    reset_trace( &tr, &fin );
    hit = PL_ACTOR_NONE;
    pl_trace_actors( &world, (Player *) 0, (MdlEnt far *) 0,
                     &start, &fin, &mins, &maxs, &tr, &hit );
    assert( tr.frac == 1.0f && hit == PL_ACTOR_NONE );
    mon[0].kind = MDL_KIND_DEMON;
    mon[0].anim_frame = 4;
    reset_trace( &tr, &fin );
    pl_trace_actors( &world, (Player *) 0, (MdlEnt far *) 0,
                     &start, &fin, &mins, &maxs, &tr, &hit );
    assert( same_float( tr.end_pos.x, -48.03125f ) );
    mon[0].anim_frame = 5;
    reset_trace( &tr, &fin );
    hit = PL_ACTOR_NONE;
    pl_trace_actors( &world, (Player *) 0, (MdlEnt far *) 0,
                     &start, &fin, &mins, &maxs, &tr, &hit );
    assert( tr.frac == 1.0f && hit == PL_ACTOR_NONE );

    mon[0].kind = MDL_KIND_ZOMBIE;
    mon[0].state = MDL_ST_PAIN;
    mon[0].anim_set = MDL_SET_PAIN5;
    mon[0].anim_frame = 10;
    reset_trace( &tr, &fin );
    pl_trace_actors( &world, (Player *) 0, (MdlEnt far *) 0,
                     &start, &fin, &mins, &maxs, &tr, &hit );
    assert( tr.frac == 1.0f );
    mon[0].state = MDL_ST_STAND;
    mon[0].cruc = 1;
    reset_trace( &tr, &fin );
    hit = PL_ACTOR_NONE;
    pl_trace_actors( &world, (Player *) 0, (MdlEnt far *) 0,
                     &start, &fin, &mins, &maxs, &tr, &hit );
    assert( same_float( tr.end_pos.x, -32.03125f ) && hit == 0 );

    /* A monster collides with the player and another monster but not
       with its own record. */
    memset( mon, 0, sizeof(mon) );
    world.mon_count = 2;
    mon[0].kind = mon[1].kind = MDL_KIND_ARMY;
    mon[0].state = mon[1].state = MDL_ST_RUN;
    mon[1].pos.x = 64.0f;
    start.x = 0.0f;
    reset_trace( &tr, &fin );
    hit = PL_ACTOR_NONE;
    pl_trace_actors( &world, (Player *) 0, &mon[0],
                     &start, &fin, &mins, &maxs, &tr, &hit );
    assert( same_float( tr.end_pos.x, 31.96875f ) && hit == 1 );
    world.mon_count = 1;
    player.pos.x = 64.0f;
    reset_trace( &tr, &fin );
    hit = PL_ACTOR_NONE;
    pl_trace_actors( &world, &player, &mon[0],
                     &start, &fin, &mins, &maxs, &tr, &hit );
    assert( same_float( tr.end_pos.x, 31.96875f ) && hit == PL_ACTOR_PLAYER );

    /* pl_trace_move's nearer actor invalidates an earlier item hit. */
    world.mon_count = 1;
    mon[0].kind = MDL_KIND_DOG;
    mon[0].state = MDL_ST_RUN;
    mon[0].pos.x = 0.0f;
    world.item_count = 1;
    item[0].gone = 0;
    item[0].kind = ENT_ITEM_EXPLOBOX;
    item[0].pos.x = 50.0f;
    item[0].pos.y = item[0].pos.z = 0.0f;
    start.x = -100.0f;
    hit = -1;
    pl_trace_move( &world, &start, &fin, -1, &tr, &hit );
    assert( same_float( tr.end_pos.x, -48.03125f ) && tr.hit_model == -1 && hit == -1 );
    return 0;
}
