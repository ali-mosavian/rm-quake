/*
 * qmain.c -- main: one Host, run until it stops. See host.h.
 */

#include "qrcfg.h"
#include <string.h>
#include "dbg.h"
#include "host.h"

/* Borland's stack size, and it is not optional: medium model puts the
   stack in DGROUP beside every near global, so an overflow eats data
   instead of faulting, and the BSP walk recurses deep. Near data plus
   this must fit one 64K DGROUP with room for the near heap, or the
   startup aborts before main to a stderr DOS does not redirect; 32768
   was 22 bytes over. tools/dgroup-check.sh fails the link instead. */
extern unsigned _stklen;
unsigned _stklen = 23808U;

int main( void )
{
    Host host;

    mark( "start" );
    memset( &host, 0, sizeof(host) );
    dbg_probe_paint( &host.probe );

    host_init( &host );
    do {
        while ( host_running( &host ) ) host_frame( &host );
    } while ( host_changelevel( &host ) );
    host_shutdown( &host );
    return 0;
}
