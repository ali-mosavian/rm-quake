/*
 * snd_mix.c -- Quake's S_PaintChannels for one 8-bit stereo ring.
 *
 * Eighteen channels, each a run of samples in the EMS handle snd.bsc
 * was loaded into, mixed into the DMA ring dsp.asm plays, from where the
 * mixer last stopped to a quarter second past where the DMA is. Two are
 * S_UpdateAmbientSounds' water and sky, faded to the listener's leaf;
 * eight are S_StartSound's, re-placed from where each began every frame;
 * eight are the map's statics, one a sound however many points play it.
 *
 * The paint buffer, the table and the channels live in the block
 * dsp.asm took from DOS, beyond its ring -- conventional memory this
 * program is short of, and the one place already sized for it.
 *
 * A sample is read through PAGE_SLOT, mapped fresh for every run and
 * held across nothing, the same way every other reader of that slot
 * behaves; the call sits between the tick and the render, where no one
 * else has a window open.
 *
 * It is not a byte on the way in. mksnd.py ships bsc4/32n -- 32 samples
 * a block, a scale byte and sixteen of 4-bit codes -- so snd_fetch
 * decodes a run into mix->dbuf through mksnd's own table and the paint
 * reads that. 963 blocks fit a 16K page and none straddles one, so a
 * block is still one window and a divide away.
 */

#include <math.h>

#include "snd_mix.h"
#include "qgl.h"

#define PAGE_SLOT   2
#define SND_RING    4096L
#define SND_DYN0    SND_LEAF
#define SND_VOICE0  (SND_LEAF + SND_CHANS)
#define SND_BLK     32              /* samples a block, bsc4/32n */
#define SND_BLK_B   17              /* its bytes: the scale, then sixteen of codes */
#define SND_NSCALE  32              /* and the scale byte selects one, so it is masked */
#define SND_TAB     176             /* records */
#define SND_AHEAD   2756L           /* a quarter second at 11025 */
#define SND_RATE    11025L
#define EMS_PAGE    16384
#define SND_BLK_PG  (EMS_PAGE / SND_BLK_B)  /* 963; the page's last 13 bytes are padding */
#define SND_CLIP    1000.0f         /* sound_nominal_clip_dist */
#define SND_ATTN_STATIC 3.0f
#define SND_REACH   333.4f          /* past this a static is silent */
#define SND_FADE    100L            /* ambient_fade, volume a second */

short snd_mix_statics( SndMix *mix, short n )
{
    mix->pts = n > 0 ? (SndPoint far *) qglMemAlloc( (long) n * sizeof(SndPoint) ) : 0;
    mix->ptmax = mix->pts ? n : 0;
    mix->npts = mix->nvoice = 0;
    return mix->ptmax;
}

short snd_mix_ambient( SndMix *mix, short id, short vol, BspVec3 *org )
{
    SndPoint far *p;
    short v;

    if ( mix->npts >= mix->ptmax ) return -1;
    for ( v = 0; v < mix->nvoice && mix->voice_id[v] != id; v++ ) ;
    if ( v == mix->nvoice ) {
        if ( v == SND_VOICES ) return -1;
        mix->voice_id[ mix->nvoice++ ] = id;
    }
    p = &mix->pts[mix->npts];
    p->voice = (unsigned char) v;
    p->vol   = (unsigned char) vol;
    p->x = (short) org->x;
    p->y = (short) org->y;
    p->z = (short) org->z;
    return mix->npts++;
}

short snd_mix_loops( const SndMix *mix )  { return mix->loops; }
short snd_mix_voices( const SndMix *mix ) { return mix->nvoice; }
short snd_mix_wraps( const SndMix *mix )  { return mix->wraps; }

short snd_mix_leaf_vol( const SndMix *mix, short i )
{
    return mix->chan ? (short) ( ( mix->chan[i].left + mix->chan[i].right ) / 2 ) : 0;
}

short snd_mix_live( const SndMix *mix, short *ids )
{
    short i, n = 0;

    if ( !mix->chan ) return 0;
    for ( i = SND_DYN0; i < SND_VOICE0; i++ )
        if ( mix->chan[i].end ) ids[n++] = mix->chan[i].id;
    return n;
}

