/* keybind.c -- Quake-style key names, bindings and logical button state. */

#include <ctype.h>
#include <string.h>

#include "qgltypes.h"
#include "keybind.h"

#define KB_MOUSE1 128
#define KB_MOUSE2 129
#define KB_MOUSE3 130

typedef struct { char *name; short key; } KeyName;

static KeyName key_names[] = {
    { "ESCAPE", KEY_ESC }, { "TAB", KEY_TABK }, { "ENTER", KEY_ENTER },
    { "SPACE", KEY_SPCBAR }, { "BACKSPACE", KEY_BACKSPC },
    { "UPARROW", KEY_UP }, { "DOWNARROW", KEY_DOWN },
    { "LEFTARROW", KEY_LEFT }, { "RIGHTARROW", KEY_RIGHT },
    { "ALT", KEY_ALT }, { "CTRL", KEY_CTRL }, { "SHIFT", KEY_LSHIFT },
    { "F1", KEY_F1 }, { "F2", KEY_F2 }, { "F3", KEY_F3 }, { "F4", KEY_F4 },
    { "F5", KEY_F5 }, { "F6", KEY_F6 }, { "F7", KEY_F7 }, { "F8", KEY_F8 },
    { "F9", KEY_F9 }, { "F10", KEY_F10 }, { "F11", KEY_F11 }, { "F12", KEY_F12 },
    { "INS", KEY_INS }, { "DEL", KEY_DEL }, { "PGDN", KEY_PGDW },
    { "PGUP", KEY_PGUP }, { "HOME", KEY_HOME }, { "END", KEY_ENDK },
    { "SEMICOLON", KEY_SEMICOL }, { "MOUSE1", KB_MOUSE1 },
    { "MOUSE2", KB_MOUSE2 }, { "MOUSE3", KB_MOUSE3 }
};

static short action_for( const char KB_FAR *command );

static void far_put( char KB_FAR *dst, const char *src, short size )
{
    short i = 0;
    while ( i < size - 1 && src[i] ) { dst[i] = src[i]; i++; }
    dst[i] = 0;
}

static void far_get( char *dst, const char KB_FAR *src, short size )
{
    short i = 0;
    while ( i < size - 1 && src[i] ) { dst[i] = src[i]; i++; }
    dst[i] = 0;
}

static short text_eq( const char KB_FAR *a, const char *b )
{
    while ( *a && *a == *b ) { a++; b++; }
    return (short) ( !*a && !*b );
}

static short text_ieq( const char *a, const char *b )
{
    while ( *a && tolower( *a ) == tolower( *b ) ) { a++; b++; }
    return (short) ( !*a && !*b );
}

static short char_key( int ch )
{
    static char *row1 = "1234567890-=";
    static char *row2 = "qwertyuiop[]";
    static char *row3 = "asdfghjkl;'`";
    static char *row4 = "zxcvbnm,./";
    char *p;

    ch = tolower( ch );
    if ( ( p = strchr( row1, ch ) ) != 0 ) return (short) ( KEY_ONE + p - row1 );
    if ( ( p = strchr( row2, ch ) ) != 0 ) return (short) ( KEY_Q + p - row2 );
    if ( ( p = strchr( row3, ch ) ) != 0 ) return (short) ( KEY_A + p - row3 );
    if ( ( p = strchr( row4, ch ) ) != 0 ) return (short) ( KEY_Z + p - row4 );
    if ( ch == '\\' ) return KEY_BSLASH;
    return -1;
}

short keybind_keynum( const char *name )
{
    short i;
    if ( name[0] && !name[1] ) return char_key( name[0] );
    for ( i = 0; i < (short) ( sizeof(key_names) / sizeof(key_names[0]) ); i++ )
        if ( text_ieq( name, key_names[i].name ) ) return key_names[i].key;
    return -1;
}

static short action_for( const char KB_FAR *command )
{
    static char *names[KA_COUNT] = {
        "+forward", "+back", "+moveleft", "+moveright", "+jump", "+attack",
        "+showscores"
    };
    short i;
    for ( i = 0; i < KA_COUNT; i++ ) if ( text_eq( command, names[i] ) ) return i;
    return -1;
}

static short binding_action( const char *command )
{
    char button[16];
    short i = 0;
    while ( command[i] && command[i] != ';' && command[i] != ' ' &&
            command[i] != '\t' && i < (short) sizeof(button) - 1 ) {
        button[i] = command[i];
        i++;
    }
    button[i] = 0;
    return action_for( button );
}

