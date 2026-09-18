/*
 * dl.c -- Quake's dynamic lights, from WinQuake: CL_AllocDlight,
 * CL_DecayLights and CL_RelinkEntities' effects (cl_main.c), the
 * TE_EXPLOSION light (cl_tent.c) and R_AddDynamicLights (r_surf.c).
 *
 * Persistent light state lives behind the renderer's EMS frame store.
 * Decay and relinking run once per rendered frame; events may replace a
 * slot during the fixed simulation steps. The flicker draws from its own
 * LCG so a light cannot move the AI or weapon sequences.
 */

#include <math.h>
#include <mem.h>

#include "dl.h"

#define DL_KEY_SPIKE( n ) ( -1 - (n) )   /* the nail slot's */
#define DL_EMS_SLOT 3

typedef char DlStoreFits[
    ( RDR_DLIGHT_OFS + sizeof(DynLight) * DL_MAX <= RDR_FRAME_BYTES ) ? 1 : -1 ];

static DynLight far *dl_map( Renderer *rdr )
{
    unsigned char far *base;

    if ( !rdr->frame_dc ) return 0;
    base = (unsigned char far *) qglSfAccessWrEx( rdr->frame_dc, 0, DL_EMS_SLOT );
    return base ? (DynLight far *) ( base + RDR_DLIGHT_OFS ) : 0;
}

/* id's neutral light style, d_lightstylevalue for 'm': a luxel byte
   stands for byte * 264 of blocklights, which qglSbBuild's LEVEL2T
   (16320 - byte * 66) undoes */
#define DL_STYLE_UNIT 264L

/* rand() & 31 */
static short dl_rand31( Renderer *rdr )
{
    rdr->dl_seed = (unsigned short) ( rdr->dl_seed * 25173U + 13849U );
    return (short) ( ( rdr->dl_seed >> 8 ) & 31 );
}

static short dl_slot( Renderer *rdr, DynLight far *lights, short key )
{
    short i;

    if ( key )
        for ( i = 0; i < DL_MAX; i++ )
            if ( lights[i].key == key ) return i;
    for ( i = 0; i < DL_MAX; i++ )
        if ( lights[i].die < rdr->anim_time ) return i;
    return 0;
}

static DynLight far *dl_alloc( Renderer *rdr, DynLight far *lights, short key )
{
    DynLight far *dl = &lights[ dl_slot( rdr, lights, key ) ];

    _fmemset( dl, 0, sizeof(*dl) );
    dl->key = key;
    return dl;
}

void dl_decay( Renderer *rdr, float dt )
{
    DynLight far *lights = dl_map( rdr );
    short i;

    rdr->dl_tick = (short) ( ( rdr->dl_tick + 1 ) % 32000 );
    if ( !lights ) return;
    for ( i = 0; i < DL_MAX; i++ ) {
        DynLight far *dl = &lights[i];
        if ( dl->die < rdr->anim_time || dl->radius == 0.0f ) continue;
        dl->radius -= dt * dl->decay;
        if ( dl->radius < 0.0f ) dl->radius = 0.0f;
    }
}

void dl_muzzle( Renderer *rdr, short key, BspVec3 *org, BspVec3 *fwd )
{
    DynLight far *lights = dl_map( rdr );
    DynLight far *dl;

    if ( !lights ) return;
    dl = dl_alloc( rdr, lights, key );

    dl->origin.x = org->x + 18.0f * fwd->x;
    dl->origin.y = org->y + 18.0f * fwd->y;
    dl->origin.z = org->z + 16.0f + 18.0f * fwd->z;
    dl->radius   = 200.0f + dl_rand31( rdr );
    dl->minlight = 32.0f;
    dl->die      = rdr->anim_time + 0.1f;
}

void dl_explosion( Renderer *rdr, BspVec3 *org )
{
    DynLight far *lights = dl_map( rdr );
    DynLight far *dl;

    if ( !lights ) return;
    dl = dl_alloc( rdr, lights, 0 );

    dl->origin = *org;
    dl->radius = 350.0f;
    dl->die    = rdr->anim_time + 0.5f;
    dl->decay  = 300.0f;
}

