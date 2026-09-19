/* menu.c -- id's single-player front-end menus. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "menu.h"
#include "qgl.h"
#include "sys.h"
#include "uigfx.h"

#define MENU_W       UIGFX_MENU_W
#define MENU_H       UIGFX_MENU_H
#define MENU_CHARS_Y UIGFX_MENU_CHARS_Y
#define HELP_W       UIGFX_HELP_W
#define HELP_H       UIGFX_HELP_H
#define HELP_STRIDE   512
#define HELP_PAGES   UIGFX_HELP_PAGES
#define QUIT_W       UIGFX_QUIT_W
#define QUIT_H       UIGFX_QUIT_H
#define QUIT_STRIDE    256
#define QUIT_MESSAGES UIGFX_QUIT_MESSAGES
#define MENU_BYTES    UIGFX_MENU_BYTES
#define HELP_BYTES    UIGFX_HELP_BYTES
#define QUIT_BYTES    UIGFX_QUIT_BYTES
#define QUIT_OFFSET   UIGFX_QUIT_OFFSET
#define MENU_FILE_BYTES UIGFX_FILE_BYTES

typedef struct { char *command, *label; } BindRow;

static BindRow bind_rows[] = {
    { "+attack", "attack" },
    { "impulse 10", "change weapon" },
    { "+jump", "jump / swim up" },
    { "+use", "use" },
    { "+forward", "walk forward" },
    { "+back", "backpedal" },
    { "+moveleft", "step left" },
    { "+moveright", "step right" },
    { "+showscores", "show scores" }
};

static void near menu_surface_pic( QSurf source, QSurf dst,
                                   short sx, short sy, short sw, short sh,
                                   short dx, short dy )
{
    unsigned char row[240];
    unsigned char far *src, far *out;
    short x, y;

    for ( y = 0; y < sh; y++ ) {
        src = (unsigned char far *) qglSfRdRow( source, (short) ( sy + y ) ) + sx;
        qglMemCopy( (long) (void far *) row, (long) src, (long) sw );
        out = (unsigned char far *) qglSfWrRow( dst, (short) ( dy + y ) ) + dx;
        for ( x = 0; x < sw; x++ ) if ( row[x] != 255 ) out[x] = row[x];
    }
}

static void near menu_pic( Menu MENU_FAR *menu, QSurf dst,
                           short sx, short sy, short sw, short sh,
                           short dx, short dy )
{
    menu_surface_pic( menu->atlas, dst, sx, sy, sw, sh, dx, dy );
}

static void near menu_char( Menu MENU_FAR *menu, QSurf dst,
                            short x, short y, unsigned char code, short brown )
{
    short index;
    if ( code < 32 && code != 12 && code != 13 ) code = '?';
    if ( brown ) code = (unsigned char) ( code + 128 );
    index = code;
    menu_pic( menu, dst, (short) ( index % 32 * 8 ),
              (short) ( MENU_CHARS_Y + index / 32 * 8 ), 8, 8, x, y );
}

static void near menu_text( Menu MENU_FAR *menu, QSurf dst,
                            short x, short y, const char *text, short brown )
{
    short i;
    for ( i = 0; text[i]; i++ )
        menu_char( menu, dst, (short) ( x + i * 8 ), y,
                   (unsigned char) text[i], brown );
}

static void near menu_center( Menu MENU_FAR *menu, QSurf dst,
                              short y, const char *text )
{
    menu_text( menu, dst, (short) ( 160 - strlen( text ) * 4 ), y, text, 0 );
}

static void near menu_slider( Menu MENU_FAR *menu, QSurf dst, short y, float value )
{
    short i, x = 220;
    if ( value < 0.0f ) value = 0.0f;
    if ( value > 1.0f ) value = 1.0f;
    menu_char( menu, dst, (short) ( x - 8 ), y, '[', 0 );
    for ( i = 0; i < 10; i++ ) menu_char( menu, dst, (short) ( x + i * 8 ), y, '-', 0 );
    menu_char( menu, dst, (short) ( x + 80 ), y, ']', 0 );
    menu_char( menu, dst, (short) ( x + value * 72.0f ), y, 'O', 0 );
}

static void near menu_checkbox( Menu MENU_FAR *menu, QSurf dst, short y, short on )
{
    menu_text( menu, dst, 220, y, on ? "on" : "off", 0 );
}

void menu_init( Menu MENU_FAR *menu )
{
    _fmemset( menu, 0, sizeof(*menu) );
}

static void near menu_load_help_page( Menu MENU_FAR *menu, Pak *pak )
{
    short handle, y;

    handle = pak_seek_part( pak, "menu.raw",
                            MENU_BYTES + (long) menu->help_page * HELP_BYTES,
                            HELP_BYTES );
    for ( y = 0; y < HELP_H; y++ )
        if ( qglFileRead( handle, qglSfWrRow( menu->atlas, y ), HELP_W ) != HELP_W )
            sys_error( "menu.raw help page is short" );
}

static short near menu_surface( Menu MENU_FAR *menu, Pak *pak, short help )
{
    short handle, y, had_help = (short) ( menu->page == MENU_HELP );
    long bytes;

    if ( menu->atlas && had_help == help ) return -1;
    if ( menu->atlas ) qglSfFree( menu->atlas );
    menu->atlas = help
        ? qglSfNewEx( HELP_W, HELP_H, HELP_STRIDE, QGL_SURF_EMS )
        : qglSfNew( MENU_W, MENU_H, QGL_SURF_EMS );
    if ( !menu->atlas ) {
        menu->page = MENU_CLOSED;
        return 0;
    }
    handle = pak_seek( pak, "menu.raw", &bytes );
    if ( bytes != MENU_FILE_BYTES ) sys_error( "menu.raw has the wrong size" );
    if ( help ) return -1;
    for ( y = 0; y < MENU_H; y++ )
        if ( qglFileRead( handle, qglSfWrRow( menu->atlas, y ), MENU_W ) != MENU_W )
            sys_error( "menu.raw atlas is short" );
    return -1;
}

short menu_open( Menu MENU_FAR *menu, Pak *pak, MenuPage page )
{
    if ( menu->quit_dialog ) qglSfFree( menu->quit_dialog );
    menu->quit_dialog = 0;
    menu->quit_active = 0;
    if ( !menu_surface( menu, pak, (short) ( page == MENU_HELP ) ) ) return 0;
    menu->page = page;
    menu->cursor = 0;
    menu->bind_grab = 0;
    menu->notice[0] = 0;
    if ( page == MENU_HELP ) {
        menu->help_page = 0;
        menu_load_help_page( menu, pak );
    }
    if ( page == MENU_LOAD || page == MENU_SAVE )
        savegame_scan( menu->slots, menu->loadable );
    return -1;
}

short menu_quit( Menu MENU_FAR *menu, Pak *pak )
{
    short handle, y;
    long bytes, offset;

    if ( menu->quit_active ) return -1;
    menu->quit_message = (short) ( rand() & ( QUIT_MESSAGES - 1 ) );
    menu->quit_dialog = qglSfNewEx( QUIT_W, QUIT_H, QUIT_STRIDE, QGL_SURF_EMS );
    if ( !menu->quit_dialog ) return 0;
    handle = pak_seek( pak, "menu.raw", &bytes );
    if ( bytes != MENU_FILE_BYTES ) sys_error( "menu.raw has the wrong size" );
    offset = QUIT_OFFSET + (long) menu->quit_message * QUIT_BYTES;
    handle = pak_seek_part( pak, "menu.raw", offset, QUIT_BYTES );
    for ( y = 0; y < QUIT_H; y++ )
        if ( qglFileRead( handle, qglSfWrRow( menu->quit_dialog, y ), QUIT_W ) != QUIT_W )
            sys_error( "menu.raw quit dialog is short" );
    menu->quit_active = -1;
    return -1;
}

static void near menu_quit_cancel( Menu MENU_FAR *menu )
{
    if ( menu->quit_dialog ) qglSfFree( menu->quit_dialog );
    menu->quit_dialog = 0;
    menu->quit_active = 0;
}

void menu_close( Menu MENU_FAR *menu )
{
    if ( menu->atlas ) qglSfFree( menu->atlas );
    if ( menu->quit_dialog ) qglSfFree( menu->quit_dialog );
    menu->atlas = 0;
    menu->quit_dialog = 0;
    menu->page = MENU_CLOSED;
    menu->bind_grab = 0;
    menu->quit_active = 0;
}

short menu_is_open( const Menu MENU_FAR *menu )
{
    return (short) ( menu && ( menu->page != MENU_CLOSED || menu->quit_active ) );
}

static void near menu_set_cvar( CmdState far *cmd, const char *name, float value )
{
    char text[24], error[CMD_TEXT];
    sprintf( text, "%.2f", value );
    cvar_set( cmd, name, text, error );
}

static void near menu_adjust( Menu MENU_FAR *menu, CmdState far *cmd, short direction )
{
    float value;
    switch ( menu->cursor ) {
    case 3:
        value = cvar_value( cmd, "viewsize" ) + direction * 10.0f;
        menu_set_cvar( cmd, "viewsize", value );
        break;
    case 4:
        value = cvar_value( cmd, "fov" ) + direction * 5.0f;
        menu_set_cvar( cmd, "fov", value );
        break;
    case 5:
        value = cvar_value( cmd, "volume" ) + direction * 0.1f;
        menu_set_cvar( cmd, "volume", value );
        break;
    case 6:
        value = cvar_value( cmd, "cl_forwardspeed" ) > 200.0f ? 200.0f : 400.0f;
        menu_set_cvar( cmd, "cl_forwardspeed", value );
        break;
    case 7:
        value = -cvar_value( cmd, "m_pitch" );
        menu_set_cvar( cmd, "m_pitch", value );
        break;
    }
}

MenuAction menu_key( Menu MENU_FAR *menu, short key,
                     KeyBindings KB_FAR *keys, CmdState far *cmd, Pak *pak )
{
    short max, first, second;
    char name[16], error[CMD_TEXT];

    if ( menu->quit_active ) {
        if ( key == KEY_Y ) return MENU_ACTION_QUIT;
        if ( key == KEY_N || key == KEY_ESC ) menu_quit_cancel( menu );
        return MENU_ACTION_NONE;
    }
    if ( menu->page == MENU_KEYS && menu->bind_grab ) {
        if ( key != KEY_ESC && key != KEY_TILDE && keybind_name( key, name ) )
            keybind_set( keys, name, bind_rows[menu->cursor].command, error );
        menu->bind_grab = 0;
        return MENU_ACTION_NONE;
    }
    if ( menu->page == MENU_HELP ) {
        if ( key == KEY_ESC ) menu_open( menu, pak, MENU_MAIN );
        else if ( key == KEY_UP || key == KEY_RIGHT ) {
            menu->help_page = (short) ( ( menu->help_page + 1 ) % HELP_PAGES );
            menu_load_help_page( menu, pak );
        } else if ( key == KEY_DOWN || key == KEY_LEFT ) {
            menu->help_page = (short) ( menu->help_page ? menu->help_page - 1
                                                       : HELP_PAGES - 1 );
            menu_load_help_page( menu, pak );
        }
        return MENU_ACTION_NONE;
    }
    if ( key == KEY_ESC ) {
        if ( menu->page == MENU_MAIN ) menu_close( menu );
        else if ( menu->page == MENU_KEYS ) { menu->page = MENU_OPTIONS; menu->cursor = 0; }
        else if ( menu->page == MENU_LOAD || menu->page == MENU_SAVE ) {
            menu->page = MENU_SINGLE; menu->cursor = 0;
        }
        else { menu->page = MENU_MAIN; menu->cursor = 0; }
        return MENU_ACTION_NONE;
    }
    max = menu->page == MENU_MAIN ? 5 : menu->page == MENU_SINGLE ? 3
          : menu->page == MENU_OPTIONS ? 8
          : menu->page == MENU_KEYS ? (short) ( sizeof(bind_rows) / sizeof(bind_rows[0]) )
          : SG_SLOTS;
    if ( key == KEY_UP || ( key == KEY_LEFT && menu->page != MENU_OPTIONS ) ) {
        menu->cursor = (short) ( menu->cursor ? menu->cursor - 1 : max - 1 );
        return MENU_ACTION_NONE;
    }
    if ( key == KEY_DOWN || ( key == KEY_RIGHT && menu->page != MENU_OPTIONS ) ) {
        menu->cursor = (short) ( ( menu->cursor + 1 ) % max );
        return MENU_ACTION_NONE;
    }
    if ( menu->page == MENU_OPTIONS && ( key == KEY_LEFT || key == KEY_RIGHT ) ) {
        menu_adjust( menu, cmd, (short) ( key == KEY_RIGHT ? 1 : -1 ) );
        return MENU_ACTION_NONE;
    }
    if ( menu->page == MENU_KEYS && ( key == KEY_BACKSPC || key == KEY_DEL ) ) {
        keybind_unbind_command( keys, bind_rows[menu->cursor].command );
        return MENU_ACTION_NONE;
    }
    if ( key != KEY_ENTER ) return MENU_ACTION_NONE;

    if ( menu->page == MENU_MAIN ) {
        if ( menu->cursor == 0 ) { menu->page = MENU_SINGLE; menu->cursor = 0; }
        else if ( menu->cursor == 1 ) strcpy( menu->notice, "Multiplayer is not available" );
        else if ( menu->cursor == 2 ) { menu->page = MENU_OPTIONS; menu->cursor = 0; }
        else if ( menu->cursor == 3 ) menu_open( menu, pak, MENU_HELP );
        else menu_quit( menu, pak );
    } else if ( menu->page == MENU_SINGLE ) {
        if ( menu->cursor == 0 ) return MENU_ACTION_NEW;
        menu->page = menu->cursor == 1 ? MENU_LOAD : MENU_SAVE;
        menu->cursor = 0;
    } else if ( menu->page == MENU_OPTIONS ) {
        if ( menu->cursor == 0 ) { menu->page = MENU_KEYS; menu->cursor = 0; }
        else if ( menu->cursor == 1 ) return MENU_ACTION_CONSOLE;
        else if ( menu->cursor == 2 ) return MENU_ACTION_DEFAULTS;
        else menu_adjust( menu, cmd, 1 );
    } else if ( menu->page == MENU_KEYS ) {
        keybind_find_command( keys, bind_rows[menu->cursor].command, &first, &second );
        if ( second >= 0 ) keybind_unbind_command( keys, bind_rows[menu->cursor].command );
        menu->bind_grab = -1;
    } else if ( menu->page == MENU_LOAD ) {
        if ( menu->loadable[menu->cursor] ) return MENU_ACTION_LOAD;
    } else if ( menu->page == MENU_SAVE ) {
        return MENU_ACTION_SAVE;
    }
    return MENU_ACTION_NONE;
}

static void near menu_draw_main( Menu MENU_FAR *menu, QSurf dst, float now )
{
    short frame = (short) ( (long) ( now * 10.0f ) % 6L );
    menu_pic( menu, dst, 0, 176, 32, 144, 16, 4 );
    menu_pic( menu, dst, 32, 176, 96, 24, 112, 4 );
    menu_pic( menu, dst, 0, 0, 240, 112, 72, 32 );
    menu_pic( menu, dst, (short) ( 32 + frame * 16 ), 224, 16, 24,
              54, (short) ( 32 + menu->cursor * 20 ) );
    if ( menu->notice[0] ) menu_center( menu, dst, 160, menu->notice );
}

static void near menu_draw_single( Menu MENU_FAR *menu, QSurf dst, float now )
{
    short frame = (short) ( (long) ( now * 10.0f ) % 6L );
    menu_pic( menu, dst, 0, 176, 32, 144, 16, 4 );
    menu_pic( menu, dst, 128, 176, 128, 24, 96, 4 );
    menu_pic( menu, dst, 0, 112, 232, 64, 72, 32 );
    menu_pic( menu, dst, (short) ( 32 + frame * 16 ), 224, 16, 24,
              54, (short) ( 32 + menu->cursor * 20 ) );
}

static void near menu_draw_options( Menu MENU_FAR *menu, CmdState far *cmd, QSurf dst )
{
    static char *labels[] = {
        "    Customize controls", "         Go to console", "     Reset to defaults",
        "           Screen size", "         Field of view", "          Sound volume",
        "            Always Run", "          Invert Mouse"
    };
    short i;
    menu_pic( menu, dst, 0, 176, 32, 144, 16, 4 );
    menu_pic( menu, dst, 32, 200, 144, 24, 88, 4 );
    for ( i = 0; i < 8; i++ ) menu_text( menu, dst, 16, (short) ( 32 + i * 8 ), labels[i], -1 );
    menu_slider( menu, dst, 56, ( cvar_value( cmd, "viewsize" ) - 30.0f ) / 90.0f );
    menu_slider( menu, dst, 64, ( cvar_value( cmd, "fov" ) - 10.0f ) / 160.0f );
    menu_slider( menu, dst, 72, cvar_value( cmd, "volume" ) );
    menu_checkbox( menu, dst, 80, (short) ( cvar_value( cmd, "cl_forwardspeed" ) > 200.0f ) );
    menu_checkbox( menu, dst, 88, (short) ( cvar_value( cmd, "m_pitch" ) < 0.0f ) );
    menu_char( menu, dst, 200, (short) ( 32 + menu->cursor * 8 ), '>', 0 );
}

static void near menu_draw_keys( Menu MENU_FAR *menu, const KeyBindings KB_FAR *keys,
                                 QSurf dst )
{
    short i, first, second, y;
    char one[16], two[16];
    menu_pic( menu, dst, 32, 248, 184, 24, 68, 4 );
    menu_center( menu, dst, 32, menu->bind_grab
                 ? "Press a key for this action"
                 : "Enter to change, backspace to clear" );
    for ( i = 0; i < (short) ( sizeof(bind_rows) / sizeof(bind_rows[0]) ); i++ ) {
        y = (short) ( 48 + i * 8 );
        menu_text( menu, dst, 16, y, bind_rows[i].label, -1 );
        keybind_find_command( keys, bind_rows[i].command, &first, &second );
        if ( first < 0 ) menu_text( menu, dst, 148, y, "???", -1 );
        else {
            keybind_name( first, one );
            menu_text( menu, dst, 148, y, one, -1 );
            if ( second >= 0 ) {
                keybind_name( second, two );
                menu_text( menu, dst, (short) ( 156 + strlen(one) * 8 ), y, "or", -1 );
                menu_text( menu, dst, (short) ( 180 + strlen(one) * 8 ), y, two, -1 );
            }
        }
    }
    menu_char( menu, dst, 132, (short) ( 48 + menu->cursor * 8 ),
               menu->bind_grab ? '=' : '>', 0 );
}

static void near menu_draw_slots( Menu MENU_FAR *menu, QSurf dst, float now )
{
    short i;
    char line[37];
    menu_pic( menu, dst, menu->page == MENU_LOAD ? 32 : 136, 272,
              menu->page == MENU_LOAD ? 104 : 88, 24,
              menu->page == MENU_LOAD ? 108 : 116, 4 );
    for ( i = 0; i < SG_SLOTS; i++ ) {
        strncpy( line, menu->slots[i], 36 );
        line[36] = 0;
        menu_text( menu, dst, 16, (short) ( 32 + i * 8 ), line, -1 );
    }
    menu_char( menu, dst, 8, (short) ( 32 + menu->cursor * 8 ),
               (unsigned char) ( 12 + ( (long) ( now * 4.0f ) & 1L ) ), 0 );
    if ( menu->notice[0] ) menu_center( menu, dst, 144, menu->notice );
}

static void near menu_fade( QSurf dst, short w, short h )
{
    unsigned char row[320];
    unsigned char far *src, far *out;
    short x, y;

    for ( y = 0; y < h; y++ ) {
        src = (unsigned char far *) qglSfRdRow( dst, y );
        qglMemCopy( (long) (void far *) row, (long) src, w );
        out = (unsigned char far *) qglSfWrRow( dst, y );
        for ( x = 0; x < w; x++ )
            out[x] = ( x & 3 ) == ( ( y & 1 ) << 1 ) ? row[x] : 0;
    }
}

void menu_draw( Menu MENU_FAR *menu, const KeyBindings KB_FAR *keys,
                const CmdState far *cmd, float now, QSurf dst, short w, short h,
                short fade )
{
    short ox = (short) ( ( w - 320 ) / 2 );
    short oy = (short) ( ( h - 200 ) / 2 );
    QSurf view;

    if ( !menu_is_open( menu ) ) return;
    if ( ox || oy ) {
        view = qglSfViewNew( dst, 320, 200, w );
        if ( !view ) return;
        if ( !qglSfViewAim( view, (long) oy * w + ox ) ) {
            qglDelView( view );
            return;
        }
    } else view = dst;
    if ( fade && menu->page != MENU_HELP ) menu_fade( view, 320, 200 );
    if ( menu->page != MENU_CLOSED ) {
        if ( menu->page == MENU_HELP ) qglDrBlit( view, 0, 0, menu->atlas );
        else if ( menu->page == MENU_MAIN ) menu_draw_main( menu, view, now );
        else if ( menu->page == MENU_SINGLE ) menu_draw_single( menu, view, now );
        else if ( menu->page == MENU_OPTIONS )
            menu_draw_options( menu, (CmdState far *) cmd, view );
        else if ( menu->page == MENU_KEYS ) menu_draw_keys( menu, keys, view );
        else menu_draw_slots( menu, view, now );
    }
    if ( menu->quit_active )
        menu_surface_pic( menu->quit_dialog, view, 0, 0, QUIT_W, QUIT_H, 56, 76 );
    if ( view != dst ) qglDelView( view );
}
