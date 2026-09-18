/*
 * d_poly.c -- the turbulence table. C port of the old d_poly.bas.
 *
 * That module had already been reduced to just this by the time the
 * BASIC-hosted transitional port reached it: d_draw_faces itself moved
 * to a C module a session ago, and nothing called d_poly.bas's own
 * d_clip_z after that -- confirmed by grep across the whole BASIC tree
 * before it was dropped rather than ported.
 */

#include "d_poly.h"

#include "qgl.h"

#define TURB_COUNT 128
#define TURB_AMP   (8L * 65536L)

/* FSIN is a plain 8087+ instruction; this project already assumes the
   FPU is there, so the one-time table build needs no libm. */
static double fsin( double rad )
{
    double result;
    __asm {
        fld   rad
        fsin
        fstp  result
    }
    return result;
}

static short turb_ready;

short d_init_turb( void )
{
    short i;
    long table[TURB_COUNT];

    for ( i = 0; i < TURB_COUNT; i++ )
        table[i] = (long) ( (double) TURB_AMP
                           + (double) TURB_AMP
                           * fsin( (double) i * (2.0 * 3.14159265 / TURB_COUNT) ) );

    turb_ready = qglRsTurbInit( (void far *) table );
    return turb_ready;
}

short d_turb_ready( void )
{
    return turb_ready;
}