void dl_relink( Renderer *rdr, Player *player, Fight *fight )
{
    DynLight far *lights = dl_map( rdr );
    DynLight far *dl;
    short n;

    if ( !lights ) return;

    /* the player's own key: with the quad this replaces a muzzle flash,
       as the relink's EF_DIMLIGHT does in id's */
    if ( fight->quad_until > rdr->anim_time || fight->pent_until > rdr->anim_time ) {
        dl = dl_alloc( rdr, lights, DL_KEY_PLAYER );
        dl->origin = player->pos;
        dl->radius = 200.0f + dl_rand31( rdr );
        dl->die    = rdr->anim_time + 0.001f;
    }

    /* missile.mdl and lavaball.mdl carry EF_ROCKET; grenade.mdl,
       w_spike.mdl and zom_gib.mdl do not */
    for ( n = 0; n < PL_NAILS_MAX; n++ ) {
        Spike *s = &fight->nail[n];
        if ( !s->alive || !( s->rocket || s->toss ) ) continue;
        dl = dl_alloc( rdr, lights, (short) DL_KEY_SPIKE( n ) );
        dl->origin = s->pos;
        dl->radius = 200.0f;
        dl->die    = rdr->anim_time + 0.01f;
    }
}

unsigned long dl_live( Renderer *rdr )
{
    DynLight far *lights = dl_map( rdr );
    unsigned long live = 0, m = 1;
    short i;

    if ( rdr->no_dlight || !lights ) return 0;
    for ( i = 0; i < DL_MAX; i++, m <<= 1 )
        if ( lights[i].die >= rdr->anim_time && lights[i].radius > 0.0f )
            live |= m;
    return live;
}

static float dl_plane_dist( DynLight far *dl, Plane far *pl )
{
    return dl->origin.x * pl->norm.x + dl->origin.y * pl->norm.y +
           dl->origin.z * pl->norm.z - pl->dist;
}

/* The light's foot on the plane in the face's texel space, less
   texturemins: R_AddDynamicLights' local[]. */
static void dl_local( DynLight far *dl, Plane far *pl, TexInfo far *ti, float d,
                      short tms, short tmt, float *ls, float *lt )
{
    float ix = dl->origin.x - pl->norm.x * d;
    float iy = dl->origin.y - pl->norm.y * d;
    float iz = dl->origin.z - pl->norm.z * d;

    *ls = ix * ti->vecs[0] + iy * ti->vecs[1] + iz * ti->vecs[2] + ti->vecs[3] - tms;
    *lt = ix * ti->vect[0] + iy * ti->vect[1] + iz * ti->vect[2] + ti->vect[3] - tmt;
}

unsigned long dl_mark( Renderer *rdr, unsigned long live, Plane far *pl,
                       TexInfo far *ti, short tms, short tmt,
                       short extw, short exth )
{
    DynLight far *lights = dl_map( rdr );
    unsigned long bits = 0, m = 1;
    short i;
    float d, r, ls, lt;

    if ( !lights ) return 0;
    for ( i = 0; live; i++, m <<= 1, live >>= 1 ) {
        DynLight far *dl = &lights[i];
        if ( !( live & 1 ) ) continue;
        d = dl_plane_dist( dl, pl );
        if ( d > dl->radius || d < -dl->radius ) continue;

        /* a luxel is lit only under an octagonal distance of radius,
           which is at least the larger axis; one more for the truncation */
        r = dl->radius + 1.0f;
        dl_local( dl, pl, ti, d, tms, tmt, &ls, &lt );
        if ( ls < -r || ls > extw + r || lt < -r || lt > exth + r ) continue;
        bits |= m;
    }
    return bits;
}

short dl_stag( Renderer *rdr )
{
    return (short) ( -1 - rdr->dl_tick );
}

