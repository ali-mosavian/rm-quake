/* console.c -- the in-game command console. */

#include <string.h>

#include "console.h"
#include "input.h"
#include "qgl.h"
#include "screen.h"

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
        "bind", "clear", "cmdlist", "cvarlist", "echo", "exec", "help", "impulse", "map", "quit",
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

void con_draw( const Console CMD_FAR *con, Hud far *hud, QSurf dc, short w, short h )
{
    char text[CON_LINE + 2];
    short panel_h, rows, i, idx, y, cols, input_len, input_start;
    char input[CON_LINE];

    if ( con->height <= 0.0f ) {
        rows = con->line_count < 4 ? con->line_count : 4;
        y = 0;
        for ( i = rows; i > 0; i-- ) {
            idx = (short) ( ( con->line_head - i + CON_LINES ) % CON_LINES );
            if ( con->line_time[idx] <= 0.0f ) continue;
            far_get( text, con->line[idx], CON_LINE );
            draw_string( hud, dc, 4, y, text, hud->hc_text );
            y = (short) ( y + 8 );
        }
        return;
    }
    panel_h = (short) con->height;
    qglDrFill( dc, 0, 0, (short) ( w - 1 ), panel_h, hud->hc_bg );
    qglDrHline( dc, 0, panel_h, (short) ( w - 1 ), hud->hc_slabhi );
    rows = (short) ( ( panel_h - 12 ) / 8 );
    if ( rows > con->line_count - con->scroll ) rows = (short) ( con->line_count - con->scroll );
    y = (short) ( panel_h - 12 - rows * 8 );
    for ( i = rows; i > 0; i-- ) {
        idx = (short) ( ( con->line_head - con->scroll - i + CON_LINES ) % CON_LINES );
        far_get( text, con->line[idx], CON_LINE );
        draw_string( hud, dc, 4, y, text, hud->hc_text );
        y = (short) ( y + 8 );
    }
    text[0] = ']';
    far_get( input, con->input, CON_LINE );
    cols = (short) ( ( w - 8 ) / 4 );
    input_len = (short) strlen( input );
    input_start = input_len > cols - 2 ? (short) ( input_len - ( cols - 2 ) ) : 0;
    strcpy( text + 1, input + input_start );
    strcat( text, "_" );
    draw_string( hud, dc, 4, (short) ( panel_h - 12 ), text, hud->hc_text );
}
