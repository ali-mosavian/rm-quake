/* console.c -- the in-game command console. */

#include <string.h>

#include "console.h"
#include "input.h"
#include "qgl.h"
#include "screen.h"
#include "sys.h"
#include "uigfx.h"

static void far_zero( void CMD_FAR *ptr, unsigned size )
{
    unsigned i;
    unsigned char CMD_FAR *p = (unsigned char CMD_FAR *) ptr;
    for ( i = 0; i < size; i++ ) p[i] = 0;
}

static void far_put( char CMD_FAR *dst, const char *src, short size )
{
    short i = 0;
    while ( i < size - 1 && src[i] ) { dst[i] = src[i]; i++; }
    dst[i] = 0;
}

static void far_get( char *dst, const char CMD_FAR *src, short size )
{
    short i = 0;
    while ( i < size - 1 && src[i] ) { dst[i] = src[i]; i++; }
    dst[i] = 0;
}

static void far_copy( char CMD_FAR *dst, const char CMD_FAR *src, short size )
{
    short i = 0;
    while ( i < size - 1 && src[i] ) { dst[i] = src[i]; i++; }
    dst[i] = 0;
}

static short near con_has_notifications( const Console CMD_FAR *con )
{
    short i, idx;
    short rows = con->line_count < 4 ? con->line_count : 4;

    for ( i = rows; i > 0; i-- ) {
        idx = (short) ( ( con->line_head - i + CON_LINES ) % CON_LINES );
        if ( con->line_time[idx] > 0.0f ) return -1;
    }
    return 0;
}

static short near con_gfx_trim( Console CMD_FAR *con )
{
    short live;

    if ( con->height > 0.0f ) return -1;
    if ( con->conback ) {
        qglArFree( con->conback );
        con->conback = 0;
    }
    live = con_has_notifications( con );
    if ( !live && con->chars ) {
        qglSfFree( con->chars );
        con->chars = 0;
    }
    return live;
}

void con_init( Console CMD_FAR *con )
{
    far_zero( con, sizeof(*con) );
    con->history_pos = -1;
    con_print( con, "Quake console initialized" );
}

short con_open( const Console CMD_FAR *con )
{
    return con->open;
}

void con_animate( Console CMD_FAR *con, float dt, short screen_h )
{
    float target = con->open ? screen_h * 0.5f : 0.0f;
    float step = 300.0f * dt;
    short i;

    for ( i = 0; i < CON_LINES; i++ ) {
        con->line_time[i] -= dt;
        if ( con->line_time[i] < 0.0f ) con->line_time[i] = 0.0f;
    }
    if ( con->height < target ) {
        con->height += step;
        if ( con->height > target ) con->height = target;
    } else if ( con->height > target ) {
        con->height -= step;
        if ( con->height < target ) con->height = target;
    }
    con_gfx_trim( con );
}

void con_toggle( Console CMD_FAR *con, Input *input )
{
    con->open = (short) !con->open;
    if ( con->open ) {
        con->mouse_x = input->mouse.x;
        con->mouse_y = input->mouse.y;
    } else {
        qglMousePos( con->mouse_x, con->mouse_y );
    }
}

void con_clear( Console CMD_FAR *con )
{
    short i;
    con->line_head = con->line_count = 0;
    for ( i = 0; i < CON_LINES; i++ ) {
        con->line[i][0] = 0;
        con->line_time[i] = 0.0f;
    }
}

void con_print( Console CMD_FAR *con, const char *text )
{
    char piece[CON_LINE];
    short n, cut;

    do {
        n = 0;
        while ( text[n] && text[n] != '\n' && n < CON_WRAP ) n++;
        cut = n;
        if ( text[n] && text[n] != '\n' && n == CON_WRAP ) {
            while ( cut > 0 && text[cut] != ' ' ) cut--;
            if ( cut == 0 ) cut = n;
        }
        for ( n = 0; n < cut; n++ ) piece[n] = text[n];
        piece[cut] = 0;
        far_put( con->line[con->line_head], piece, CON_LINE );
        con->line_time[con->line_head] = 3.0f;
        con->line_head = (short) ( ( con->line_head + 1 ) % CON_LINES );
        if ( con->line_count < CON_LINES ) con->line_count++;
        con->scroll = 0;
        text += cut;
        while ( *text == ' ' ) text++;
        if ( *text == '\n' ) text++;
    } while ( *text );
}

