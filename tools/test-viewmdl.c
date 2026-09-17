#include <assert.h>
#include <string.h>

#define long int
#pragma pack(push, 2)
#include "mdl.h"
#include "fight.h"
#include "viewmdl.c"
#pragma pack(pop)

static World *test_world;
static short loads, unloads, unload_calls, fail_rock = 1;

void mdl_load( Pak *pak, MdlState *m, char *name, short need )
{
    short i;
    (void) pak; (void) need;
    for ( i = 0; i < PL_VIEW_N; i++ )
        assert( !test_world->vmdl[i].loaded );
    loads++;
    if ( fail_rock && !strcmp( name, "v_rock" ) ) { fail_rock = 0; return; }
    m->loaded = 1;
    m->ntri = 1;
}

void mdl_unload( MdlState *m )
{
    unload_calls++;
    if ( m->loaded ) unloads++;
    memset( m, 0, sizeof(*m) );
}

static short resident( const World *world )
{
    short i, count = 0;
    for ( i = 0; i < PL_VIEW_N; i++ ) if ( world->vmdl[i].loaded ) count++;
    return count;
}

int main( void )
{
    World world;

    memset( &world, 0, sizeof(world) );
    test_world = &world;
    host_view_load( &world, PL_IT_SHOTGUN );
    host_view_load( &world, PL_IT_GL );
    assert( !world.vmdl[PL_VIEW_GL].loaded && world.vmdl[PL_VIEW_GL].ntri == -1 );
    host_view_load( &world, PL_IT_GL );
    assert( loads == 2 );
    host_view_load( &world, PL_IT_NAILGUN );
    host_view_load( &world, PL_IT_GL );
    assert( loads == 4 );
    assert( unloads == 2 );
    assert( unload_calls == 3 );
    assert( resident( &world ) == 1 );
    assert( world.vmdl[PL_VIEW_GL].loaded );
    return 0;
}
