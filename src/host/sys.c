/*
 * sys.c -- see sys.h.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>  /* offsetof */
#include <mem.h>     /* _fmemcpy */

#include "sys.h"
#include "qgl.h"

/* Borland/tc201 both provide this from their own C startup regardless
   of which C0*.OBJ is linked -- see sys.h's own note on why that
   matters here. Not declared through <dos.h>/<stdlib.h>: mgl ships
   its own "dos.h" (qglMemAlloc/qglMemCopy) that bcc.sh's include order
   resolves first, so the real one never gets included. */
extern unsigned _psp;

void sys_error( char *msg )
{
    FILE *errf = fopen( "error.log", "w" );
    if ( errf ) { fprintf( errf, "%s\n", msg ); fclose( errf ); }

    qglDspShutdown();
    qglTmrShutdown();
    qglKbdShutdown();
    qglMouseShutdown();
    qglVgaShutdown();
    qglMemShutdown();

    fprintf( stderr, "Error: %s\n", msg );
    exit( 1 );
}

#define MAX_ARGS 32     /* tokens past this are dropped, -ticks with them */
#define CMD_MAX  128    /* the PSP tail is at most 127 characters */

/* The flags that set one RunArgs field to -1. Names live in the table,
   in far data: a table of `char *` would put every name in DGROUP. */
typedef struct {
    char     name[11];
    unsigned ofs;
} ArgFlag;

static const ArgFlag far arg_flags[] = {
    { "-walk",      offsetof( RunArgs, walk ) },
    { "-jump",      offsetof( RunArgs, jump ) },
    { "-strafe",    offsetof( RunArgs, strafe ) },
    { "-noents",    offsetof( RunArgs, no_ents ) },
    { "-noitems",   offsetof( RunArgs, no_items ) },
    { "-nomdl",     offsetof( RunArgs, no_mdl ) },
    { "-noai",      offsetof( RunArgs, no_ai ) },
    { "-fire",      offsetof( RunArgs, fire ) },
    { "-carry",     offsetof( RunArgs, carry ) },
    { "-noview",    offsetof( RunArgs, no_view ) },
    { "-nosound",   offsetof( RunArgs, no_sound ) },
    { "-sndsum",    offsetof( RunArgs, sndsum ) },
    { "-mdlsum",    offsetof( RunArgs, mdlsum ) },
    { "-badorder",  offsetof( RunArgs, bad_order ) },
    { "-entinsert", offsetof( RunArgs, ent_insert ) },
    { "-noz",       offsetof( RunArgs, no_z ) },
    { "-nocull",    offsetof( RunArgs, no_cull ) },
    { "-nomip",     offsetof( RunArgs, no_mip ) },
    { "-noportal",  offsetof( RunArgs, no_portal ) },
    { "-nosubvis",  offsetof( RunArgs, no_subvis ) },
    { "-nolcache",  offsetof( RunArgs, no_lcache ) },
    { "-nodlight",  offsetof( RunArgs, no_dlight ) },
    { "-nostyles",  offsetof( RunArgs, no_styles ) },
    { "-comp",      offsetof( RunArgs, comp ) },
    { "-ptwire",    offsetof( RunArgs, ptwire ) },
    { "-nostats",   offsetof( RunArgs, no_stats ) },
    { "-stats",     offsetof( RunArgs, stats ) },
    { "-nodraw",    offsetof( RunArgs, no_draw ) },
    { "-lm",        offsetof( RunArgs, use_lm ) }
};

#define ARG_FLAGS ( sizeof(arg_flags) / sizeof(arg_flags[0]) )

/* The flags that take values: each handler reads exactly nval tokens. */
typedef void (near *ArgTake)( RunArgs *args, char **val );

typedef struct {
    char    name[8];
    short   nval;
    ArgTake take;
} ArgValued;

static void near take_at( RunArgs *args, char **val )
{
    args->at_x = (float) atof( val[0] );
    args->at_y = (float) atof( val[1] );
    args->at_z = (float) atof( val[2] );
    args->at_set = -1;
}

static void near take_warp( RunArgs *args, char **val )
{
    short w = args->nwarp;
    if ( w >= 4 ) return;
    args->warp_tick[w]   = atol( val[0] );
    args->warp_xyz[w][0] = (float) atof( val[1] );
    args->warp_xyz[w][1] = (float) atof( val[2] );
    args->warp_xyz[w][2] = (float) atof( val[3] );
    args->nwarp++;
}

static void near take_yaw( RunArgs *args, char **val )
{
    args->yaw = (float) atof( val[0] );
    while ( args->yaw < 0.0f ) args->yaw += 360.0f;
    args->yaw_set = -1;
}

