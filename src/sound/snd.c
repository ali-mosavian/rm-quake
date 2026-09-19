/*
 * snd.c -- see snd.h.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "snd.h"
#include "r_bsp.h"
#include "qgl.h"
#include "sys.h"

#define SND_RATE       11025
#define SND_PAGE       16384L
#define PAGE_SLOT      2

typedef void (far *SndDspCallback)( unsigned char far *, unsigned short );

static Sound *snd_irq_sound;
static short  snd_device_on;

static void far cdecl snd_dsp_callback( unsigned char far *dst, unsigned short bytes )
{
    if ( snd_irq_sound ) {
        snd_mix_paint( &snd_irq_sound->mix, dst, (short) ( bytes >> 1 ) );
        snd_irq_sound->painted++;
    }
}

static void snd_dsp_attach( Sound *snd )
{
    snd_irq_sound = snd;
    qglDspCallback( (long) (SndDspCallback) snd_dsp_callback );
}

static void snd_dsp_detach( void )
{
    qglDspCallback( 0L );
    snd_irq_sound = 0;
}

static void snd_dsp_silence( void )
{
    short lock;

    if ( !snd_device_on ) return;
    lock = qglDspLock();
    _fmemset( (unsigned char far *) qglDspBuf(), 128, 4096 );
    qglDspUnlock( lock );
}

void snd_device_init( short off )
{
    snd_device_on = (short) ( !off && qglDspInit( SND_RATE ) );
}

void snd_device_shutdown( void )
{
    if ( !snd_device_on ) return;
    snd_dsp_detach();
    qglDspShutdown();
    snd_device_on = 0;
}

void snd_init( Sound *snd, Pak *pak, short off )
{
    SndRec far *tab;
    unsigned char far *raw;
    unsigned char far *dec;
    long  remain, want, n, saved_map;
    short fh, pg, pgseg, count;

    snd->on = 0;
    snd->hash = 0UL;
    snd->painted = 0;
    if ( off || !snd_device_on ) return;
    if ( !pak_has( pak, "snd.bsc" ) || !pak_has( pak, "sndtab.raw" ) || !pak_has( pak, "snddec.raw" ) ) {
        fprintf( stderr, "no sounds in the container -- rebuild its assets with the PAK\n" );
        return;
    }
    /* the table first: its count has to agree with snd.h's SND_*,
       which are indices into it. A table from another mksnd.py run
       would play the wrong sound for every id and say nothing. */
    raw = pak_load_whole( pak, "sndtab.raw", &n );
    count = *(short far *) raw;
    if ( count != SND_COUNT || n < 2 + (long) count * sizeof(SndRec) ) {
        fprintf( stderr, "sndtab.raw has %d sounds, snd.h says %d\n", (int) count, SND_COUNT );
        qglMemFree( (long) raw );
        return;
    }
    tab = (SndRec far *) ( raw + 2 );
    dec = pak_load( pak, "snddec.raw", (long) SND_DTAB );

    /* and the samples, straight into EMS a page at a time: 1.2M of
       bsc4/32n, which is why they are not in the far heap */
    fh = pak_seek( pak, "snd.bsc", &remain );
    snd->hnd = qglGemAlloc( remain );
    if ( !snd->hnd ) {
        fprintf( stderr, "snd.bsc: no EMS for %ld bytes\n", remain );
        qglMemFree( (long) dec );
        qglMemFree( (long) raw );
        return;
    }
    saved_map = qglGemSaveMap( PAGE_SLOT );
    for ( pg = 0; remain > 0; pg++ ) {
        pgseg = qglGemMap( snd->hnd, pg, PAGE_SLOT );
        if ( !pgseg ) sys_error( "snd.bsc: a page would not map" );
        want = remain > SND_PAGE ? SND_PAGE : remain;
        if ( qglFileRead( fh, (long) pgseg << 16, want ) != want ) {
            sys_error( "snd.bsc: short read" );
        }
        remain -= want;
    }
    qglGemRestoreMap( saved_map, PAGE_SLOT );

    if ( snd_mix_setup( &snd->mix, snd->hnd, (void far *) qglDspScratch(), tab, count,
                        qglDspScratchBytes(), (signed char far *) dec,
                        SND_WATER, SND_WIND ) < 0 ) {
        fprintf( stderr, "dsp.asm's scratch is short of the mixer's table and channels\n" );
        qglMemFree( (long) dec );
        qglMemFree( (long) raw );
        qglGemFree( snd->hnd );
        snd->hnd = 0;
        return;
    }
    qglMemFree( (long) dec );
    qglMemFree( (long) raw );
    snd_dsp_attach( snd );
    snd->on = -1;
}

/* Every sound decoded and summed, the run's own answer to what mksnd.py
   put in the container -- 0 with the card or the flag off. */
