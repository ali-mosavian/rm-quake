#include <assert.h>
#include <string.h>
#include "part.h"

static unsigned char frame_page[RDR_FRAME_BYTES];
static unsigned char particle_page[PART_PAGES][PART_STATE_PAGE_BYTES];
static unsigned char particle_copy[PART_PAGES][PART_STATE_PAGE_BYTES];
static unsigned char pixels[320 * 200];
static unsigned short depth[320 * 200];
static short state_maps[PART_PAGES];
static short scratch_maps, dst_maps, z_maps;
static short freed_frame, freed_state;
static short fail_state_alloc, fail_frame_map, fail_state_page = -1;

long pascal far qglSfNew( short w, short h, short where )
{
    assert( w == RDR_PART_BYTES && h == RDR_FRAME_ROWS && where == QGL_SURF_EMS );
    return 1;
}

long pascal far qglSfNewEx( short w, short h, short stride, short where )
{
    assert( w == PART_STATE_STRIDE && h == PART_SURF_ROWS );
    assert( stride == PART_STATE_STRIDE && where == QGL_SURF_EMS );
    return fail_state_alloc ? 0 : 4;
}

void pascal far qglSfFree( long dc )
{
    if ( dc == 1 ) freed_frame++;
    else { assert( dc == 4 ); freed_state++; }
}

