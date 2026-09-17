#include <assert.h>
#include <stdlib.h>

#include "video.h"

typedef struct { long dst; short y0, y1; } Fill;

static Fill fills[4];
static short fill_count;
static long blit_dst;

void qglDrFill( long d, short x0, short y0, short x1, short y1, short col )
{
    (void) x0; (void) x1; (void) col;
    fills[fill_count].dst = d;
    fills[fill_count].y0 = y0;
    fills[fill_count].y1 = y1;
    fill_count++;
}

void qglDrBlitScl( long d, short x, short y, short w, short h, long s )
{ blit_dst = d; (void) x; (void) y; (void) w; (void) h; (void) s; }
void qglMemInit( void ) {}
short qglSfInit( void ) { return -1; }
short qglGemInit( void ) { return -1; }
long qglVgaInit( void ) { return 1; }
long qglSfNew( short w, short h, short kind )
{ (void) w; (void) h; (void) kind; return 1; }
long qglSfViewNew( long p, short w, short h, short pitch )
{ (void) p; (void) w; (void) h; (void) pitch; return 1; }
short qglSfViewResize( long v, long p, short w, short h, short pitch )
{ (void) v; (void) p; (void) w; (void) h; (void) pitch; return -1; }
void qglDelView( long s ) { (void) s; }
void qglSfFree( long s ) { (void) s; }
void qglMemFree( long p ) { (void) p; }
void pal_install( void *p ) { (void) p; }
void sys_error( const char *s ) { (void) s; abort(); }

static void check( Video *v, short keep_sbar, short cutoff, long target )
{
    short i;

    fill_count = 0;
    blit_dst = 0;
    v_present( v, 2, 0, keep_sbar );
    assert( fill_count == 4 );
    assert( blit_dst == target );
    for ( i = 0; i < fill_count; i++ ) {
        assert( fills[i].dst == target );
        assert( fills[i].y1 < cutoff );
    }
    assert( fills[1].y1 == cutoff - 1 );
}

int main( void )
{
    Video v = { 1, 9, 2, 3, 160, 100, 160, 100,
                320, 200, 80, 26, 1, 48, 0, 0 };

    check( &v, -1, 152, 1 );
    v.sbar_lines = 24;
    check( &v, -1, 176, 1 );
    v.sbar_lines = 0;
    check( &v, -1, 200, 1 );
    v.sbar_lines = 48;
    check( &v, 0, 200, 1 );
    v.comp = -1;
    check( &v, -1, 152, 9 );
    return 0;
}
