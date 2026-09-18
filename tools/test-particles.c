#include <assert.h>
#include <string.h>
#include "part.h"

static unsigned char page[4096];
static unsigned char pixels[320 * 200];
static unsigned short depth[320 * 200];
static short part_maps;
static short dst_maps;
static short z_maps;

long pascal far qglSfNew( short w, short h, short where )
{
    assert( w == 4096 && h == 1 && where == QGL_SURF_EMS );
    return 1;
}

void pascal far qglSfFree( long dc ) { assert( dc == 1 ); }

long pascal far qglSfAccessWrEx( long dc, short y, short slot )
{
    if ( dc == 1 ) {
        assert( y == 0 && slot == PART_EMS_SLOT );
        part_maps++;
        return (long) page;
    }
    if ( dc == 2 ) {
        assert( y >= 0 && y < 200 && slot == 2 );
        dst_maps++;
        return (long) &pixels[y * 320];
    }
    assert( dc == 3 && y >= 0 && y < 200 && slot == QGL_Z_SLOT );
    z_maps++;
    return (long) &depth[y * 320];
}

int main( void )
{
    Renderer rdr;
    BspVec3 org = { 0.0f, 0.0f, 0.0f };
    Mat4 m;

    assert( PART_EMS_SLOT == 3 );
    memset( &rdr, 0, sizeof(rdr) );
    memset( &m, 0, sizeof(m) );
    m.m44 = 1.0f;
    assert( part_init( &rdr ) );
    assert( part_maps == 1 );

    part_maps = 0;
    part_explosion( &rdr, &org );
    part_frame( &rdr, &m, 160.0f, 100.0f, 0.1f, 2, 3 );
    assert( part_maps == 1 );
    assert( rdr.part_pending_n == 0 );
    assert( rdr.part_live == PART_MAX );
    assert( rdr.part_drawn == PART_MAX );
    assert( dst_maps == z_maps && dst_maps <= 200 );
    assert( pixels[100 * 320 + 160] >= 0x61 );
    assert( depth[100 * 320 + 160] > 0 );

    part_frame( &rdr, &m, 160.0f, 100.0f, 0.1f, 2, 3 );
    assert( part_maps == 2 );
    part_shutdown( &rdr );
    return 0;
}
