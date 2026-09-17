/* sbar.c -- id's single-player status and inventory bars. */

#include <stdio.h>
#include <string.h>

#include "sbar.h"
#include "qgl.h"
#include "pak.h"
#include "pl_move.h"
#include "sys.h"

#define CELL             24
#define CELL_NORMAL       0
#define CELL_RED         11
#define CELL_AMMO        22
#define CELL_ARMOR       26
#define CELL_FACE        29
#define CELL_FACE_INVIS  39
#define CELL_FACE_PENT   40
#define CELL_FACE_BOTH   41
#define CELL_FACE_QUAD   42
#define CELL_DISC        43

#define WEAPON_FRAMES     7
#define ITEM_KEY1         0
#define ITEM_KEY2        16
#define ITEM_INVIS       32
#define ITEM_PENT        48
#define ITEM_SUIT        64
#define ITEM_QUAD        80
#define ITEM_SIGIL       96

typedef struct {
    long items;
    short health, armor, armor_kind, weapon;
    short shells, nails, rockets, cells, serverflags;
    short power, face, score;
    short kills, monsters, secrets, secret_total;
    short lines;
    short weapon_frame[6];
    long seconds;
} SbarState;

static QSurf sbar_atlas, sbar_view24, sbar_view48;
static SbarState sbar_last;
static short sbar_valid;

static void near far_text( char *dst, const char far *src, short size );

void scr_sbar_load( Pak *pak )
{
    short handle, y;
    long bytes;

    sbar_atlas = qglSfNew( SBARC_EMS_W, SBARC_EMS_H, QGL_SURF_EMS );
    if ( !sbar_atlas ) sys_error( "0x3003, no memory for the status bar" );
    sbar_view48 = qglSfViewNew( sbar_atlas, SBARC_W, SBARC_H, SBARC_EMS_W );
    sbar_view24 = qglSfViewNew( sbar_atlas, SBARC_W, 24, SBARC_EMS_W );
    if ( !sbar_view48 || !sbar_view24 ) sys_error( "0x3007, no view for the status bar" );
    if ( !qglSfViewAim( sbar_view48, (long) SBARC_WORK_Y * SBARC_EMS_W ) ||
         !qglSfViewAim( sbar_view24, (long) ( SBARC_WORK_Y + 24 ) * SBARC_EMS_W ) )
        sys_error( "0x3008, the status bar view would not aim" );

    handle = pak_seek( pak, "sbar.raw", &bytes );
    if ( bytes != (long) SBARC_EMS_W * SBARC_EMS_H )
        sys_error( "0x3004, sbar.raw has the wrong size" );
    for ( y = 0; y < SBARC_EMS_H; y++ )
        qglFileRead( handle, qglSfWrRow( sbar_atlas, y ), SBARC_EMS_W );
    sbar_valid = 0;
}

void scr_sbar_shutdown( void )
{
    if ( sbar_view24 ) qglDelView( sbar_view24 );
    if ( sbar_view48 ) qglDelView( sbar_view48 );
    if ( sbar_atlas ) qglSfFree( sbar_atlas );
    sbar_view24 = sbar_view48 = sbar_atlas = 0;
    sbar_valid = 0;
}

static void near sbar_copy_bar( short src_y, short dst_y )
{
    short y;
    for ( y = 0; y < 24; y++ )
        qglMemCopy( qglSfWrRow( sbar_atlas, (short) ( dst_y + y ) ),
                    qglSfRdRow( sbar_atlas, (short) ( src_y + y ) ), SBARC_W );
}

static void near sbar_pic( short sx, short sy, short sw, short sh,
                           short dx, short dy )
{
    unsigned char far *src, far *dst;
    short y, x;

    for ( y = 0; y < sh; y++ ) {
        src = (unsigned char far *) qglSfRdRow( sbar_atlas, (short) ( sy + y ) ) + sx;
        dst = (unsigned char far *) qglSfWrRow( sbar_atlas,
                                                (short) ( SBARC_WORK_Y + dy + y ) ) + dx;
        for ( x = 0; x < sw; x++ ) if ( src[x] != 255 ) dst[x] = src[x];
    }
}

static void near sbar_cell( short index, short x, short y )
{
    sbar_pic( (short) ( index % SBARC_CELL_COLS * CELL ),
               (short) ( SBARC_CELL_Y + index / SBARC_CELL_COLS * CELL ),
               CELL, CELL, x, y );
}

static void near sbar_weapon( short index, short x )
{
    sbar_pic( (short) ( index % SBARC_WEAPON_COLS * 24 ),
               (short) ( SBARC_WEAPON_Y + index / SBARC_WEAPON_COLS * 16 ),
               24, 16, x, 8 );
}

static void near sbar_item( short sx, short width, short x )
{
    sbar_pic( sx, SBARC_ITEM_Y, width, 16, x, 8 );
}

