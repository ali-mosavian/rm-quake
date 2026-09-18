#include <assert.h>
#include <string.h>

#include "item.h"
#include "pl_trace.h"

static void reset_trace( TraceResult *tr, BspVec3 *fin )
{
    memset( tr, 0, sizeof(*tr) );
    tr->frac = 1.0f;
    tr->end_pos = *fin;
}

int main( void )
{
    World world;
    ItemEnt item[2];
    TraceResult tr;
    BspVec3 start = { -64.0f, 16.0f, 24.0f };
    BspVec3 fin = { 64.0f, 16.0f, 24.0f };
    BspVec3 mins = { PL_ZLO + 8.0f, PL_ZLO + 8.0f, PL_ZLO };
    BspVec3 maxs = { PL_HALF, PL_HALF, PL_ZHI };
    BspVec3 point = { 0.0f, 0.0f, 0.0f };
    short hit;

    memset( &world, 0, sizeof(world) );
    memset( item, 0, sizeof(item) );
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
    assert( tr.end_pos.x == 63.96875f && tr.start_solid && !tr.all_solid && hit == 0 );

    world.item_count = 1;
    item[0].gone = -1;
    reset_trace( &tr, &fin );
    hit = -1;
    pl_trace_items( &world, &start, &fin, &mins, &maxs, -1, &tr, &hit );
    assert( tr.frac == 1.0f && hit == -1 );
    return 0;
}
