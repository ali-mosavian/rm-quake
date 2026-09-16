/* cmd.c -- bounded console command buffer and cvar registry. */

#include <stdlib.h>
#include <string.h>

#include "cmd.h"

#define CMD_ARGC 8

typedef struct {
    short argc;
    char *argv[CMD_ARGC];
} CmdArgs;

static short near_text_len( const char *s, short limit )
{
    short n = 0;
    while ( n < limit && s[n] ) n++;
    return n;
}

static short far_text_eq( const char CMD_FAR *a, const char *b )
{
    while ( *a && *a == *b ) { a++; b++; }
    return (short) ( *a == 0 && *b == 0 );
}

static void far_text_put( char CMD_FAR *dst, const char *src, short size )
{
    short i = 0;
    if ( size <= 0 ) return;
    while ( i < size - 1 && src[i] ) { dst[i] = src[i]; i++; }
    dst[i] = 0;
}

static void far_text_get( char *dst, const char CMD_FAR *src, short size )
{
    short i = 0;
    if ( size <= 0 ) return;
    while ( i < size - 1 && src[i] ) { dst[i] = src[i]; i++; }
    dst[i] = 0;
}

static void text_put( char *dst, const char *src, short size )
{
    short i = 0;
    if ( size <= 0 ) return;
    while ( i < size - 1 && src[i] ) { dst[i] = src[i]; i++; }
    dst[i] = 0;
}

static short text_add( char *dst, short used, short size, const char *src )
{
    while ( *src && used < size - 1 ) dst[used++] = *src++;
    dst[used] = 0;
    return used;
}

static short cvar_find( const CmdState CMD_FAR *state, const char *name )
{
    short i;
    for ( i = 0; i < state->cvar_count; i++ )
        if ( far_text_eq( state->cvar[i].name, name ) ) return i;
    return -1;
}

void cmd_init( CmdState CMD_FAR *state )
{
    short i, j;
    unsigned char CMD_FAR *p = (unsigned char CMD_FAR *) state;
    for ( i = 0; i < (short) sizeof(*state); i++ ) p[i] = 0;
    for ( i = 0; i < CMD_CVARS; i++ )
        for ( j = 0; j < CMD_NAME; j++ ) state->cvar[i].name[j] = 0;
}

short cvar_register( CmdState CMD_FAR *state, const char *name, const char *value,
                     short flags, float min_value, float max_value )
{
    short i;
    char error[CMD_TEXT];

    if ( !name[0] || near_text_len( name, CMD_NAME ) >= CMD_NAME ) return -1;
    i = cvar_find( state, name );
    if ( i >= 0 ) return i;
    if ( state->cvar_count >= CMD_CVARS ) return -1;
    i = state->cvar_count++;
    far_text_put( state->cvar[i].name, name, CMD_NAME );
    state->cvar[i].flags = flags;
    state->cvar[i].min_value = min_value;
    state->cvar[i].max_value = max_value;
    if ( !cvar_set( state, name, value, error ) ) {
        state->cvar_count--;
        return -1;
    }
    return i;
}

short cvar_set( CmdState CMD_FAR *state, const char *name, const char *value, char *error )
{
    short i;
    char *end;
    double number;

    i = cvar_find( state, name );
    if ( i < 0 ) {
        text_put( error, "unknown variable", CMD_TEXT );
        return 0;
    }
    if ( !value[0] || near_text_len( value, CMD_VALUE ) >= CMD_VALUE ) {
        text_put( error, "invalid value", CMD_TEXT );
        return 0;
    }
    number = strtod( value, &end );
    if ( *end || number < state->cvar[i].min_value || number > state->cvar[i].max_value ) {
        text_put( error, "value out of range", CMD_TEXT );
        return 0;
    }
    far_text_put( state->cvar[i].string, value, CMD_VALUE );
    state->cvar[i].value = (float) number;
    return 1;
}

float cvar_value( const CmdState CMD_FAR *state, const char *name )
{
    short i = cvar_find( state, name );
    return i < 0 ? 0.0f : state->cvar[i].value;
}

short cvar_get( const CmdState CMD_FAR *state, short index, char *name, char *value, short *flags )
{
    if ( index < 0 || index >= state->cvar_count ) return 0;
    far_text_get( name, state->cvar[index].name, CMD_NAME );
    far_text_get( value, state->cvar[index].string, CMD_VALUE );
    if ( flags ) *flags = state->cvar[index].flags;
    return 1;
}

static short queue_one( CmdState CMD_FAR *state, const char *beg, short len )
{
    short i;
    char CMD_FAR *dst;

    while ( len > 0 && ( *beg == ' ' || *beg == '\t' ) ) { beg++; len--; }
    while ( len > 0 && ( beg[len - 1] == ' ' || beg[len - 1] == '\t' ) ) len--;
    if ( len == 0 ) return 0;
    if ( len >= CMD_TEXT ) return -1;
    if ( state->queue_count >= CMD_QUEUE ) return -2;
    dst = state->queue[state->queue_head];
    for ( i = 0; i < len; i++ ) dst[i] = beg[i];
    dst[len] = 0;
    state->queue_head = (short) ( ( state->queue_head + 1 ) % CMD_QUEUE );
    state->queue_count++;
    return 1;
}