static void release_key( KeyBindings KB_FAR *keys, short key )
{
    short action;
    if ( key < 0 || key >= KB_KEYS ) return;
    action = (short) keys->owner[key] - 1;
    if ( action >= 0 && action < KA_COUNT && keys->held[action] ) keys->held[action]--;
    keys->owner[key] = 0;
}

void keybind_init( KeyBindings KB_FAR *keys )
{
    unsigned char KB_FAR *p = (unsigned char KB_FAR *) keys;
    unsigned i;
    for ( i = 0; i < sizeof(*keys); i++ ) p[i] = 0;
}

short keybind_set( KeyBindings KB_FAR *keys, const char *name, const char *command,
                   char *error )
{
    short key = keybind_keynum( name );
    short action;
    if ( key < 0 ) { strcpy( error, "unknown key" ); return 0; }
    if ( strlen( command ) >= KB_TEXT ) { strcpy( error, "binding is too long" ); return 0; }
    if ( command[0] == '+' && binding_action( command ) < 0 ) {
        strcpy( error, "unsupported button command" );
        return 0;
    }
    action = (short) keys->owner[key] - 1;
    release_key( keys, key );
    if ( action >= 0 && action < KA_COUNT && !keys->held[action] ) keys->pressed[action] = 0;
    far_put( keys->binding[key], command, KB_TEXT );
    return -1;
}

short keybind_get( const KeyBindings KB_FAR *keys, const char *name, char *command )
{
    short key = keybind_keynum( name );
    if ( key < 0 ) return 0;
    far_get( command, keys->binding[key], KB_TEXT );
    return -1;
}

void keybind_unbind_all( KeyBindings KB_FAR *keys )
{
    short i;
    keybind_release_all( keys );
    for ( i = 0; i < KB_KEYS; i++ ) keys->binding[i][0] = 0;
}

void keybind_event( KeyBindings KB_FAR *keys, CmdState CMD_FAR *cmd, short key, short down )
{
    short action;
    char command[KB_TEXT], *tail;

    if ( key < 0 || key >= KB_KEYS ) return;
    if ( !down ) { release_key( keys, key ); return; }
    if ( keys->owner[key] ) return;
    far_get( command, keys->binding[key], KB_TEXT );
    action = binding_action( command );
    if ( action >= 0 ) {
        keys->owner[key] = (signed char) ( action + 1 );
        if ( keys->held[action] < 255 ) keys->held[action]++;
        keys->pressed[action] = 1;
        tail = strchr( command, ';' );
        if ( tail && tail[1] ) cmd_enqueue( cmd, tail + 1 );
        return;
    }
    if ( command[0] ) cmd_enqueue( cmd, command );
}

void keybind_mouse( KeyBindings KB_FAR *keys, CmdState CMD_FAR *cmd,
                     short left, short right, short middle )
{
    short state[3], i;
    state[0] = left; state[1] = right; state[2] = middle;
    for ( i = 0; i < 3; i++ ) if ( !!state[i] != !!keys->mouse[i] ) {
        keys->mouse[i] = (unsigned char) ( state[i] ? 1 : 0 );
        keybind_event( keys, cmd, (short) ( KB_MOUSE1 + i ), state[i] );
    }
}

void keybind_release_all( KeyBindings KB_FAR *keys )
{
    short i;
    for ( i = 0; i < KB_KEYS; i++ ) release_key( keys, i );
    for ( i = 0; i < KA_COUNT; i++ )
        keys->pressed[i] = keys->forced[i] = keys->manual[i] = 0;
    keys->mouse[0] = keys->mouse[1] = keys->mouse[2] = 0;
}

short keybind_action( const KeyBindings KB_FAR *keys, KeyAction action )
{
    if ( action < 0 || action >= KA_COUNT ) return 0;
    return (short) ( keys->held[action] || keys->pressed[action] ||
                     keys->forced[action] || keys->manual[action] );
}

short keybind_record_state( const KeyBindings KB_FAR *keys, KeyAction action )
{
    short state = 0;
    if ( action < 0 || action >= KA_COUNT ) return 0;
    if ( keys->held[action] || keys->forced[action] || keys->manual[action] ) state |= 1;
    if ( keys->pressed[action] ) state |= 2;
    return state;
}

