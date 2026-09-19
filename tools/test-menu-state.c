#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "menu.h"
#include "qgl.h"

#define MENU_BYTES 98304L
#define HELP_BYTES 64000L

static unsigned char row[512];
static short standard_news, help_news, frees, part_reads;
static long last_part_offset;
static short fail_new;

long qglSfNew( short w, short h, short kind )
{
    assert( w == 256 && h == 384 && kind == QGL_SURF_EMS );
    standard_news++;
    if ( fail_new ) { fail_new = 0; return 0; }
    return 100 + standard_news;
}

long qglSfNewEx( short w, short h, short stride, short kind )
{
    assert( w == 320 && h == 200 && stride == 512 && kind == QGL_SURF_EMS );
    help_news++;
    if ( fail_new ) { fail_new = 0; return 0; }
    return 200 + help_news;
}

void qglSfFree( long surface ) { assert( surface != 0 ); frees++; }
long qglSfWrRow( long surface, short y )
{ assert( surface != 0 && y >= 0 && y < 384 ); return (long) row; }
long qglSfRdRow( long surface, short y )
{ assert( surface != 0 && y >= 0 ); return (long) row; }
long qglFileRead( short handle, long destination, long bytes )
{ assert( handle == 1 && destination != 0 ); return bytes; }
short pak_seek( Pak *pak, char *member, long *bytes )
{
    (void) pak;
    assert( strcmp( member, "menu.raw" ) == 0 );
    if ( bytes ) *bytes = MENU_BYTES + 6L * HELP_BYTES;
    return 1;
}
short pak_seek_part( Pak *pak, char *member, long offset, long bytes )
{
    (void) pak;
    assert( strcmp( member, "menu.raw" ) == 0 && bytes == HELP_BYTES );
    assert( offset >= MENU_BYTES && offset < MENU_BYTES + 6L * HELP_BYTES );
    last_part_offset = offset;
    part_reads++;
    return 1;
}

void qglMemCopy( long destination, long source, long bytes )
{ memcpy( (void *) destination, (void *) source, (size_t) bytes ); }
void qglDrBlit( long d, short x, short y, long s )
{ (void) d; (void) x; (void) y; (void) s; }
void qglDrFill( long d, short x0, short y0, short x1, short y1, short colour )
{ (void) d; (void) x0; (void) y0; (void) x1; (void) y1; (void) colour; }
long qglSfViewNew( long parent, short w, short h, short stride )
{ (void) parent; (void) w; (void) h; (void) stride; return 300; }
short qglSfViewAim( long view, long offset )
{ (void) view; (void) offset; return -1; }
void qglDelView( long view ) { (void) view; }

void savegame_scan( char slots[SG_SLOTS][SG_COMMENT], unsigned char loadable[SG_SLOTS] )
{ (void) slots; (void) loadable; }
short keybind_name( short key, char *name )
{ (void) key; strcpy( name, "key" ); return -1; }
void keybind_find_command( const KeyBindings *keys, const char *command,
                           short *first, short *second )
{ (void) keys; (void) command; *first = *second = -1; }
void keybind_unbind_command( KeyBindings *keys, const char *command )
{ (void) keys; (void) command; }
short keybind_set( KeyBindings *keys, const char *name, const char *command, char *error )
{ (void) keys; (void) name; (void) command; (void) error; return -1; }
float cvar_value( const CmdState *cmd, const char *name )
{ (void) cmd; (void) name; return 0.0f; }
short cvar_set( CmdState *cmd, const char *name, const char *value, char *error )
{ (void) cmd; (void) name; (void) value; (void) error; return -1; }
void sys_error( char *message ) { (void) message; abort(); }

static void expect_help( const Menu *menu, short page )
{
    assert( menu->page == MENU_HELP && menu->help_page == page );
    assert( last_part_offset == MENU_BYTES + (long) page * HELP_BYTES );
}

int main( void )
{
    Menu menu;
    Pak pak;
    KeyBindings keys;
    CmdState cmd;
    short i;

    memset( &pak, 0, sizeof(pak) );
    memset( &keys, 0, sizeof(keys) );
    memset( &cmd, 0, sizeof(cmd) );
    menu_init( &menu );
    assert( menu_open( &menu, &pak, MENU_MAIN ) );
    assert( standard_news == 1 && help_news == 0 && frees == 0 );

    menu.cursor = 3;
    assert( menu_key( &menu, KEY_ENTER, &keys, &cmd, &pak ) == MENU_ACTION_NONE );
    expect_help( &menu, 0 );
    assert( standard_news == 1 && help_news == 1 && frees == 1 );
    for ( i = 1; i < 6; i++ ) {
        menu_key( &menu, KEY_RIGHT, &keys, &cmd, &pak );
        expect_help( &menu, i );
    }
    menu_key( &menu, KEY_UP, &keys, &cmd, &pak );
    expect_help( &menu, 0 );
    menu_key( &menu, KEY_LEFT, &keys, &cmd, &pak );
    expect_help( &menu, 5 );
    menu_key( &menu, KEY_DOWN, &keys, &cmd, &pak );
    expect_help( &menu, 4 );
    i = part_reads;
    menu_key( &menu, KEY_ENTER, &keys, &cmd, &pak );
    expect_help( &menu, 4 );
    assert( part_reads == i );

    menu_key( &menu, KEY_ESC, &keys, &cmd, &pak );
    assert( menu.page == MENU_MAIN && standard_news == 2 && frees == 2 );
    assert( menu_open( &menu, &pak, MENU_HELP ) );
    expect_help( &menu, 0 );
    assert( menu_open( &menu, &pak, MENU_OPTIONS ) );
    assert( menu.page == MENU_OPTIONS && standard_news == 3 && frees == 4 );

    assert( menu_open( &menu, &pak, MENU_HELP ) );
    fail_new = 1;
    assert( !menu_open( &menu, &pak, MENU_MAIN ) );
    assert( menu.page == MENU_CLOSED && menu.atlas == 0 );
    menu_close( &menu );
    return 0;
}