short cmd_enqueue( CmdState CMD_FAR *state, const char *text )
{
    const char *beg = text;
    const char *p = text;
    short quoted = 0, added = 0, needed = 0, r;

    if ( near_text_len( text, CMD_TEXT ) >= CMD_TEXT ) return -1;
    for ( ; ; p++ ) {
        if ( *p == '"' ) quoted = !quoted;
        if ( ( *p == ';' && !quoted ) || *p == 0 ) {
            const char *trim = beg;
            short len = (short) ( p - beg );
            while ( len > 0 && ( *trim == ' ' || *trim == '\t' ) ) { trim++; len--; }
            while ( len > 0 && ( trim[len - 1] == ' ' || trim[len - 1] == '\t' ) ) len--;
            if ( len >= CMD_TEXT ) return -1;
            if ( len > 0 ) needed++;
            if ( *p == 0 ) break;
            beg = p + 1;
        }
    }
    if ( needed > CMD_QUEUE - state->queue_count ) return -2;

    beg = text;
    p = text;
    quoted = 0;
    for ( ; ; p++ ) {
        if ( *p == '"' ) quoted = !quoted;
        if ( ( *p == ';' && !quoted ) || *p == 0 ) {
            r = queue_one( state, beg, (short) ( p - beg ) );
            if ( r == -1 ) return -1;
            if ( r == -2 ) return -2;
            added = (short) ( added + r );
            if ( *p == 0 ) return added;
            beg = p + 1;
        }
    }
}

static void cmd_tokenize( char *line, CmdArgs *args )
{
    char *p = line;
    char *out;

    args->argc = 0;
    while ( *p && args->argc < CMD_ARGC ) {
        while ( *p == ' ' || *p == '\t' ) p++;
        if ( !*p ) break;
        out = p;
        args->argv[args->argc++] = out;
        if ( *p == '"' ) {
            p++; out = args->argv[args->argc - 1] = p;
            while ( *p && *p != '"' ) *out++ = *p++;
            if ( *p == '"' ) p++;
            *out = 0;
        } else {
            while ( *p && *p != ' ' && *p != '\t' ) p++;
            if ( *p ) *p++ = 0;
        }
    }
}

static void cvar_result( const CmdState CMD_FAR *state, short i, CmdResult *result )
{
    short used = 0;
    char name[CMD_NAME], value[CMD_VALUE];
    far_text_get( name, state->cvar[i].name, CMD_NAME );
    far_text_get( value, state->cvar[i].string, CMD_VALUE );
    used = text_add( result->text, used, CMD_TEXT, "\"" );
    used = text_add( result->text, used, CMD_TEXT, name );
    used = text_add( result->text, used, CMD_TEXT, "\" is \"" );
    used = text_add( result->text, used, CMD_TEXT, value );
    text_add( result->text, used, CMD_TEXT, "\"" );
}

CmdKind cmd_run( CmdState CMD_FAR *state, CmdResult *result )
{
    char line[CMD_TEXT];
    CmdArgs args;
    short i, used;

    result->name[0] = result->text[0] = 0;
    if ( state->queue_count == 0 ) return CMD_NONE;
    far_text_get( line, state->queue[state->queue_tail], CMD_TEXT );
    state->queue_tail = (short) ( ( state->queue_tail + 1 ) % CMD_QUEUE );
    state->queue_count--;
    cmd_tokenize( line, &args );
    if ( args.argc == 0 ) return CMD_NONE;

    if ( !strcmp( args.argv[0], "echo" ) ) {
        used = 0;
        for ( i = 1; i < args.argc; i++ ) {
            if ( i > 1 ) used = text_add( result->text, used, CMD_TEXT, " " );
            used = text_add( result->text, used, CMD_TEXT, args.argv[i] );
        }
        return CMD_PRINT;
    }
    if ( !strcmp( args.argv[0], "clear" ) ) return CMD_CLEAR;
    if ( !strcmp( args.argv[0], "cvarlist" ) ) return CMD_CVARLIST;
    if ( !strcmp( args.argv[0], "cmdlist" ) ) return CMD_CMDLIST;
    if ( !strcmp( args.argv[0], "wait" ) ) return CMD_WAIT;

    if ( !strcmp( args.argv[0], "set" ) ) {
        if ( args.argc != 3 ) {
            text_put( result->text, "usage: set <variable> <value>", CMD_TEXT );
            return CMD_ERROR;
        }
        i = cvar_find( state, args.argv[1] );
        if ( i < 0 || !cvar_set( state, args.argv[1], args.argv[2], result->text ) ) return CMD_ERROR;
        cvar_result( state, i, result );
        return CMD_CVAR;
    }

    i = cvar_find( state, args.argv[0] );
    if ( i >= 0 ) {
        if ( args.argc > 2 ) {
            text_put( result->text, "usage: <variable> [value]", CMD_TEXT );
            return CMD_ERROR;
        }
        if ( args.argc == 2 && !cvar_set( state, args.argv[0], args.argv[1], result->text ) )
            return CMD_ERROR;
        cvar_result( state, i, result );
        return CMD_CVAR;
    }

    text_put( result->name, args.argv[0], CMD_NAME );
    text_put( result->text, args.argc > 1 ? args.argv[1] : "", CMD_TEXT );
    return CMD_EXTERNAL;
}

CmdKind cmd_run_cvar( CmdState CMD_FAR *state, CmdResult *result )
{
    char line[CMD_TEXT];
    CmdArgs args;

    if ( state->queue_count == 0 ) return CMD_NONE;
    far_text_get( line, state->queue[state->queue_tail], CMD_TEXT );
    cmd_tokenize( line, &args );
    if ( args.argc == 0 ) return CMD_NONE;
    if ( cvar_find( state, args.argv[0] ) >= 0 ) return cmd_run( state, result );
    if ( !strcmp( args.argv[0], "set" ) && args.argc >= 2 &&
         cvar_find( state, args.argv[1] ) >= 0 )
        return cmd_run( state, result );
    return CMD_NONE;
}
