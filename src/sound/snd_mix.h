#ifndef __SND_MIX_H__
#define __SND_MIX_H__

#include "bsptypes.h"

/*
 * snd_mix.h -- S_PaintChannels for one 8-bit stereo ring. snd.c is the
 * layer above: this owns the channels and the paint and knows nothing
 * about files, the card or the game.
 */

#define SND_LEAF    2               /* water and sky; id's slime and lava have no sound */
#define SND_CHANS   8               /* MAX_DYNAMIC_CHANNELS */
#define SND_VOICES  8               /* distinct static sounds: e1's maps use seven at most */
#define SND_ALL     (SND_LEAF + SND_CHANS + SND_VOICES)
#define SND_CHUNK   512
#define SND_DTAB    512             /* 32 scales x 16 codes, mksnd.py's */

/* an (offset, length, loop) into the sample stream, sndtab.raw's own
   record. The offset is in SAMPLES and lands on a block; the length is
   the wav's own, so the silence a block's tail is padded with never
   plays. loop is where a looping sound rejoins, -1 for one that plays
   once. */
typedef struct { long ofs, len; short loop; } SndRec;

typedef struct {
    long  pos, end, beg;            /* end 0: free; beg >= 0 loops */
    short left, right, master;
    short id, ent, chan, attn;
    float x, y, z;
} SndChan;

/* a static's place, whole units, and the voice that plays it */
typedef struct { unsigned char voice, vol; short x, y, z; } SndPoint;

/* The mixer. The table and the channels live in dsp.asm's scratch block,
   beyond its ring; the codec table and one decoded run live here. */
typedef struct {
    short hnd, count, loops, wraps, master;
    long  fade_acc;
    int           far *paint_l;     /* SND_CHUNK ints each */
    int           far *paint_r;
    SndRec        far *tab;
    SndChan       far *chan;
    SndPoint      far *pts;
    short npts, ptmax, nvoice;
    short voice_id[SND_VOICES];
    BspVec3 listener_right;
    signed char dtab[SND_DTAB];     /* scale row, then code */
    signed char dbuf[SND_CHUNK];    /* one run, decoded */
} SndMix;

/* the entity whose sounds are never spatialised and never stolen by
   another's: cl.viewentity */
#define SND_ENT_PLAYER 1

/*
 * name: snd_mix_setup
 * desc: hnd is the samples' EMS handle, scratch dsp.asm's spare block
 *       and scratch_bytes how much of it there
 *       is -- -1 when that is short, which is a real failure and not a
 *       rounding: the channels once ran 304 bytes past dsp.asm's 1792
 *       and the map hung in the allocator instead. dec is snddec.raw,
 *       512 bytes of it, the codec's whole contract. water and sky are
 *       the leaves' two ambient loops. Starts every recorded static.
 */
short snd_mix_setup( SndMix *mix, short hnd, void far *scratch,
                      SndRec far *tab, short count, short scratch_bytes,
                      signed char far *dec, short water, short sky );

/* S_StartSound: a channel of ent's own chan (not 0) is replaced, else a
   free one or the one with least left, never the player's for another
   entity's sound. vol 1..255, attn ATTN_*, org where it starts, ear the
   listener. A sound silent from there takes nothing. The channel, or -1. */
short snd_mix_start( SndMix *mix, short id, short vol, short ent, short chan, short attn,
                      BspVec3 *org, BspVec3 *ear );
void snd_mix_stop( SndMix *mix, short ent, short chan );

/* room for n static points, before any snd_mix_ambient */
short snd_mix_statics( SndMix *mix, short n );

/* S_StaticSound, before the card is up: a point playing id at vol. Every
   point of one sound shares one looping channel, its volume the points'
   sum -- Quake's combine, and what lets 71 torches play on eight
   channels. -1 when full. */
short snd_mix_ambient( SndMix *mix, short id, short vol, BspVec3 *org );

/* static points playing, and the channels they share */
short snd_mix_loops( const SndMix *mix );
short snd_mix_voices( const SndMix *mix );

/* times an entity's looping sound rejoined its loop */
short snd_mix_wraps( const SndMix *mix );

/* the sounds on the dynamic channels now, up to 8, and how many */
short snd_mix_live( const SndMix *mix, short *ids );

/* first placed static voice, for the headless stereo regression */
short snd_mix_pan( const SndMix *mix, short *id, short *left, short *right );

/* the water (0) or sky (1) ambient's volume now */
short snd_mix_leaf_vol( const SndMix *mix, short i );

/* Every sound in the table decoded through the paint's own fetch and
   summed, table order, the duplicates twice: the only headless view of
   what the card is handed, since a decode that reads the wrong nibble
   or the wrong page still fills the ring and every counter stays
   right. -sndsum prints it; test-sndcodec.sh has mksnd.py's own answer. */
unsigned long snd_mix_sum( SndMix *mix );

/*
 * name: snd_mix_update / snd_mix_paint
 * desc: Update runs from the foreground and owns floating-point
 *       spatialisation. Paint is the bounded, integer-only SB callback:
 *       it fills exactly frames stereo samples at dst, advances voices,
 *       and restores the EMS slot it borrows before returning.
 */
void snd_mix_update( SndMix *mix, long adv, BspVec3 *ear,
                     BspVec3 *right, unsigned char amb );
void snd_mix_paint( SndMix *mix, unsigned char far *dst, short frames );
void snd_mix_volume( SndMix *mix, short volume );

#endif
