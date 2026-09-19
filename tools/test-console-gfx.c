#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "console.h"
#include "qgl.h"
#include "uigfx.h"

static unsigned char source_row[512];
static unsigned char destination_row[512];
static short news, ar_news, frees;
static short new_w[8], new_h[8], new_stride[8];

long qglSfNewEx( short w, short h, short stride, short kind )
{
    assert( kind == QGL_SURF_EMS );
    assert( news < 8 );
    new_w[news] = w;
    new_h[news] = h;
    new_stride[news] = stride;
    news++;
    return 100 + news;
}

long qglArNew( short type, short element_size, long count, short slot )
{
    assert( type == QGL_AR_EMS && element_size == 320 );
    assert( count == 200 && slot == QGL_TEX_SLOT );
    ar_news++;
    return 500 + ar_news;
}

long qglArWin( long handle, long index )
{
    assert( handle >= 501 && index >= 0 && index < 200 );
    return (long) source_row;
}

void qglArFree( long handle )
{
    assert( handle >= 501 );
    frees++;
}

void qglSfFree( long surface )
{
    assert( surface >= 101 );
    frees++;
}

long qglSfWrRow( long surface, short y )
{
    (void) y;
    return surface == 900 ? (long) destination_row : (long) source_row;
}

long qglSfRdRow( long surface, short y )
{
    assert( surface >= 101 );
    (void) y;
    return (long) source_row;
}

long qglFileRead( short handle, long destination, long bytes )
{
    assert( handle == 1 && destination != 0 );
    return bytes;
}

void qglMemCopy( long destination, long source, long bytes )
{
    memcpy( (void *) destination, (void *) source, (size_t) bytes );
}

void qglDrBlit( long destination, short x, short y, long source )
{
    assert( destination == 900 && x == 0 && source >= 101 );
    (void) y;
}

void qglMousePos( short x, short y ) { (void) x; (void) y; }

short pak_seek( Pak *pak, char *member, long *bytes )
{
    (void) pak;
    assert( !strcmp( member, "menu.raw" ) );
    *bytes = UIGFX_FILE_BYTES;
    return 1;
}

short pak_seek_part( Pak *pak, char *member, long offset, long bytes )
{
    (void) pak;
    assert( !strcmp( member, "menu.raw" ) );
    assert( offset >= 0 && bytes > 0 && offset + bytes <= UIGFX_FILE_BYTES );
    return 1;
}

short cmd_enqueue( CmdState far *state, const char *text )
{ (void) state; (void) text; return 1; }
short cvar_get( const CmdState far *state, short index, char *name,
                char *value, short *flags )
{ (void) state; (void) index; (void) name; (void) value; (void) flags; return 0; }
void sys_error( char *message ) { (void) message; abort(); }

int main( void )
{
    Console console;
    Console handoff;
    Pak pak;
    short before;

    memset( &console, 0, sizeof(console) );
    memset( &pak, 0, sizeof(pak) );
    console.line_count = 1;
    console.line_head = 1;
    strcpy( console.line[0], "expired" );
    con_draw( &console, &pak, 900, 320, 0.0f );
    assert( news == 0 && ar_news == 0 );

    console.line_time[0] = 1.0f;
    con_draw( &console, &pak, 900, 320, 0.0f );
    assert( news == 1 && ar_news == 0 );
    assert( new_w[0] == 128 && new_h[0] == 128 && new_stride[0] == 128 );

    console.height = 100.0f;
    con_draw( &console, &pak, 900, 320, 0.0f );
    assert( news == 1 && ar_news == 1 );

    con_gfx_release( &console );
    assert( frees == 2 );
    con_draw( &console, &pak, 900, 320, 0.0f );
    assert( news == 2 && ar_news == 2 );

    console.height = 0.0f;
    console.line_time[0] = 0.0f;
    con_draw( &console, &pak, 900, 320, 0.0f );
    assert( frees == 4 );

    memset( &handoff, 0, sizeof(handoff) );
    handoff.height = 100.0f;
    con_draw_full( &handoff, &pak, 900 );
    before = frees;
    con_animate( &handoff, 1.0f, 200 );
    assert( handoff.height == 0.0f && frees == before + 1 );
    return 0;
}
