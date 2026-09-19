#include <assert.h>
#include <string.h>

#define long int
#pragma pack(push, 2)
#include "mdl.h"
#include "fight.h"
#include "viewmdl.c"
#pragma pack(pop)

static short loads, unloads, unload_calls, fail_rock = 1;

void mdl_load( Pak *pak, MdlState *m, char *name, short need )
{
    (void) pak; (void) need;
    assert( !m->loaded );
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

int main( void )
{
    World world;

    memset( &world, 0, sizeof(world) );
    /* The axe is a real view weapon, not the shotgun fallback. */
    assert( pl_view_of( PL_IT_AXE ) == PL_VIEW_AXE );
    assert( pl_view_of( PL_IT_LIGHTNING ) == PL_VIEW_LIGHT );
    assert( !strcmp( pl_view_name( PL_VIEW_LIGHT ), "v_light" ) );
    host_view_load( &world, PL_IT_SHOTGUN );
    assert( host_view_ready( &world, PL_IT_SHOTGUN ) );
    assert( !host_view_ready( &world, PL_IT_NAILGUN ) );
    host_view_load( &world, PL_IT_GL );
    assert( world.vmdl_id == PL_VIEW_GL );
    assert( !world.vmdl.loaded && world.vmdl.ntri == -1 );
    host_view_load( &world, PL_IT_GL );
    assert( loads == 2 );
    host_view_load( &world, PL_IT_NAILGUN );
    host_view_load( &world, PL_IT_GL );
    assert( loads == 4 );
    assert( unloads == 2 );
    assert( unload_calls == 3 );
    assert( world.vmdl_id == PL_VIEW_GL );
    assert( world.vmdl.loaded );
    return 0;
}