static char con_char( short key, short shift )
{
    static char lower[] = "1234567890-=";
    static char upper[] = "!@#$%^&*()_+";
    static char letters[] = "qwertyuiop[] asdfghjkl;'` \\zxcvbnm,./";

    if ( key >= KEY_ONE && key <= KEY_EQUAL ) return shift ? upper[key - KEY_ONE] : lower[key - KEY_ONE];
    if ( key >= KEY_Q && key <= KEY_CLSBRCK ) {
        char ch = letters[key - KEY_Q];
        if ( shift && ch >= 'a' && ch <= 'z' ) ch = (char) ( ch - 'a' + 'A' );
        if ( shift && ch == '[' ) ch = '{';
        if ( shift && ch == ']' ) ch = '}';
        return ch;
    }
    if ( key >= KEY_A && key <= KEY_TILDE ) {
        char ch = letters[13 + key - KEY_A];
        if ( shift && ch >= 'a' && ch <= 'z' ) ch = (char) ( ch - 'a' + 'A' );
        if ( shift && ch == ';' ) ch = ':';
        if ( shift && ch == '\'' ) ch = '"';
        return ch;
    }
    if ( key == KEY_BSLASH ) return shift ? '|' : '\\';
    if ( key >= KEY_Z && key <= KEY_SLASH ) {
        char ch = letters[27 + key - KEY_Z];
        if ( shift && ch >= 'a' && ch <= 'z' ) ch = (char) ( ch - 'a' + 'A' );
        if ( shift && ch == ',' ) ch = '<';
        if ( shift && ch == '.' ) ch = '>';
        if ( shift && ch == '/' ) ch = '?';
        return ch;
    }
    if ( key == KEY_SPCBAR ) return ' ';
    return 0;
}

static void con_history( Console CMD_FAR *con, short direction )
{
    char input[CON_LINE];

    if ( con->history_count == 0 ) return;
    con->history_pos = (short) ( con->history_pos + direction );
    if ( con->history_pos < 0 ) {
        con->history_pos = -1;
        con->input[0] = 0;
        con->input_len = 0;
        return;
    }
    if ( con->history_pos >= con->history_count ) {
        con->history_pos = (short) ( con->history_count - 1 );
    }
    far_get( input, con->history[con->history_pos], CON_LINE );
    far_put( con->input, input, CON_LINE );
    con->input_len = (short) strlen( input );
}

static void con_complete( Console CMD_FAR *con, CmdState CMD_FAR *cmd )
{
    static char *builtins[] = {
        "bind", "clear", "cmdlist", "cvarlist", "echo", "exec", "help", "impulse", "map",
        "menu_quit", "quit",
        "set", "toggleconsole", "unbind", "unbindall", "wait", "writeconfig"
    };
    char input[CON_LINE], name[CMD_NAME], value[CMD_VALUE];
    short i, flags, len, found = 0;

    far_get( input, con->input, CON_LINE );
    len = (short) strlen( input );
    if ( len == 0 || strchr( input, ' ' ) ) return;
    for ( i = 0; i < (short) ( sizeof(builtins) / sizeof(builtins[0]) ); i++ ) {
        if ( !strncmp( builtins[i], input, len ) ) { far_put( con->input, builtins[i], CON_LINE ); found = -1; break; }
    }
    if ( !found ) for ( i = 0; cvar_get( cmd, i, name, value, &flags ); i++ ) {
        if ( !strncmp( name, input, len ) ) { far_put( con->input, name, CON_LINE ); found = -1; break; }
    }
    if ( found ) {
        far_get( input, con->input, CON_LINE );
        con->input_len = (short) strlen( input );
    }
}