short snd_mix_pan( const SndMix *mix, short *id, short *left, short *right )
{
    short i;

    if ( !mix->chan ) return 0;
    for ( i = SND_VOICE0; i < SND_ALL; i++ ) {
        if ( !mix->chan[i].end || ( mix->chan[i].left == 0 && mix->chan[i].right == 0 ) ) continue;
        *id = mix->chan[i].id;
        *left = mix->chan[i].left;
        *right = mix->chan[i].right;
        return 1;
    }
    return 0;
}

/* a loop from sample 0, silent until it is placed */
static short snd_loop_start( const SndMix *mix, SndChan far *ch, short id )
{
    if ( id < 0 || id >= mix->count || mix->tab[id].loop < 0 ) return 0;
    ch->pos = mix->tab[id].ofs;
    ch->end = ch->pos + mix->tab[id].len;
    ch->beg = ch->pos + mix->tab[id].loop;
    ch->left = ch->right = ch->master = 0;
    ch->id = id;
    ch->ent = ch->chan = 0;
    return 1;
}

short snd_mix_setup( SndMix *mix, short hnd, unsigned char far *ring, void far *scratch,
                      SndRec far *tab, short count, short scratch_bytes,
                      signed char far *dec, short water, short sky )
{
    short i;

    if ( count > SND_TAB ||
         SND_CHUNK * 4 + SND_TAB * (short) sizeof(SndRec) + SND_ALL * (short) sizeof(SndChan) > scratch_bytes )
        return -1;
    mix->hnd   = hnd;
    mix->count = count;
    mix->ring  = ring;
    mix->paint_l = (int far *) scratch;
    mix->paint_r = mix->paint_l + SND_CHUNK;
    mix->tab   = (SndRec far *) ( (char far *) scratch + SND_CHUNK * 4 );
    mix->chan  = (SndChan far *) ( (char far *) mix->tab + SND_TAB * sizeof(SndRec) );
    for ( i = 0; i < count; i++ ) mix->tab[i] = tab[i];
    for ( i = 0; i < SND_DTAB; i++ ) mix->dtab[i] = dec[i];
    for ( i = 0; i < SND_ALL; i++ ) mix->chan[i].end = 0;

    snd_loop_start( mix, &mix->chan[0], water );
    snd_loop_start( mix, &mix->chan[1], sky );
    for ( i = 0; i < mix->nvoice; i++ )
        snd_loop_start( mix, &mix->chan[SND_VOICE0 + i], mix->voice_id[i] );
    mix->loops = 0;
    for ( i = 0; i < mix->npts; i++ )
        if ( mix->chan[SND_VOICE0 + mix->pts[i].voice].end ) mix->loops++;

    mix->time = mix->painted = 0;
    mix->lastpos = mix->under = 0;
    mix->master = 255;
    mix->listener_right.x = 0.0f;
    mix->listener_right.y = -1.0f;
    mix->listener_right.z = 0.0f;
    return count;
}

void snd_mix_volume( SndMix *mix, short volume )
{
    if ( volume < 0 ) volume = 0;
    if ( volume > 255 ) volume = 255;
    mix->master = volume;
}

/* id SND_Spatialize: the player's own sounds are centred at full volume. */
static void snd_mix_spatialize( SndMix *mix, SndChan far *ch, BspVec3 *ear )
{
    float dx, dy, dz, dist, dot, scale;

    if ( ch->ent == SND_ENT_PLAYER ) { ch->left = ch->right = ch->master; return; }
    dx = ch->x - ear->x;
    dy = ch->y - ear->y;
    dz = ch->z - ear->z;
    dist = (float) sqrt( dx*dx + dy*dy + dz*dz );
    if ( dist > 0.0f ) {
        dx /= dist; dy /= dist; dz /= dist;
    }
    dot = dx * mix->listener_right.x + dy * mix->listener_right.y + dz * mix->listener_right.z;
    scale = 1.0f - dist * ch->attn / SND_CLIP;
    ch->right = scale <= 0.0f ? 0 : (short) ( ch->master * scale * ( 1.0f + dot ) );
    ch->left  = scale <= 0.0f ? 0 : (short) ( ch->master * scale * ( 1.0f - dot ) );
    if ( ch->right < 0 ) ch->right = 0; else if ( ch->right > 255 ) ch->right = 255;
    if ( ch->left  < 0 ) ch->left  = 0; else if ( ch->left  > 255 ) ch->left  = 255;
}