static void near take_pitch( RunArgs *args, char **val )
{
    args->pitch = (float) atof( val[0] );
    if ( args->pitch < 0.0f )   args->pitch = 0.0f;
    if ( args->pitch > 180.0f ) args->pitch = 180.0f;
    args->pitch_set = -1;
}

static void near take_ticks( RunArgs *args, char **val )
{
    args->bench_ticks = atol( val[0] );
}

static void near take_record( RunArgs *args, char **val )
{
    strncpy( args->record_name, val[0], sizeof(args->record_name) - 1 );
}

static void near take_play( RunArgs *args, char **val )
{
    strncpy( args->play_name, val[0], sizeof(args->play_name) - 1 );
}

static void near take_skill( RunArgs *args, char **val )
{
    args->skill = (short) atoi( val[0] );
}

static void near take_sflags( RunArgs *args, char **val )
{
    args->sflags = (short) atoi( val[0] );
}

static const ArgValued far arg_valued[] = {
    { "-at",     3, take_at },
    { "-warp",   4, take_warp },
    { "-yaw",    1, take_yaw },
    { "-pitch",  1, take_pitch },
    { "-ticks",  1, take_ticks },
    { "-record", 1, take_record },
    { "-play",   1, take_play },
    { "-skill",  1, take_skill },
    { "-sflags", 1, take_sflags }
};

#define ARG_VALUED ( sizeof(arg_valued) / sizeof(arg_valued[0]) )

/* Splits buf in place on blanks; returns the token count. */
static int near sys_tokenize( char *buf, unsigned len, char **argv )
{
    int argc = 0, in_tok = 0;
    unsigned i;

    for ( i = 0; i < len && argc < MAX_ARGS; i++ ) {
        if ( buf[i] == ' ' || buf[i] == '\t' ) {
            buf[i] = '\0';
            in_tok = 0;
        } else if ( !in_tok ) {
            argv[argc++] = &buf[i];
            in_tok = 1;
        }
    }
    return argc;
}

static void near sys_usage( void )
{
    printf( "Usage: qcport mapname.qmp [-ticks N]\n" );
    printf( "  -ticks N      run N simulation ticks, then write bench.bmp/bench.txt, exit\n" );
    printf( "  -lm           composite lightmaps via the surface cache\n" );
    printf( "  -nosubvis     walk subtrees with no visible leaf, as before the skip\n" );
    printf( "  -nolcache     re-find each entity's leaves every frame, as before the cache\n" );
    exit( 0 );
}

static int near arg_flag( RunArgs *args, const char *tok )
{
    unsigned k;
    for ( k = 0; k < ARG_FLAGS; k++ ) {
        if ( _fstricmp( arg_flags[k].name, tok ) != 0 ) continue;
        *(short *) ( (char *) args + arg_flags[k].ofs ) = -1;
        return 1;
    }
    return 0;
}

/* Returns the values taken. A flag short of its values is ignored, as an
   unrecognised one is. */
static int near arg_values( RunArgs *args, const char *tok, char **rest, int nrest )
{
    unsigned k;
    for ( k = 0; k < ARG_VALUED; k++ ) {
        if ( _fstricmp( arg_valued[k].name, tok ) != 0 ) continue;
        if ( nrest < arg_valued[k].nval ) return 0;
        arg_valued[k].take( args, rest );
        return arg_valued[k].nval;
    }
    return 0;
}

void sys_parse_args( RunArgs *args )
{
    char  cmd[CMD_MAX];
    char *argv[MAX_ARGS];
    unsigned char far *tail;
    unsigned len;
    int argc, i;

    memset( args, 0, sizeof(*args) );

    /* PSP:0x80 is the DOS command tail -- a length byte, then that
       many raw characters, no null terminator of its own. */
    tail = (unsigned char far *) ( ( (unsigned long) _psp << 16 ) | 0x80L );
    len = tail[0];
    if ( len > CMD_MAX - 1 ) len = CMD_MAX - 1;
    _fmemcpy( cmd, (char far *) ( tail + 1 ), len );
    cmd[len] = '\0';

    argc = sys_tokenize( cmd, len, argv );
    if ( argc < 1 ) sys_usage();

    strncpy( args->map_name, argv[0], sizeof(args->map_name) - 1 );

    /* unrecognised flags are ignored, as they always were */
    for ( i = 1; i < argc; i++ ) {
        if ( arg_flag( args, argv[i] ) ) continue;
        i += arg_values( args, argv[i], argv + i + 1, argc - i - 1 );
    }

    /* -nodraw rasterises no overlay either */
    if ( args->no_draw ) args->no_stats = -1;
}