void dl_add_luxels( Renderer *rdr, unsigned long bits, Plane far *pl,
                    TexInfo far *ti, short tms, short tmt,
                    unsigned char *lum, short lmw, short lmh )
{
    DynLight far *lights = dl_map( rdr );
    float ls[DL_MAX], lt[DL_MAX];
    long  rad256[DL_MAX], minl[DL_MAX], td[DL_MAX];
    short i, k, n = 0, s, t;
    long  sd, dist, sum;
    float d, rad, ml;

    if ( !lights ) return;
    for ( i = 0; bits; i++, bits >>= 1 ) {
        DynLight far *dl = &lights[i];
        if ( !( bits & 1 ) ) continue;

        d   = dl_plane_dist( dl, pl );
        rad = dl->radius - (float) fabs( d );
        if ( rad < dl->minlight ) continue;
        ml  = rad - dl->minlight;
        dl_local( dl, pl, ti, d, tms, tmt, &ls[n], &lt[n] );

        /* id adds (unsigned) ((rad - dist) * 256) for an integer dist,
           which is this; and dist < ml for an integer is dist < ceil */
        rad256[n] = (long) ( rad * 256.0f );
        minl[n]   = (long) ceil( ml );
        n++;
    }
    if ( n == 0 ) return;

    for ( t = 0; t < lmh; t++ ) {
        for ( k = 0; k < n; k++ ) {
            td[k] = (long) ( lt[k] - t * 16.0f );
            if ( td[k] < 0 ) td[k] = -td[k];
        }
        for ( s = 0; s < lmw; s++, lum++ ) {
            sum = 0;
            for ( k = 0; k < n; k++ ) {
                sd = (long) ( ls[k] - s * 16.0f );
                if ( sd < 0 ) sd = -sd;
                dist = sd > td[k] ? sd + ( td[k] >> 1 ) : td[k] + ( sd >> 1 );
                if ( dist < minl[k] ) sum += rad256[k] - ( dist << 8 );
            }
            if ( sum == 0 ) continue;
            /* ( *lum * 264 + sum ) / 264, the luxel's multiple dividing out */
            sum = *lum + sum / DL_STYLE_UNIT;
            *lum = (unsigned char) ( sum > 255 ? 255 : sum );
        }
    }
}

void dl_export( Renderer *rdr, DynLight far *dst )
{
    DynLight far *lights = dl_map( rdr );

    if ( lights ) _fmemcpy( dst, lights, sizeof(DynLight) * DL_MAX );
}

void dl_import( Renderer *rdr, DynLight far *src )
{
    DynLight far *lights = dl_map( rdr );

    if ( lights ) _fmemcpy( lights, src, sizeof(DynLight) * DL_MAX );
}

/*
 * The luxel expectations are worked by hand from R_AddDynamicLights: a
 * light on the plane z = 0 at the origin, s along x and t along y, one
 * luxel every 16 texels from texturemins, and ( rad - dist ) * 256 / 264
 * bytes a luxel.
 */