short snd_mix_start( SndMix *mix, short id, short vol, short ent, short chan, short attn,
                      BspVec3 *org, BspVec3 *ear )
{
    SndChan far *ch;
    short i, pick = -1;
    long  least = 0x7fffffffL, left;

    if ( id < 0 || id >= mix->count ) return -1;
    for ( i = SND_DYN0; i < SND_VOICE0; i++ ) {
        ch = &mix->chan[i];
        if ( chan != 0 && ch->end && ch->ent == ent && ch->chan == chan ) { pick = i; break; }
        if ( ch->end && ch->ent == SND_ENT_PLAYER && ent != SND_ENT_PLAYER ) continue;
        left = ch->end ? ch->end - ch->pos : -1L;
        if ( left < least ) { least = left; pick = i; }
    }
    if ( pick < 0 ) return -1;

    ch = &mix->chan[pick];
    ch->pos    = mix->tab[id].ofs;
    ch->end    = ch->pos + mix->tab[id].len;
    ch->beg    = mix->tab[id].loop < 0 ? -1L : ch->pos + mix->tab[id].loop;
    ch->master = vol;
    ch->id     = id;
    ch->ent    = ent;
    ch->chan   = chan;
    ch->attn   = attn;
    ch->x = org->x; ch->y = org->y; ch->z = org->z;
    snd_mix_spatialize( mix, ch, ear );
    /* the channel was taken either way: a stop out of earshot still
       ends its entity's loop */
    if ( ch->left == 0 && ch->right == 0 ) { ch->end = 0; return -1; }
    return pick;
}

void snd_mix_stop( SndMix *mix, short ent, short chan )
{
    short i;
    for ( i = SND_DYN0; i < SND_VOICE0; i++ )
        if ( mix->chan[i].end && mix->chan[i].ent == ent && mix->chan[i].chan == chan ) {
            mix->chan[i].end = 0;
            return;
        }
}

/*
 * name: snd_fetch
 * desc: n samples from pos, decoded into out. bsc4/32n: the block's
 *       scale byte picks one of 32 rows of mksnd.py's table and each
 *       4-bit code indexes it, so nothing here computes a gain and the
 *       runtime never meets the ladder. Answers how many it did --
 *       fewer than n at the end of a page, where the caller comes back
 *       for the rest -- and 0 when the page would not map.
 */
static short snd_fetch( const SndMix *mix, long pos, short n, signed char *out )
{
    long  blk = pos >> 5;                       /* SND_BLK */
    short seg, k = 0, j, k0, run, nb;
    unsigned char far *p;
    const signed char *row;

    seg = qglGemMap( mix->hnd, (short) ( blk / SND_BLK_PG ), PAGE_SLOT );
    if ( seg == 0 ) return 0;
    nb = (short) ( blk % SND_BLK_PG );
    p  = (unsigned char far *) ( ( (unsigned long) seg << 16 ) | (unsigned) ( nb * SND_BLK_B ) );
    k0 = (short) ( pos & (SND_BLK - 1) );
    nb = SND_BLK_PG - nb;                       /* blocks this page has left */
    if ( n > nb * SND_BLK - k0 ) n = (short) ( nb * SND_BLK - k0 );
    while ( k < n ) {
        row = &mix->dtab[ (short) ( p[0] & (SND_NSCALE - 1) ) << 4 ];
        run = SND_BLK - k0;
        if ( run > n - k ) run = (short) ( n - k );
        for ( j = 0; j < run; j++, k0++ )
            out[k + j] = row[ (k0 & 1) ? ( p[1 + (k0 >> 1)] >> 4 )
                                       : ( p[1 + (k0 >> 1)] & 15 ) ];
        k  = (short) ( k + run );
        k0 = 0;
        p += SND_BLK_B;
    }
    return n;
}