static void con_submit( Console CMD_FAR *con, CmdState CMD_FAR *cmd )
{
    char input[CON_LINE], echo[CON_LINE + 2];
    short i;

    far_get( input, con->input, CON_LINE );
    echo[0] = ']';
    strcpy( echo + 1, input );
    con_print( con, echo );
    if ( input[0] ) {
        if ( con->history_count < CON_HISTORY ) con->history_count++;
        for ( i = con->history_count - 1; i > 0; i-- )
            far_copy( con->history[i], con->history[i - 1], CON_LINE );
        far_put( con->history[0], input, CON_LINE );
        if ( cmd_enqueue( cmd, input ) <= 0 ) con_print( con, "command buffer full" );
    }
    con->history_pos = -1;
    con->input[0] = 0;
    con->input_len = 0;
}

void con_key( Console CMD_FAR *con, CmdState CMD_FAR *cmd, short key, short shift )
{
    char ch;

    if ( key == KEY_ESC ) return; /* host restores the saved mouse position */
    if ( key == KEY_ENTER ) { con_submit( con, cmd ); return; }
    if ( key == KEY_BACKSPC ) {
        if ( con->input_len > 0 ) con->input[--con->input_len] = 0;
        return;
    }
    if ( key == KEY_TABK ) { con_complete( con, cmd ); return; }
    if ( key == KEY_UP ) { con_history( con, 1 ); return; }
    if ( key == KEY_DOWN ) { con_history( con, -1 ); return; }
    if ( key == KEY_PGUP ) {
        con->scroll = (short) ( con->scroll + 3 );
        if ( con->scroll >= con->line_count ) con->scroll = (short) ( con->line_count - 1 );
        if ( con->scroll < 0 ) con->scroll = 0;
        return;
    }
    if ( key == KEY_PGDW ) {
        con->scroll = (short) ( con->scroll - 3 );
        if ( con->scroll < 0 ) con->scroll = 0;
        return;
    }
    ch = con_char( key, shift );
    if ( ch && con->input_len < CON_LINE - 1 ) {
        con->input[con->input_len++] = ch;
        con->input[con->input_len] = 0;
    }
}

static void near con_gfx_check( Pak *pak )
{
    long bytes;

    pak_seek( pak, "menu.raw", &bytes );
    if ( bytes != UIGFX_FILE_BYTES ) sys_error( "menu.raw has the wrong size" );
}

static short near con_back_load( Console CMD_FAR *con, Pak *pak )
{
    short handle, y;

    if ( con->conback ) return -1;
    con_gfx_check( pak );
    con->conback = qglArNew( QGL_AR_EMS, UIGFX_CONBACK_W,
                             UIGFX_CONBACK_H, QGL_TEX_SLOT );
    if ( !con->conback ) return 0;
    handle = pak_seek_part( pak, "menu.raw", UIGFX_CONBACK_OFFSET,
                            UIGFX_CONBACK_BYTES );
    for ( y = 0; y < UIGFX_CONBACK_H; y++ )
        if ( qglFileRead( handle, qglArWin( con->conback, y ), UIGFX_CONBACK_W )
             != UIGFX_CONBACK_W )
            sys_error( "menu.raw console backdrop is short" );
    return -1;
}

static short near con_chars_load( Console CMD_FAR *con, Pak *pak )
{
    short handle, y;

    if ( con->chars ) return -1;
    con_gfx_check( pak );
    con->chars = qglSfNewEx( UIGFX_CHARS_W, UIGFX_CHARS_H,
                             UIGFX_CHARS_W, QGL_SURF_EMS );
    if ( !con->chars ) return 0;
    handle = pak_seek_part( pak, "menu.raw", UIGFX_CHARS_OFFSET,
                            UIGFX_CHARS_BYTES );
    for ( y = 0; y < UIGFX_CHARS_H; y++ )
        if ( qglFileRead( handle, qglSfWrRow( con->chars, y ), UIGFX_CHARS_W )
             != UIGFX_CHARS_W )
            sys_error( "menu.raw console font is short" );
    return -1;
}

void con_gfx_release( Console CMD_FAR *con )
{
    if ( con->conback ) qglArFree( con->conback );
    if ( con->chars ) qglSfFree( con->chars );
    con->conback = con->chars = 0;
}