static void near sbar_char( unsigned char code, short x, short y )
{
    sbar_pic( (short) ( code % 64 * 8 ),
              (short) ( SBARC_CHARS_Y + code / 64 * 8 ), 8, 8, x, y );
}

static void near sbar_small_num( short x, short value )
{
    char text[8];
    short i, n;

    sprintf( text, "%3d", (int) value );
    n = (short) strlen( text );
    if ( n > 3 ) { memmove( text, text + n - 3, 4 ); n = 3; }
    for ( i = 0; i < n; i++ )
        if ( text[i] >= '0' && text[i] <= '9' )
            sbar_char( (unsigned char) ( 18 + text[i] - '0' ),
                       (short) ( x + i * 8 ), 0 );
}

static void near sbar_num( short x, short y, short value, short red )
{
    char text[8], *p;
    short i, n, base = red ? CELL_RED : CELL_NORMAL;

    sprintf( text, "%d", (int) value );
    n = (short) strlen( text );
    p = text;
    if ( n > 3 ) { p += n - 3; n = 3; }
    x = (short) ( x + ( 3 - n ) * CELL );
    for ( i = 0; i < n; i++ ) {
        short digit = p[i] == '-' ? 10 : (short) ( p[i] - '0' );
        sbar_cell( (short) ( base + digit ), (short) ( x + i * CELL ), y );
    }
}

static short near armor_kind( const Fight *fight )
{
    if ( fight->armor_type >= PL_ARMOR3_TYPE ) return 2;
    if ( fight->armor_type >= PL_ARMOR2_TYPE ) return 1;
    return 0;
}

static short near power_bits( const Fight *fight, float now )
{
    return (short) ( ( now < fight->invis_until ? 1 : 0 ) |
                     ( now < fight->pent_until  ? 2 : 0 ) |
                     ( now < fight->suit_until  ? 4 : 0 ) |
                     ( now < fight->quad_until  ? 8 : 0 ) );
}

static short near face_cell( const Fight *fight, float now, short power )
{
    short tier;
    if ( ( power & 3 ) == 3 ) return CELL_FACE_BOTH;
    if ( power & 8 ) return CELL_FACE_QUAD;
    if ( power & 1 ) return CELL_FACE_INVIS;
    if ( power & 2 ) return CELL_FACE_PENT;
    tier = fight->health >= 100 ? 0 : (short) ( 4 - fight->health / 20 );
    if ( tier < 0 ) tier = 0;
    if ( tier > 4 ) tier = 4;
    return (short) ( CELL_FACE + tier * 2 + ( now <= fight->face_until ? 1 : 0 ) );
}

static void near sbar_state( SbarState *state, const World *world,
                             const Fight *fight, float now,
                             short showscores, short lines )
{
    static short bits[6] = { PL_IT_SHOTGUN, PL_IT_SSG, PL_IT_NAILGUN,
                              PL_IT_SNG, PL_IT_GL, PL_IT_RL };
    short i, frame, elapsed;

    memset( state, 0, sizeof(*state) );
    state->items = fight->items;
    state->health = fight->health;
    state->armor = fight->armor;
    state->armor_kind = armor_kind( fight );
    state->weapon = fight->weapon;
    state->shells = fight->shells;
    state->nails = fight->nails;
    state->rockets = fight->rockets;
    state->cells = fight->cells;
    state->serverflags = fight->serverflags;
    state->power = power_bits( fight, now );
    state->face = face_cell( fight, now, state->power );
    state->score = (short) ( showscores || fight->health <= 0 );
    state->kills = fight->kills;
    state->monsters = world->mon_count;
    state->secrets = fight->secrets;
    state->secret_total = fight->secret_total;
    state->seconds = (long) ( now - fight->level_start );
    state->lines = lines;
    for ( i = 0; i < 6; i++ ) {
        frame = fight->weapon == bits[i] ? 1 : 0;
        if ( fight->weapon_at[i] >= 0.0f && now - fight->weapon_at[i] < 1.0f ) {
            elapsed = (short) ( ( now - fight->weapon_at[i] ) * 10.0f );
            if ( elapsed < 0 ) elapsed = 0;
            frame = (short) ( elapsed % 5 + 2 );
        }
        state->weapon_frame[i] = frame;
    }
}

static short near current_ammo( const Fight *fight )
{
    if ( fight->weapon == PL_IT_NAILGUN || fight->weapon == PL_IT_SNG ) return fight->nails;
    if ( fight->weapon == PL_IT_GL || fight->weapon == PL_IT_RL ) return fight->rockets;
    return fight->shells;
}