unsigned long snd_mix_sum( SndMix *mix )
{
    unsigned long sum = 0;
    long saved = qglGemSaveMap( PAGE_SLOT );
    long  pos, end;
    short i, k, n, got;

    for ( i = 0; i < mix->count; i++ ) {
        pos = mix->tab[i].ofs;
        end = pos + mix->tab[i].len;
        while ( pos < end ) {
            n = end - pos > SND_CHUNK ? SND_CHUNK : (short) ( end - pos );
            got = snd_fetch( mix, pos, n, mix->dbuf );
            if ( got == 0 ) {
                qglGemRestoreMap( saved, PAGE_SLOT );
                return 0;
            }
            for ( k = 0; k < got; k++ )
                sum = sum * 31 + (unsigned char) ( mix->dbuf[k] + 128 );
            pos += got;
        }
    }
    qglGemRestoreMap( saved, PAGE_SLOT );
    return sum;
}

static void snd_mix_chan( SndMix *mix, SndChan far *ch, short n )
{
    int  far    *paint_l = mix->paint_l;
    int  far    *paint_r = mix->paint_r;
    signed char *dbuf  = mix->dbuf;
    long  remain;
    short i = 0, run, k, lvol = ch->left, rvol = ch->right, got;

    while ( i < n ) {
        if ( ch->pos >= ch->end ) {
            if ( ch->beg < 0 ) { ch->end = 0; return; }
            ch->pos = ch->beg;
            if ( ch->ent ) mix->wraps++;
        }
        remain = ch->end - ch->pos;
        run = (short) ( n - i );
        if ( run > remain ) run = (short) remain;
        if ( lvol == 0 && rvol == 0 ) { ch->pos += run; i = (short) ( i + run ); continue; }
        got = snd_fetch( mix, ch->pos, run, dbuf );
        if ( got == 0 ) { ch->end = 0; return; }
        for ( k = 0; k < got; k++ ) {
            paint_l[i + k] += ( dbuf[k] * lvol ) >> 3;
            paint_r[i + k] += ( dbuf[k] * rvol ) >> 3;
        }
        i = (short) ( i + got );
        ch->pos += got;
    }
    if ( ch->pos >= ch->end && ch->beg < 0 ) ch->end = 0;
}

/* S_UpdateAmbientSounds: ambient_level 0.3 of the leaf's level, nothing
   under 8, moving toward it ambient_fade a second. adv is the frame's
   time in samples; the remainder carries, or short frames never fade. */
static void snd_mix_leaf( SndMix *mix, unsigned char amb, long adv )
{
    SndChan far *ch;
    short i, want, step;

    mix->fade_acc += adv * SND_FADE;
    step = (short) ( mix->fade_acc / SND_RATE );
    mix->fade_acc %= SND_RATE;
    for ( i = 0; i < SND_LEAF; i++ ) {
        ch = &mix->chan[i];
        want = (short) ( ( ( i ? amb : amb >> 4 ) & 15 ) * 17 * 3 / 10 );
        if ( want < 8 ) want = 0;
        if ( ch->left < want ) {
            ch->left = (short) ( ch->left + step );
            if ( ch->left > want ) ch->left = want;
        } else if ( ch->left > want ) {
            ch->left = (short) ( ch->left - step );
            if ( ch->left < want ) ch->left = want;
        }
        ch->right = ch->left;
    }
}

/* SND_Spatialize for the statics, then S_UpdateSounds' combine: a voice
   plays at its points' summed volume, 255 at most. */