static short dl_selftest_cases( Renderer *rp, DynLight far *lights )
{
    Renderer *r = rp;
    Plane pl;
    TexInfo ti;
    BspVec3 o, f;
    unsigned char lum[6];
    short st, i;

    r->anim_time = 1.0f;
    o.x = 0.0f; o.y = 0.0f; o.z = 0.0f;
    f.x = 1.0f; f.y = 0.0f; f.z = 0.0f;

    /* CL_AllocDlight: a dead slot, the key's own slot, then slot 0 */
    dl_muzzle( r, 5, &o, &f );
    if ( lights[0].key != 5 ) return -1;
    if ( lights[0].origin.x != 18.0f || lights[0].origin.z != 16.0f ) return -2;
    if ( lights[0].radius < 200.0f || lights[0].radius > 231.0f ) return -3;
    dl_muzzle( r, 6, &o, &f );
    if ( lights[1].key != 6 ) return -4;
    dl_muzzle( r, 5, &o, &f );
    if ( lights[0].key != 5 || lights[2].key != 0 ) return -5;
    for ( i = 0; i < DL_MAX; i++ ) lights[i].die = 2.0f;
    dl_explosion( r, &o );
    if ( lights[0].key != 0 || lights[0].radius != 350.0f ) return -6;

    /* CL_DecayLights, clamped at nothing; and a new stag each tick */
    st = dl_stag( r );
    dl_decay( r, 0.5f );
    if ( lights[0].radius != 200.0f || lights[1].radius < 200.0f ) return -7;
    if ( dl_stag( r ) == st || dl_stag( r ) >= 0 ) return -8;
    dl_decay( r, 1.0f );
    if ( lights[0].radius != 0.0f ) return -9;

    /* live and marked: within the radius of the plane, and of a 32 x 32
       luxel rect */
    _fmemset( lights, 0, sizeof(DynLight) * DL_MAX );
    r->anim_time = 1.0f;
    r->no_dlight = 0;
    lights[3].radius = 200.0f;
    lights[3].die = 2.0f;
    ti.vecs[0] = 1.0f; ti.vecs[1] = 0.0f; ti.vecs[2] = 0.0f; ti.vecs[3] = 0.0f;
    ti.vect[0] = 0.0f; ti.vect[1] = 1.0f; ti.vect[2] = 0.0f; ti.vect[3] = 0.0f;
    pl.norm.x = 0.0f; pl.norm.y = 0.0f; pl.norm.z = 1.0f;
    pl.dist = 200.0f;
    if ( dl_live( r ) != 8UL ) return -10;
    if ( dl_mark( r, 8UL, &pl, &ti, 0, 0, 32, 32 ) != 8UL ) return -11;
    pl.dist = 201.0f;
    if ( dl_mark( r, 8UL, &pl, &ti, 0, 0, 32, 32 ) != 0UL ) return -12;
    pl.dist = 0.0f;
    lights[3].origin.x = -200.0f;
    if ( dl_mark( r, 8UL, &pl, &ti, 0, 0, 32, 32 ) != 8UL ) return -13;
    lights[3].origin.x = -202.0f;
    if ( dl_mark( r, 8UL, &pl, &ti, 0, 0, 32, 32 ) != 0UL ) return -14;
    lights[3].origin.x = 234.0f;
    if ( dl_mark( r, 8UL, &pl, &ti, 0, 0, 32, 32 ) != 0UL ) return -15;
    lights[3].origin.x = 0.0f;
    r->no_dlight = -1;
    if ( dl_live( r ) != 0UL ) return -16;
    r->no_dlight = 0;
    lights[3].die = 0.5f;
    if ( dl_live( r ) != 0UL ) return -17;
    lights[3].die = 2.0f;

    /* 3 x 2: 200 - the octagonal distance, 16 a luxel; clamped at 255 */
    memset( lum, 0, sizeof(lum) );
    lum[2] = 100; lum[5] = 50;
    dl_add_luxels( r, 8UL, &pl, &ti, 0, 0, lum, 3, 2 );
    if ( lum[0] != 193 || lum[1] != 178 || lum[2] != 255 ) return -18;
    if ( lum[3] != 178 || lum[4] != 170 || lum[5] != 205 ) return -19;

    /* 50 above the plane: 150 at the foot */
    memset( lum, 0, sizeof(lum) );
    pl.dist = -50.0f;
    dl_add_luxels( r, 8UL, &pl, &ti, 0, 0, lum, 3, 2 );
    if ( lum[0] != 145 || lum[1] != 129 ) return -20;
    pl.dist = 0.0f;

    /* texturemins move the grid: luxel 1 is the light's foot */
    memset( lum, 0, sizeof(lum) );
    dl_add_luxels( r, 8UL, &pl, &ti, -16, 0, lum, 3, 2 );
    if ( lum[0] != 178 || lum[1] != 193 ) return -21;

    /* minlight: 48 less 32 lights only dist under 16; 30 lights nothing */
    memset( lum, 0, sizeof(lum) );
    lights[3].radius = 48.0f;
    lights[3].minlight = 32.0f;
    dl_add_luxels( r, 8UL, &pl, &ti, 0, 0, lum, 3, 2 );
    if ( lum[0] != 46 || lum[1] != 0 ) return -22;
    lights[3].radius = 30.0f;
    dl_add_luxels( r, 8UL, &pl, &ti, 0, 0, lum, 3, 2 );
    if ( lum[0] != 46 ) return -23;

    /* two lights of 97.8 bytes each: 195, where truncating each is 194 */
    memset( lum, 0, sizeof(lum) );
    lights[3].radius = 100.9f;
    lights[3].minlight = 0.0f;
    lights[4] = lights[3];
    dl_add_luxels( r, 24UL, &pl, &ti, 0, 0, lum, 3, 2 );
    if ( lum[0] != 195 ) return -24;

    return 1;
}

short dl_selftest( void )
{
    Renderer r;
    DynLight far *lights;
    short result;

    memset( &r, 0, sizeof(r) );
    r.frame_dc = qglSfNew( RDR_PART_BYTES, RDR_FRAME_ROWS, QGL_SURF_EMS );
    if ( !r.frame_dc ) return -25;
    lights = dl_map( &r );
    if ( !lights ) {
        qglSfFree( r.frame_dc );
        return -26;
    }
    _fmemset( lights, 0, sizeof(DynLight) * DL_MAX );
    result = dl_selftest_cases( &r, lights );
    qglSfFree( r.frame_dc );
    return result;
}