short con_visible( const Console CMD_FAR *con )
{
    return (short) ( con && con->height > 0.0f );
}

static void near con_glyph( Console CMD_FAR *con, QSurf dc,
                            short x, short y, unsigned char code )
{
    unsigned char row[8];
    unsigned char far *src, far *dst;
    short gx, gy;
    short sx = (short) ( ( code & 15 ) * 8 );
    short sy = (short) ( code / 16 * 8 );

    for ( gy = 0; gy < 8; gy++ ) {
        src = (unsigned char far *) qglSfRdRow( con->chars, (short) ( sy + gy ) ) + sx;
        qglMemCopy( (long) (void far *) row, (long) src, 8L );
        dst = (unsigned char far *) qglSfWrRow( dc, (short) ( y + gy ) ) + x;
        for ( gx = 0; gx < 8; gx++ ) if ( row[gx] ) dst[gx] = row[gx];
    }
}

static void near con_text( Console CMD_FAR *con, QSurf dc,
                           short x, short y, const char *text )
{
    short i;
    for ( i = 0; text[i] && x + i * 8 < 320; i++ )
        con_glyph( con, dc, (short) ( x + i * 8 ), y, (unsigned char) text[i] );
}

static void near con_back_draw( Console CMD_FAR *con, QSurf dc, short top )
{
    short sy, dy;

    sy = top < 0 ? (short) -top : 0;
    for ( ; sy < UIGFX_CONBACK_H; sy++ ) {
        dy = (short) ( top + sy );
        if ( dy >= UIGFX_CONBACK_H ) break;
        qglMemCopy( qglSfWrRow( dc, dy ), qglArWin( con->conback, sy ),
                    UIGFX_CONBACK_W );
    }
}

void con_draw_full( Console CMD_FAR *con, Pak *pak, QSurf dc )
{
    if ( con_back_load( con, pak ) ) con_back_draw( con, dc, 0 );
}

void con_draw( Console CMD_FAR *con, Pak *pak, QSurf dc, short w, float now )
{
    char text[CON_LINE + 2];
    short panel_h, rows, i, idx, y, cols, input_len, input_start;
    char input[CON_LINE];

    if ( con->height <= 0.0f ) {
        rows = con->line_count < 4 ? con->line_count : 4;
        if ( !con_gfx_trim( con ) ) return;
        if ( !con_chars_load( con, pak ) ) return;
        y = 0;
        for ( i = rows; i > 0; i-- ) {
            idx = (short) ( ( con->line_head - i + CON_LINES ) % CON_LINES );
            if ( con->line_time[idx] <= 0.0f ) continue;
            far_get( text, con->line[idx], CON_LINE );
            con_text( con, dc, 8, y, text );
            y = (short) ( y + 8 );
        }
        return;
    }
    if ( !con_back_load( con, pak ) ) return;
    panel_h = (short) con->height;
    con_back_draw( con, dc, (short) ( panel_h - UIGFX_CONBACK_H ) );
    if ( panel_h < 16 ) return;
    if ( !con_chars_load( con, pak ) ) return;
    rows = (short) ( ( panel_h - 16 ) / 8 );
    if ( rows > con->line_count - con->scroll ) rows = (short) ( con->line_count - con->scroll );
    y = (short) ( panel_h - 16 - rows * 8 );
    for ( i = rows; i > 0; i-- ) {
        idx = (short) ( ( con->line_head - con->scroll - i + CON_LINES ) % CON_LINES );
        far_get( text, con->line[idx], CON_LINE );
        con_text( con, dc, 8, y, text );
        y = (short) ( y + 8 );
    }
    text[0] = ']';
    far_get( input, con->input, CON_LINE );
    cols = (short) ( w / 8 - 2 );
    input_len = (short) strlen( input );
    input_start = input_len > cols - 2 ? (short) ( input_len - ( cols - 2 ) ) : 0;
    strcpy( text + 1, input + input_start );
    input_len = (short) strlen( text );
    text[input_len++] = (char) ( 10 + ( (long) ( now * 4.0f ) & 1L ) );
    text[input_len] = 0;
    con_text( con, dc, 8, (short) ( panel_h - 16 ), text );
}