static short near current_ammo_icon( const Fight *fight )
{
    if ( fight->weapon == PL_IT_NAILGUN || fight->weapon == PL_IT_SNG ) return 1;
    if ( fight->weapon == PL_IT_GL || fight->weapon == PL_IT_RL ) return 2;
    return 0;
}

static void near sbar_string( short x, short y, const char *text )
{
    short i;
    for ( i = 0; text[i]; i++ )
        sbar_char( (unsigned char) text[i], (short) ( x + i * 8 ), y );
}

static void near sbar_score( const World *world, const SbarState *state )
{
    char text[48], title[48];
    short len;

    sprintf( text, "Monsters:%3d /%3d", (int) state->kills, (int) state->monsters );
    sbar_string( 8, 28, text );
    sprintf( text, "Secrets :%3d /%3d", (int) state->secrets, (int) state->secret_total );
    sbar_string( 8, 36, text );
    sprintf( text, "Time :%3ld:%02ld", state->seconds / 60L, state->seconds % 60L );
    sbar_string( 184, 28, text );
    far_text( title, world->title, sizeof(title) );
    len = (short) strlen( title );
    sbar_string( (short) ( 232 - len * 4 ), 36, title );
}

static void near sbar_paint( const World *world, const Fight *fight,
                             const SbarState *state )
{
    static short bits[6] = { PL_IT_SHOTGUN, PL_IT_SSG, PL_IT_NAILGUN,
                              PL_IT_SNG, PL_IT_GL, PL_IT_RL };
    short i, ammo;

    if ( state->lines == 48 ) sbar_copy_bar( SBARC_IBAR_Y, SBARC_WORK_Y );
    sbar_copy_bar( state->score ? SBARC_SCORE_Y : SBARC_SBAR_Y,
                    (short) ( SBARC_WORK_Y + 24 ) );

    if ( state->lines == 48 ) {
        for ( i = 0; i < 6; i++ ) if ( fight->items & bits[i] )
            sbar_weapon( (short) ( i * WEAPON_FRAMES + state->weapon_frame[i] ),
                          (short) ( i * 24 ) );
        sbar_small_num( 10, fight->shells );
        sbar_small_num( 58, fight->nails );
        sbar_small_num( 106, fight->rockets );
        sbar_small_num( 154, fight->cells );

        if ( fight->items & PL_IT_KEY1 ) sbar_item( ITEM_KEY1, 16, 192 );
        if ( fight->items & PL_IT_KEY2 ) sbar_item( ITEM_KEY2, 16, 208 );
        if ( state->power & 1 ) sbar_item( ITEM_INVIS, 16, 224 );
        if ( state->power & 2 ) sbar_item( ITEM_PENT, 16, 240 );
        if ( state->power & 4 ) sbar_item( ITEM_SUIT, 16, 256 );
        if ( state->power & 8 ) sbar_item( ITEM_QUAD, 16, 272 );
        for ( i = 0; i < 4; i++ ) if ( fight->serverflags & ( 1 << i ) )
            sbar_item( (short) ( ITEM_SIGIL + i * 8 ), 8, (short) ( 288 + i * 8 ) );
    }

    if ( state->score ) { sbar_score( world, state ); return; }
    if ( state->power & 2 ) {
        sbar_num( 24, 24, 666, 1 );
        sbar_cell( CELL_DISC, 0, 24 );
    } else {
        sbar_num( 24, 24, fight->armor, (short) ( fight->armor <= 25 ) );
        if ( fight->armor > 0 ) sbar_cell( (short) ( CELL_ARMOR + state->armor_kind ), 0, 24 );
    }
    sbar_cell( state->face, 112, 24 );
    sbar_num( 136, 24, fight->health, (short) ( fight->health <= 25 ) );
    ammo = current_ammo( fight );
    sbar_cell( (short) ( CELL_AMMO + current_ammo_icon( fight ) ), 224, 24 );
    sbar_num( 248, 24, ammo, (short) ( ammo <= 10 ) );
}

static void near far_text( char *dst, const char far *src, short size )
{
    short i = 0;
    if ( src ) while ( i < size - 1 && src[i] ) { dst[i] = src[i]; i++; }
    dst[i] = 0;
}

void scr_sbar_draw( World *world, Fight *fight, float now, short showscores,
                    short lines, QSurf dst, short w, short h )
{
    SbarState state;

    if ( lines == 0 ) {
        if ( !showscores && fight->health > 0 ) return;
        lines = 24;
    }
    sbar_state( &state, world, fight, now, showscores, lines );
    if ( !sbar_valid || memcmp( &state, &sbar_last, sizeof(state) ) ) {
        sbar_paint( world, fight, &state );
        sbar_last = state;
        sbar_valid = -1;
    }
    qglDrBlit( dst, (short) ( ( w - SBARC_W ) / 2 ),
                (short) ( h - lines ), lines == 48 ? sbar_view48 : sbar_view24 );
}