unsigned long snd_sum( Sound *snd )
{
    unsigned long sum;

    if ( !snd->on ) return 0L;
    snd_dsp_detach();
    sum = snd_mix_sum( &snd->mix );
    snd_dsp_attach( snd );
    return sum;
}

short snd_sum_slot_test( Sound *snd, unsigned long *sum )
{
    BspVec3 zero, right;
    long outer, expected, actual;
    short sentinel, restored, sum_ok, frame_ok;

    *sum = 0L;
    if ( !snd->on ) return 0;
    snd_dsp_detach();
    memset( &zero, 0, sizeof(zero) );
    right = zero;
    outer = qglGemSaveMap( PAGE_SLOT );
    sentinel = qglGemAlloc( SND_PAGE );
    if ( !sentinel || !qglGemMap( sentinel, 0, PAGE_SLOT ) ) {
        if ( sentinel ) qglGemFree( sentinel );
        qglGemRestoreMap( outer, PAGE_SLOT );
        snd_dsp_attach( snd );
        return 0;
    }
    expected = qglGemSaveMap( PAGE_SLOT );
    *sum = snd_mix_sum( &snd->mix );
    actual = qglGemSaveMap( PAGE_SLOT );
    sum_ok = actual == expected;
    snd_mix_start( &snd->mix, SND_SHOTGUN, 255, SND_ENT_PLAYER,
                   CHAN_AUTO, ATTN_NORM, &zero, &zero );
    snd_mix_update( &snd->mix, 0L, &zero, &right, 0 );
    snd_mix_paint( &snd->mix, (unsigned char far *) qglDspBuf(), 32 );
    actual = qglGemSaveMap( PAGE_SLOT );
    frame_ok = actual == expected;
    restored = qglGemRestoreMap( outer, PAGE_SLOT );
    qglGemFree( sentinel );
    snd_dsp_attach( snd );
    return restored && sum_ok && frame_ok;
}

/* Recorded whatever the card does, and unguarded on purpose: this runs
   while the map loads, before snd_init has been anywhere near a Sound
   Blaster, and an ambient nobody ever starts costs its record. */
void snd_statics( Sound *snd, short n )
{
    snd_mix_statics( &snd->mix, n );
}

void snd_ambient( Sound *snd, short id, short vol, BspVec3 *org )
{
    snd_mix_ambient( &snd->mix, id, vol, org );
}

void snd_play( Sound *snd, Player *player, short id, BspVec3 *org )
{
    snd_start( snd, player, 0, CHAN_AUTO, id, org, ATTN_NORM );
}

void snd_self( Sound *snd, Player *player, short chan, short id )
{
    snd_start( snd, player, SND_ENT_PLAYER, chan, id, &player->pos, ATTN_NORM );
}

void snd_start( Sound *snd, Player *player, short ent, short chan, short id, BspVec3 *org, short attn )
{
    snd_start_volume( snd, player, ent, chan, id, org, attn, 255 );
}

void snd_start_volume( Sound *snd, Player *player, short ent, short chan, short id,
                       BspVec3 *org, short attn, short volume )
{
    short lock;

    snd->hash = snd->hash * 31UL + (unsigned long) ( id * 8 + chan );
    if ( !snd->on ) return;
    lock = qglDspLock();
    if ( snd_mix_start( &snd->mix, id, volume, ent, chan, attn, org, &player->pos ) >= 0 ) {
        snd->started++;
        snd->seen[id >> 3] |= (unsigned char) ( 1 << ( id & 7 ) );
    }
    qglDspUnlock( lock );
}

void snd_stop( Sound *snd, short ent, short chan )
{
    short lock;

    if ( !snd->on ) return;
    lock = qglDspLock();
    snd_mix_stop( &snd->mix, ent, chan );
    qglDspUnlock( lock );
}

void snd_frame( Sound *snd, World *world, Player *player, Camera *cam, float dt )
{
    BspVec3 eye, right;
    float fx, fy, len;
    short lock;

    if ( !snd->on ) return;
    eye = player->pos;
    eye.z += PL_EYE;
    fx = cam->look_at.x - cam->pos.x;
    fy = cam->look_at.z - cam->pos.z;
    len = (float) sqrt( fx*fx + fy*fy );
    if ( len > 0.0f ) { fx /= len; fy /= len; }
    right.x = fy; right.y = -fx; right.z = 0.0f;
    lock = qglDspLock();
    snd_mix_update( &snd->mix, (long) ( dt * SND_RATE ), &player->pos,
                    &right, world->leaves[ r_point_leaf( &eye, world ) ].amb );
    qglDspUnlock( lock );
}

