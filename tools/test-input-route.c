#include <assert.h>
#include <stdlib.h>

#include "input.h"

short qglMouseInit( MouseInf *mouse, short xmax, short ymax )
{ (void) mouse; (void) xmax; (void) ymax; return -1; }
void qglKbdInit( Keys *keys ) { (void) keys; }
void qglTmrInit( short hz ) { (void) hz; }
short qglSfSize( long surface, short selector )
{ (void) surface; (void) selector; return 320; }
void sys_error( char *message ) { (void) message; abort(); }
void scr_screenshot( char *name, long surface, short width, short height )
{ (void) name; (void) surface; (void) width; (void) height; }

int main( void )
{
    assert( in_route_key( -1, 0, KEY_TILDE ) == IN_ROUTE_MENU );
    assert( in_route_key( -1, -1, KEY_OEM102 ) == IN_ROUTE_MENU );
    assert( in_route_key( -1, -1, KEY_A ) == IN_ROUTE_MENU );
    assert( in_route_key( 0, -1, KEY_TILDE ) == IN_ROUTE_TOGGLE_CONSOLE );
    assert( in_route_key( 0, 0, KEY_OEM102 ) == IN_ROUTE_TOGGLE_CONSOLE );
    assert( in_route_key( 0, -1, KEY_A ) == IN_ROUTE_CONSOLE );
    assert( in_route_key( 0, 0, KEY_A ) == IN_ROUTE_GAME );
    return 0;
}