long pascal far qglSfAccessWrEx( long dc, short y, short slot )
{
    short page;

    if ( dc == 1 ) {
        assert( y == 0 );
        if ( slot == PART_EMS_SLOT ) {
            if ( fail_frame_map ) return 0;
            return (long) frame_page;
        }
        assert( slot == PART_DRAW_SLOT );
        scratch_maps++;
        return (long) frame_page;
    }
    if ( dc == 4 ) {
        assert( slot == PART_EMS_SLOT && !( y & 1 ) );
        page = (short) ( y / 2 );
        assert( page >= 0 && page < PART_PAGES );
        if ( page == fail_state_page ) return 0;
        state_maps[page]++;
        return (long) particle_page[page];
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

static void reset_backing( void )
{
    memset( frame_page, 0, sizeof(frame_page) );
    memset( particle_page, 0, sizeof(particle_page) );
    memset( pixels, 0, sizeof(pixels) );
    memset( depth, 0, sizeof(depth) );
    memset( state_maps, 0, sizeof(state_maps) );
    scratch_maps = dst_maps = z_maps = 0;
    freed_frame = freed_state = 0;
    fail_state_alloc = fail_frame_map = 0;
    fail_state_page = -1;
}

static void init( Renderer *rdr, Mat4 *m )
{
    memset( rdr, 0, sizeof(*rdr) );
    memset( m, 0, sizeof(*m) );
    m->m44 = 1.0f;
    assert( part_init( rdr ) );
    assert( rdr->frame_dc == 1 && rdr->part_dc == 4 );
}

static void draw( Renderer *rdr, Mat4 *m, float time, QSurf zdc )
{
    rdr->anim_time = time;
    part_frame( rdr, m, 160.0f, 100.0f, 0.1f, 2, zdc );
}

static void assert_tail( unsigned char value )
{
    long i;

    for ( i = RDR_PART_BYTES; i < RDR_FRAME_BYTES; i++ )
        assert( frame_page[i] == value );
}

int main( void )
{
    Renderer rdr;
    BspVec3 org = { 0.0f, 0.0f, 0.0f };
    BspVec3 end = { 12.0f, 0.0f, 0.0f };
    BspVec3 dir = { 0.0f, 0.0f, 0.0f };
    Mat4 m;
    unsigned short seed;
    short i;

    assert( PART_EMS_SLOT == 3 && PART_DRAW_SLOT == 0 );
    assert( PART_MAX == 2048 && PART_CHUNK == 512 && PART_PAGES == 4 );

    reset_backing();
    memset( &rdr, 0, sizeof(rdr) );
    fail_state_alloc = 1;
    assert( !part_init( &rdr ) );
    assert( freed_frame == 1 && freed_state == 0 );

    reset_backing();
    memset( &rdr, 0, sizeof(rdr) );
    fail_frame_map = 1;
    assert( !part_init( &rdr ) );
    assert( freed_frame == 1 && freed_state == 1 );

    reset_backing();
    memset( &rdr, 0, sizeof(rdr) );
    fail_state_page = 2;
    assert( !part_init( &rdr ) );
    assert( freed_frame == 1 && freed_state == 1 );

    reset_backing();
    init( &rdr, &m );
    memset( frame_page + RDR_PART_BYTES, 0x5a,
            RDR_FRAME_BYTES - RDR_PART_BYTES );
    part_explosion( &rdr, &org );
    draw( &rdr, &m, 0.0f, 3 );
    assert( rdr.part_live == 1024 && rdr.part_drawn == 1024 );
    assert( scratch_maps == 1 && dst_maps == z_maps );
    for ( i = 0; i < PART_PAGES; i++ ) assert( state_maps[i] > 0 );
    assert_tail( 0x5a );
    part_shutdown( &rdr );
    assert( freed_frame == 1 && freed_state == 1 );

    reset_backing();
    init( &rdr, &m );
    part_effect( &rdr, &org, &dir, 73, 20 );
    draw( &rdr, &m, 0.0f, 3 );
    assert( rdr.part_drawn == 20 );
    assert( pixels[100 * 320 + 160] >= 72 &&
            pixels[100 * 320 + 160] <= 79 );
    part_shutdown( &rdr );

    reset_backing();
    init( &rdr, &m );
    draw( &rdr, &m, 10.0f, 3 );
    part_trail( &rdr, &org, &end, 2 );
    draw( &rdr, &m, 10.0f, 3 );
    assert( rdr.part_drawn == 4 );
    draw( &rdr, &m, 11.95f, 3 );
    assert( rdr.part_live == 4 );
    draw( &rdr, &m, 12.05f, 3 );
    assert( rdr.part_live == 0 && rdr.part_drawn == 0 );
    part_shutdown( &rdr );

    reset_backing();
    init( &rdr, &m );
    part_teleport( &rdr, &org );
    draw( &rdr, &m, 0.0f, 3 );
    assert( rdr.part_live == 896 && rdr.part_drawn == 896 );
    part_shutdown( &rdr );

    reset_backing();
    init( &rdr, &m );
    part_lava( &rdr, &org );
    draw( &rdr, &m, 0.0f, 3 );
    assert( rdr.part_live == 1024 && rdr.part_drawn == 1024 );
    part_shutdown( &rdr );

    reset_backing();
    init( &rdr, &m );
    part_explosion( &rdr, &org );
    part_explosion( &rdr, &org );
    draw( &rdr, &m, 0.0f, 0 );
    assert( rdr.part_live == PART_MAX && rdr.part_drawn == PART_MAX );
    memcpy( particle_copy, particle_page, sizeof(particle_copy) );
    seed = rdr.part_seed;
    part_effect( &rdr, &org, &dir, 73, 20 );
    draw( &rdr, &m, 0.0f, 0 );
    assert( rdr.part_seed == seed );
    assert( !memcmp( particle_copy, particle_page, sizeof(particle_copy) ) );

    memset( pixels, 0x22, sizeof(pixels) );
    memset( depth, 0xff, sizeof(depth) );
    draw( &rdr, &m, 0.0f, 3 );
    assert( pixels[100 * 320 + 160] == 0x22 );

    for ( i = 1; i <= 9; i++ ) draw( &rdr, &m, i * 0.1f, 0 );
    assert( rdr.part_live == 0 );
    part_effect( &rdr, &org, &dir, 73, 20 );
    draw( &rdr, &m, 0.9f, 0 );
    assert( rdr.part_drawn == 20 );
    part_shutdown( &rdr );
    return 0;
}