void snd_volume( Sound *snd, float volume )
{
    short lock;

    if ( volume < 0.0f ) volume = 0.0f;
    if ( volume > 1.0f ) volume = 1.0f;
    if ( !snd->on ) return;
    lock = qglDspLock();
    snd_mix_volume( &snd->mix, (short) ( volume * 255.0f ) );
    qglDspUnlock( lock );
}

void snd_shutdown( Sound *snd )
{
    if ( snd->on ) {
        snd_dsp_detach();
        snd_dsp_silence();
    }
    if ( snd->hnd ) qglGemFree( snd->hnd );
    if ( snd->mix.pts ) qglMemFree( (long) (void far *) snd->mix.pts );
    snd->on = snd->hnd = 0;
    snd->mix.pts = 0;
    snd->mix.ptmax = snd->mix.npts = 0;
}

void snd_stats( const Sound *snd, short *started, short *loops, short *under,
                unsigned short *painted )
{
    short lock = snd->on ? qglDspLock() : 0;

    *started = snd->started;
    *loops   = snd_mix_loops( &snd->mix );
    *under   = snd->under;
    if ( painted ) *painted = snd->painted;
    if ( snd->on ) qglDspUnlock( lock );
}

static void snd_report_add( char *buf, short cap, const char *text )
{
    short used = (short) strlen( buf );

    if ( used < cap - 1 ) strncat( buf, text, (size_t) ( cap - used - 1 ) );
    buf[cap - 1] = 0;
}

void snd_report( const Sound *snd, char *buf, short cap )
{
    short ids[8], i, n, id, left, right, invvol;
    short lock = snd->on ? qglDspLock() : 0;
    char piece[64];

    n = snd_mix_live( &snd->mix, ids );

    if ( cap <= 0 ) goto done;
    buf[0] = 0;
    snd_report_add( buf, cap, "snd seen=" );
    for ( i = 0; i < (short) sizeof(snd->seen); i++ ) {
        sprintf( piece, "%02X", (int) snd->seen[i] );
        snd_report_add( buf, cap, piece );
    }
    snd_report_add( buf, cap, " live=" );
    for ( i = 0; i < n; i++ ) {
        sprintf( piece, i ? ",%d" : "%d", (int) ids[i] );
        snd_report_add( buf, cap, piece );
    }
    sprintf( piece, " wraps=%d water=%d sky=%d voices=%d",
             (int) snd_mix_wraps( &snd->mix ), (int) snd_mix_leaf_vol( &snd->mix, 0 ),
             (int) snd_mix_leaf_vol( &snd->mix, 1 ),
             (int) snd_mix_voices( &snd->mix ) );
    snd_report_add( buf, cap, piece );
    sprintf( piece, " stack=%d", (int) qglDspStackFree() );
    snd_report_add( buf, cap, piece );
    invvol = snd_mix_live_volume( &snd->mix, SND_INV_IDLE );
    if ( invvol >= 0 ) {
        sprintf( piece, " invvol=%d", (int) invvol );
        snd_report_add( buf, cap, piece );
    } else if ( snd_mix_pan( &snd->mix, &id, &left, &right ) ) {
        sprintf( piece, " pan=%d:%d,%d", (int) id, (int) left, (int) right );
        snd_report_add( buf, cap, piece );
    }
done:
    if ( snd->on ) qglDspUnlock( lock );
}

short snd_report_selftest( void )
{
    Sound snd;
    SndChan chan[SND_ALL];
    struct {
        char buf[160];
        unsigned char guard[24];
    } out;
    short i;

    memset( &snd, 0, sizeof(snd) );
    memset( chan, 0, sizeof(chan) );
    memset( snd.seen, 0xFF, sizeof(snd.seen) );
    memset( &out, 0, sizeof(out) );
    memset( out.guard, 0xA5, sizeof(out.guard) );
    snd.painted = 65535U;
    snd.mix.chan = (SndChan far *) chan;
    snd.mix.wraps = 32767;
    snd.mix.nvoice = SND_VOICES;
    chan[0].left = chan[0].right = 255;
    chan[1].left = chan[1].right = 255;
    for ( i = SND_LEAF; i < SND_LEAF + SND_CHANS; i++ ) {
        chan[i].end = 1;
        chan[i].id = SND_COUNT - 1;
    }
    chan[SND_LEAF + SND_CHANS].end = 1;
    chan[SND_LEAF + SND_CHANS].id = SND_COUNT - 1;
    chan[SND_LEAF + SND_CHANS].left = 255;
    chan[SND_LEAF + SND_CHANS].right = 255;

    snd_report( &snd, out.buf, sizeof(out.buf) );
    for ( i = 0; i < (short) sizeof(out.guard); i++ )
        if ( out.guard[i] != 0xA5 ) return 0;
    return 1;
}
