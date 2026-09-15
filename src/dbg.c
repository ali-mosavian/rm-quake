/*
 * dbg.c -- the debug log, on port E9h, and the stack probe. See dbg.h.
 */

#include "dbg.h"
#include <stdio.h>
#include <dos.h>
#include <alloc.h>  /* sbrk -- the near heap's end, the probe's floor */

#define DBG_PORT    0xE9
#define PAINT_BYTE  0xA5
#define PROBE_GAP   1024U   /* clear of the near heap's break */

extern unsigned _stklen;

#if QR_LOG
/* The emulator flushes a line only at '\n', so every call ends one: a
   fault after this returns cannot lose it. */
void dbg_puts( const char *line )
{
    while ( *line )
        outportb( DBG_PORT, (unsigned char) *line++ );
    outportb( DBG_PORT, '\n' );
}
#endif

void dbg_probe_paint( DbgProbe *p )
{
    char sbuf[64];
    unsigned sp_now, ss_now;
    unsigned char *q;

    /* the real SP: the EXE header says 0x80 whatever _stklen is */
    _asm { mov sp_now, sp }
    _asm { mov ss_now, ss }
    sprintf( sbuf, "stack ss=%u sp=%u stklen=%u", ss_now, sp_now, _stklen );
    mark( sbuf );

    p->lo = (unsigned) sbrk( 0 ) + PROBE_GAP;
    p->hi = sp_now - 256;   /* clear of the current frame */
    for ( q = (unsigned char *) p->lo; q < (unsigned char *) p->hi; q++ )
        *q = PAINT_BYTE;
    sprintf( sbuf, "probe linear=%lu len=%u",
             (unsigned long) ss_now * 16UL + p->lo, (unsigned)( p->hi - p->lo ) );
    mark( sbuf );
}

void dbg_probe_report( const DbgProbe *p )
{
#if QR_LOG
    /* a near malloc after the paint moves the break up into it: start
       above the break as it is now */
    char sbuf[64];
    unsigned brk = (unsigned) sbrk( 0 ) + 16;
    unsigned lo  = p->lo > brk ? p->lo : brk;
    unsigned char *q = (unsigned char *) lo;

    while ( q < (unsigned char *) p->hi && *q == PAINT_BYTE ) q++;
    sprintf( sbuf, "stack_untouched %u of %u",
             (unsigned) q - lo, p->hi > lo ? p->hi - lo : 0 );
    mark( sbuf );
#else
    (void) p;
#endif
}
