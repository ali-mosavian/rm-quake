#include "pal.h"
#include "vshift.h"

static PalRgb pal_base[256];
static PalRgb pal_now[256];
static short  pal_have = 0;

void pal_install( PalRgb far *p )
{
    short i;

    for ( i = 0; i < 256; i++ ) pal_base[i] = pal_now[i] = p[i];
    pal_have = 1;
    qglVgaPalette( (PalRgb far *) pal_now );
}

void pal_apply_shifts( const ViewShift *shifts )
{
    PalRgb next[256];
    short i, changed = 0;

    if ( !pal_have ) return;
    v_shift_palette( next, pal_base, 256, shifts );
    for ( i = 0; i < 256; i++ )
        if ( next[i].red != pal_now[i].red || next[i].green != pal_now[i].green ||
             next[i].blue != pal_now[i].blue ) { changed = -1; break; }
    if ( !changed ) return;
    for ( i = 0; i < 256; i++ ) pal_now[i] = next[i];
    qglVgaPalette( (PalRgb far *) pal_now );
}

PalRgb far *pal_current( void )
{
    return (PalRgb far *) pal_now;
}

/*
 * Nearest entry by squared distance, unweighted -- which is what mgl's
 * own uglPalBestFit did, so the colours it picked stay the colours it
 * picks. Entry 0 wins a tie, matching a forward scan with a strict <.
 */
short pal_bestfit( short r, short g, short b )
{
    long best = 0x7FFFFFFFL;
    short bi = 0;
    short i;

    if ( !pal_have ) return 0;
    for ( i = 0; i < 256; i++ ) {
        long dr = (long) pal_now[i].red   - r;
        long dg = (long) pal_now[i].green - g;
        long db = (long) pal_now[i].blue  - b;
        long d  = dr*dr + dg*dg + db*db;

        if ( d < best ) { best = d; bi = i; if ( d == 0 ) break; }
    }
    return bi;
}