void keybind_replay_state( KeyBindings KB_FAR *keys, KeyAction action, short state )
{
    if ( action < 0 || action >= KA_COUNT ) return;
    keys->forced[action] = (unsigned char) ( state & 1 );
    keys->pressed[action] = (unsigned char) ( ( state & 2 ) != 0 );
}

void keybind_force( KeyBindings KB_FAR *keys, KeyAction action, short down )
{
    if ( action >= 0 && action < KA_COUNT ) keys->forced[action] = (unsigned char) ( down ? 1 : 0 );
}

short keybind_command( KeyBindings KB_FAR *keys, const char *command, short down )
{
    char button[16];
    short action;
    if ( command[0] == '-' ) {
        button[0] = '+';
        strncpy( button + 1, command + 1, sizeof(button) - 2 );
        button[sizeof(button) - 1] = 0;
        action = action_for( button );
    } else action = action_for( command );
    if ( action < 0 ) return 0;
    keys->manual[action] = (unsigned char) ( down ? 1 : 0 );
    return -1;
}

void keybind_tick( KeyBindings KB_FAR *keys )
{
    short i;
    for ( i = 0; i < KA_COUNT; i++ ) keys->pressed[i] = 0;
}

static short key_name( short key, char *name )
{
    short i;
    char *ch;
    static char *printable = "1234567890-=qwertyuiop[]asdfghjkl;'`\\zxcvbnm,./";
    for ( ch = printable; *ch; ch++ ) if ( char_key( *ch ) == key ) {
        name[0] = *ch; name[1] = 0; return -1;
    }
    for ( i = 0; i < (short) ( sizeof(key_names) / sizeof(key_names[0]) ); i++ )
        if ( key_names[i].key == key ) { strcpy( name, key_names[i].name ); return -1; }
    return 0;
}

short keybind_write( const KeyBindings KB_FAR *keys, FILE *file )
{
    short i;
    char name[16], command[KB_TEXT];
    if ( fprintf( file, "unbindall\n" ) < 0 ) return 0;
    for ( i = 0; i < KB_KEYS; i++ ) {
        if ( !keys->binding[i][0] || !key_name( i, name ) ) continue;
        far_get( command, keys->binding[i], KB_TEXT );
        if ( fprintf( file, "bind \"%s\" \"%s\"\n", name, command ) < 0 ) return 0;
    }
    return -1;
}

static short side_name( char *out, short size, const char *name, const char *ext )
{
    short i = 0, base = 0, dot = -1;
    while ( name[i] && i < size - 1 ) {
        out[i] = name[i];
        if ( name[i] == '/' || name[i] == '\\' ) { base = (short) ( i + 1 ); dot = -1; }
        else if ( name[i] == '.' && i >= base ) dot = i;
        i++;
    }
    if ( name[i] ) return 0;
    if ( dot < 0 ) dot = i;
    i = dot;
    while ( *ext && i < size - 1 ) out[i++] = *ext++;
    if ( *ext ) return 0;
    out[i] = 0;
    return -1;
}

short keybind_config_paths( char *temp, char *backup, short size, const char *name )
{
    if ( !name[0] || !side_name( temp, size, name, ".$$$" ) ||
         !side_name( backup, size, name, ".bak" ) ) return 0;
    if ( text_ieq( name, temp ) && !side_name( temp, size, name, ".tmp" ) ) return 0;
    if ( text_ieq( name, backup ) && !side_name( backup, size, name, ".old" ) ) return 0;
    if ( text_ieq( name, temp ) || text_ieq( name, backup ) || text_ieq( temp, backup ) ) return 0;
    return -1;
}

short keybind_write_config( const KeyBindings KB_FAR *keys,
                            const CmdState CMD_FAR *cmd, const char *name )
{
    FILE *file;
    char temp[68], backup[68];
    short had_old, ok;
    if ( !keybind_config_paths( temp, backup, sizeof(temp), name ) ) return 0;
    file = fopen( temp, "wt" );
    if ( !file ) return 0;
    ok = (short) ( keybind_write( keys, file ) && cmd_write_cvars( cmd, file ) );
    if ( fflush( file ) || ferror( file ) ) ok = 0;
    if ( fclose( file ) ) ok = 0;
    if ( !ok ) { remove( temp ); return 0; }

    remove( backup );
    had_old = (short) ( rename( name, backup ) == 0 );
    if ( rename( temp, name ) != 0 ) {
        if ( had_old ) rename( backup, name );
        remove( temp );
        return 0;
    }
    if ( had_old ) remove( backup );
    return -1;
}