static void snd_mix_place( SndMix *mix, BspVec3 *ear )
{
    short accl[SND_VOICES], accr[SND_VOICES];
    SndPoint far *p;
    float dx, dy, dz, dist, dot, scale;
    short i, l, r;

    for ( i = 0; i < mix->nvoice; i++ ) accl[i] = accr[i] = 0;
    for ( i = 0; i < mix->npts; i++ ) {
        p = &mix->pts[i];
        dx = p->x - ear->x;
        if ( dx > SND_REACH || dx < -SND_REACH ) continue;
        dy = p->y - ear->y;
        if ( dy > SND_REACH || dy < -SND_REACH ) continue;
        dz = p->z - ear->z;
        dist = (float) sqrt( dx*dx + dy*dy + dz*dz );
        if ( dist >= SND_REACH ) continue;
        if ( dist > 0.0f ) { dx /= dist; dy /= dist; dz /= dist; }
        dot = dx * mix->listener_right.x + dy * mix->listener_right.y + dz * mix->listener_right.z;
        scale = 1.0f - dist * SND_ATTN_STATIC / SND_CLIP;
        l = (short) ( p->vol * scale * ( 1.0f - dot ) );
        r = (short) ( p->vol * scale * ( 1.0f + dot ) );
        if ( l > 0 ) accl[p->voice] += l;
        if ( r > 0 ) accr[p->voice] += r;
    }
    for ( i = 0; i < mix->nvoice; i++ ) {
        mix->chan[SND_VOICE0 + i].left  = accl[i] > 255 ? 255 : accl[i];
        mix->chan[SND_VOICE0 + i].right = accr[i] > 255 ? 255 : accr[i];
    }
}

short snd_mix_frame( SndMix *mix, short pos, long adv, BspVec3 *ear,
                     BspVec3 *right, unsigned char amb )
{
    unsigned char far *ring  = mix->ring;
    int far *left = mix->paint_l;
    int far *right_paint = mix->paint_r;
    long  delta = (long) pos - mix->lastpos, endt;
    short n, ringpos, k, c;
    int   vl, vr;
    long  saved = qglGemSaveMap( PAGE_SLOT );

    mix->listener_right = *right;
    snd_mix_leaf( mix, amb, adv );
    for ( c = SND_DYN0; c < SND_VOICE0; c++ )
        if ( mix->chan[c].end ) snd_mix_spatialize( mix, &mix->chan[c], ear );
    snd_mix_place( mix, ear );

    if ( delta < 0 ) delta += SND_RING;
    while ( adv - delta > SND_RING / 2 ) { delta += SND_RING; adv -= SND_RING; }
    mix->lastpos = pos;
    mix->time += delta;
    if ( mix->painted < mix->time ) {
        if ( mix->painted ) mix->under++;
        mix->painted = mix->time;
    }

    /* and the paint, which runs whether or not anything is playing: an
       idle channel contributes zeros and the ring gets 128, silence.
       Skipping the write while every channel is idle would leave the
       last quarter second in the buffer, and the card plays it round
       and round -- the DMA never stops, so the mixer never does. */
    endt = mix->time + SND_AHEAD;
    while ( mix->painted < endt ) {
        n = (short) ( endt - mix->painted );
        if ( n > SND_CHUNK ) n = SND_CHUNK;
        ringpos = (short) ( mix->painted & (SND_RING - 1) );
        if ( n > (short) (SND_RING - ringpos) ) n = (short) ( SND_RING - ringpos );
        for ( k = 0; k < n; k++ ) left[k] = right_paint[k] = 0;
        for ( c = 0; c < SND_ALL; c++ )
            if ( mix->chan[c].end ) snd_mix_chan( mix, &mix->chan[c], n );
        for ( k = 0; k < n; k++ ) {
            vl = left[k] >> 5; vr = right_paint[k] >> 5;
            if ( vl > 127 ) vl = 127; else if ( vl < -128 ) vl = -128;
            if ( vr > 127 ) vr = 127; else if ( vr < -128 ) vr = -128;
            if ( mix->master != 255 ) {
                vl = (int) ( (long) vl * mix->master / 255L );
                vr = (int) ( (long) vr * mix->master / 255L );
            }
            ring[(ringpos + k) * 2]     = (unsigned char) ( vl + 128 );
            ring[(ringpos + k) * 2 + 1] = (unsigned char) ( vr + 128 );
        }
        mix->painted += n;
    }
    qglGemRestoreMap( saved, PAGE_SLOT );
    return mix->under;
}
