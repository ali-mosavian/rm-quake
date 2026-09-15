#include <stdio.h>
#include "ems.h"

static int fails;
static void ok( int c, const char *what ) { printf( "%s %s\n", c ? "ok  " : "FAIL", what ); fails += !c; }

int main( void )
{
    sint16 h = ems_alloc( 3 * EMS_PAGE );
    uint8 *w, *stale;
    long before;

    ok( h != 0, "alloc three pages" );
    w = ems_map( h, 0, 1 ); w[5] = 11;
    w = ems_map( h, 2, 1 ); w[5] = 22;
    stale = w;
    w = ems_map( h, 0, 1 );
    ok( w[5] == 11, "page 0's byte survives page 2 taking the window" );
    ok( stale[5] != 22, "a pointer kept across the remap no longer reads page 2" );
    w = ems_map( h, 2, 3 );
    ok( w[5] == 22, "page 2's byte comes back in another slot" );
    before = ems_maps();
    ems_map( h, 2, 3 );
    ok( ems_maps() == before, "the page a slot already holds costs no map" );
    ok( ems_map( h, 3, 0 ) == 0, "a page past the handle is refused" );
    printf( "%s\n", fails ? "FAILED" : "PASSED" );
    return fails != 0;
}
