#include <assert.h>
#include <stdlib.h>

#include "video.h"

typedef struct { long dst; short y0, y1; } Fill;

static Fill fills[4];
static short fill_count;
static long blit_dst, blit_src, scaled_blit_dst, scaled_blit_src;
static short blit_x, blit_y, scaled_blit_x, scaled_blit_y;
static short ex_count;
static short ex_w[2], ex_h[2], ex_stride[2], ex_kind[2];
static short new_view_pitch, resize_view_pitch;
static short row_reads, row_writes;

void qglDrFill( long d, short x0, short y0, short x1, short y1, short col )
{
    (void) x0; (void) x1; (void) col;
    assert( fill_count < (short) ( sizeof fills / sizeof fills[0] ) );
    fills[fill_count].dst = d;
    fills[fill_count].y0 = y0;
    fills[fill_count].y1 = y1;
    fill_count++;
}

void qglDrBlitScl( long d, short x, short y, short w, short h, long s )
{
    scaled_blit_dst = d; scaled_blit_src = s;
    scaled_blit_x = x; scaled_blit_y = y;
    (void) w; (void) h;
}
void qglDrBlit( long d, short x, short y, long s )
{ blit_dst = d; blit_src = s; blit_x = x; blit_y = y; }
void qglRowRead( long sf, short x, short y, short pixels, short fmt, long buff )
{ row_reads++; (void) sf; (void) x; (void) y; (void) pixels; (void) fmt; (void) buff; }
void qglRowWrite( long sf, short x, short y, short pixels, short fmt, long buff )
{ row_writes++; (void) sf; (void) x; (void) y; (void) pixels; (void) fmt; (void) buff; }
void qglMemInit( void ) {}
short qglSfInit( void ) { return -1; }
short qglGemInit( void ) { return -1; }
long qglVgaInit( void ) { return 1; }
long qglSfNew( short w, short h, short kind )
{ (void) w; (void) h; (void) kind; return 1; }
long qglSfNewEx( short w, short h, short stride, short kind )
{
    ex_w[ex_count] = w;
    ex_h[ex_count] = h;
    ex_stride[ex_count] = stride;
    ex_kind[ex_count++] = kind;
    return ex_count;
}
long qglSfViewNew( long p, short w, short h, short pitch )
{ (void) p; (void) w; (void) h; new_view_pitch = pitch; return 1; }
short qglSfViewResize( long v, long p, short w, short h, short pitch )
{ (void) v; (void) p; (void) w; (void) h; resize_view_pitch = pitch; return -1; }
void qglDelView( long s ) { (void) s; }
void qglSfFree( long s ) { (void) s; }
void qglMemFree( long p ) { (void) p; }
void pal_install( void *p ) { (void) p; }
void sys_error( const char *s ) { (void) s; abort(); }

static void check( Video *v, short keep_sbar, short cutoff, long target )
{
    short i;

    fill_count = 0;
    blit_dst = blit_src = scaled_blit_dst = scaled_blit_src = 0;
    v_present( v, 2, 0, keep_sbar );
    assert( fill_count == 4 );
    assert( blit_dst == target );
    assert( blit_src == v->h_view );
    assert( blit_x == v->view_x && blit_y == v->view_y );
    assert( scaled_blit_dst == 0 );
    for ( i = 0; i < fill_count; i++ ) {
        assert( fills[i].dst == target );
        assert( fills[i].y1 < cutoff );
    }
    assert( fills[1].y1 == cutoff - 1 );
}

int main( void )
{
    Video v = { 1, 9, 2, 3, 160, 100, 160, 100, 160,
                320, 200, 80, 26, 1, 48, 0 };
    Video init = { 0, 0, 0, 0, 320, 200, 0, 0, 0,
                   320, 200, 0, 0, 1, 0, 0 };

    check( &v, -1, 152, 9 );
    fill_count = 0;
    row_reads = row_writes = 0;
    v_present_warp( &v, 2, 0, -1, 0.0f );
    assert( row_reads == v.y_res + 6 && row_writes == v.y_res );
    v.sbar_lines = 24;
    check( &v, -1, 176, 9 );
    v.sbar_lines = 0;
    check( &v, -1, 200, 9 );
    v.sbar_lines = 48;
    check( &v, 0, 200, 9 );
    check( &v, -1, 152, 9 );

    v.view_scale = 2;
    v.view_x = v.view_y = 0;
    v.y_res = 76;
    fill_count = 0;
    blit_dst = scaled_blit_dst = 0;
    v_present( &v, 2, 0, -1 );
    assert( fill_count == 0 );
    assert( blit_dst == 0 );
    assert( scaled_blit_dst == v.h_comp_dc && scaled_blit_src == v.h_view );
    assert( scaled_blit_x == 0 && scaled_blit_y == 0 );

    scaled_blit_dst = 0;
    v_present_warp( &v, 2, 0, -1, 0.0f );
    assert( scaled_blit_dst == v.h_comp_dc );

    fill_count = 0;
    v_init( &init, 0L );
    assert( ex_count == 2 );
    assert( ex_w[0] == 320 && ex_h[0] == 200 && ex_stride[0] == 512 );
    assert( ex_kind[0] == QGL_SURF_EMS );
    assert( ex_w[1] == 320 && ex_h[1] == 200 && ex_stride[1] == 512 );
    assert( ex_kind[1] == QGL_SURF_EMS );
    assert( new_view_pitch == 512 );
    assert( resize_view_pitch == 512 );
    return 0;
}
