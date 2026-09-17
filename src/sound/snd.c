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

void snd_init( Sound *snd, Pak *pak, short off )
{
    SndRec far *tab;
    unsigned char far *raw;
    unsigned char far *dec;
    long  remain, want, n;
    short fh, pg, pgseg, count;

    snd->on = 0;
    snd->hash = 0UL;
    if ( off ) return;
    if ( !pak_has( pak, "snd.bsc" ) || !pak_has( pak, "sndtab.raw" ) || !pak_has( pak, "snddec.raw" ) ) {
        fprintf( stderr, "no sounds in the container -- rebuild its assets with the PAK\n" );
        return;
    }
    if ( !qglDspInit( SND_RATE ) ) return;   /* no card: play nothing, run on */

    /* the table first: its count has to agree with snd.h's SND_*,
       which are indices into it. A table from another mksnd.py run
       would play the wrong sound for every id and say nothing. */
    raw = pak_load_whole( pak, "sndtab.raw", &n );
    count = *(short far *) raw;
    if ( count != SND_COUNT || n < 2 + (long) count * sizeof(SndRec) ) {
        fprintf( stderr, "sndtab.raw has %d sounds, snd.h says %d\n", (int) count, SND_COUNT );
        qglMemFree( (long) raw );
        qglDspShutdown();
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
        qglDspShutdown();
        return;
    }
    for ( pg = 0; remain > 0; pg++ ) {
        pgseg = qglGemMap( snd->hnd, pg, PAGE_SLOT );
        if ( !pgseg ) sys_error( "snd.bsc: a page would not map" );
        want = remain > SND_PAGE ? SND_PAGE : remain;
        if ( qglFileRead( fh, (long) pgseg << 16, want ) != want ) {
            sys_error( "snd.bsc: short read" );
        }
        remain -= want;
    }

    if ( snd_mix_setup( &snd->mix, snd->hnd, (unsigned char far *) qglDspBuf(),
                        (void far *) qglDspScratch(), tab, count,
                        qglDspScratchBytes(), (signed char far *) dec,
                        SND_WATER, SND_WIND ) < 0 ) {
        fprintf( stderr, "dsp.asm's scratch is short of the mixer's table and channels\n" );
        qglMemFree( (long) dec );
        qglMemFree( (long) raw );
        qglGemFree( snd->hnd );
        snd->hnd = 0;
        qglDspShutdown();
        return;
    }
    qglMemFree( (long) dec );
    qglMemFree( (long) raw );
    snd->on = -1;
}

/* Every sound decoded and summed, the run's own answer to what mksnd.py
   put in the container -- 0 with the card or the flag off. */
unsigned long snd_sum( Sound *snd )
{
    return snd->on ? snd_mix_sum( &snd->mix ) : 0L;
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
    snd->hash = snd->hash * 31UL + (unsigned long) ( id * 8 + chan );
    if ( !snd->on ) return;
    if ( snd_mix_start( &snd->mix, id, volume, ent, chan, attn, org, &player->pos ) < 0 ) return;
    snd->started++;
    snd->seen[id >> 3] |= (unsigned char) ( 1 << ( id & 7 ) );
}

void snd_stop( Sound *snd, short ent, short chan )
{
    if ( snd->on ) snd_mix_stop( &snd->mix, ent, chan );
}

void snd_frame( Sound *snd, World *world, Player *player, Camera *cam, float dt )
{
    BspVec3 eye, right;
    float fx, fy, len;

    if ( !snd->on ) return;
    eye = player->pos;
    eye.z += PL_EYE;
    fx = cam->look_at.x - cam->pos.x;
    fy = cam->look_at.z - cam->pos.z;
    len = (float) sqrt( fx*fx + fy*fy );
    if ( len > 0.0f ) { fx /= len; fy /= len; }
    right.x = fy; right.y = -fx; right.z = 0.0f;
    snd->under = snd_mix_frame( &snd->mix, qglDspPos(), (long) ( dt * SND_RATE ), &player->pos,
                                &right, world->leaves[ r_point_leaf( &eye, world ) ].amb );
}

void snd_volume( Sound *snd, float volume )
{
    if ( volume < 0.0f ) volume = 0.0f;
    if ( volume > 1.0f ) volume = 1.0f;
    snd_mix_volume( &snd->mix, (short) ( volume * 255.0f ) );
}

void snd_shutdown( Sound *snd )
{
    if ( snd->on ) qglDspShutdown();
    if ( snd->hnd ) qglGemFree( snd->hnd );
    if ( snd->mix.pts ) qglMemFree( (long) (void far *) snd->mix.pts );
    snd->on = snd->hnd = 0;
    snd->mix.pts = 0;
    snd->mix.ptmax = snd->mix.npts = 0;
}

void snd_stats( const Sound *snd, short *started, short *loops, short *under )
{
    *started = snd->started;
    *loops   = snd_mix_loops( &snd->mix );
    *under   = snd->under;
}

void snd_report( const Sound *snd, char *buf )
{
    short ids[8], i, n = snd_mix_live( &snd->mix, ids ), id, left, right;

    strcpy( buf, "snd seen=" );
    for ( i = 0; i < (short) sizeof(snd->seen); i++ )
        sprintf( buf + strlen( buf ), "%02X", (int) snd->seen[i] );
    strcat( buf, " live=" );
    for ( i = 0; i < n; i++ )
        sprintf( buf + strlen( buf ), i ? ",%d" : "%d", (int) ids[i] );
    sprintf( buf + strlen( buf ), " wraps=%d water=%d sky=%d voices=%d",
             (int) snd_mix_wraps( &snd->mix ), (int) snd_mix_leaf_vol( &snd->mix, 0 ),
             (int) snd_mix_leaf_vol( &snd->mix, 1 ), (int) snd_mix_voices( &snd->mix ) );
    if ( snd_mix_pan( &snd->mix, &id, &left, &right ) )
        sprintf( buf + strlen( buf ), " pan=%d:%d,%d", (int) id, (int) left, (int) right );
}
