#include "part.h"

#define PART_EXPLODE  1
#define PART_EXPLODE2 2
#define PART_DST_SLOT 2

typedef struct {
    BspVec3 at;
    BspVec3 vel;
    unsigned char color;
    unsigned char kind;
    unsigned char live;
    unsigned char ramp;       /* 4.4 fixed point */
} Particle;

typedef struct {
    unsigned short x0;
    unsigned short z;
    unsigned char wide;
    unsigned char y0, y1;
    unsigned char color;
} DrawPart;

typedef char ParticlePoolFitsOnePage[
    ( ( sizeof(Particle) + sizeof(DrawPart) ) * PART_MAX <= PART_BYTES ) ? 1 : -1 ];

static unsigned char ramp1[8] = { 0x6f, 0x6d, 0x6b, 0x69, 0x67, 0x65, 0x63, 0x61 };
static unsigned char ramp2[8] = { 0x6f, 0x6e, 0x6d, 0x6c, 0x6b, 0x6a, 0x68, 0x66 };

static unsigned short part_rand( Renderer *rdr )
{
    rdr->part_seed = (unsigned short) ( rdr->part_seed * 25173U + 13849U );
    return rdr->part_seed;
}

short part_init( Renderer *rdr )
{
    unsigned char far *bytes;
    short i;

    rdr->part_dc = qglSfNew( PART_BYTES, 1, QGL_SURF_EMS );
    if ( !rdr->part_dc ) return 0;
    bytes = (unsigned char far *) qglSfAccessWrEx( rdr->part_dc, 0, PART_EMS_SLOT );
    if ( !bytes ) {
        qglSfFree( rdr->part_dc );
        rdr->part_dc = 0;
        return 0;
    }
    for ( i = 0; i < PART_BYTES; i++ ) bytes[i] = 0;
    rdr->part_seed = 0x1234U;
    rdr->part_next = 0;
    rdr->part_pending_n = 0;
    rdr->part_live = rdr->part_drawn = 0;
    rdr->part_last = rdr->anim_time;
    return -1;
}

void part_shutdown( Renderer *rdr )
{
    if ( rdr->part_dc ) qglSfFree( rdr->part_dc );
    rdr->part_dc = 0;
    rdr->part_pending_n = 0;
    rdr->part_live = rdr->part_drawn = 0;
}

void part_explosion( Renderer *rdr, BspVec3 *origin )
{
    if ( !rdr->part_dc || rdr->part_pending_n >= PART_PENDING_MAX ) return;
    rdr->part_pending[rdr->part_pending_n++] = *origin;
}

static void part_emit_explosion( Renderer *rdr, Particle far *pool,
                                 BspVec3 *origin )
{
    Particle far *p;
    short i;

    for ( i = 0; i < PART_MAX; i++ ) {
        p = &pool[rdr->part_next];
        rdr->part_next = (short) ( ( rdr->part_next + 1 ) % PART_MAX );
        p->ramp = (unsigned char) ( ( part_rand( rdr ) & 3 ) << 4 );
        p->color = ramp1[0];
        p->kind = (unsigned char) ( ( i & 1 ) ? PART_EXPLODE : PART_EXPLODE2 );
        p->live = 1;
        p->at.x = origin->x + (float) ( ( part_rand( rdr ) & 31 ) - 16 );
        p->at.y = origin->y + (float) ( ( part_rand( rdr ) & 31 ) - 16 );
        p->at.z = origin->z + (float) ( ( part_rand( rdr ) & 31 ) - 16 );
        p->vel.x = (float) ( ( part_rand( rdr ) & 511 ) - 256 );
        p->vel.y = (float) ( ( part_rand( rdr ) & 511 ) - 256 );
        p->vel.z = (float) ( ( part_rand( rdr ) & 511 ) - 256 );
    }
}

static short part_project( Particle far *p, DrawPart far *d, float *m,
                           float xresh, float yresh, float z_near )
{
    float x = p->at.x, y = p->at.z, z = p->at.y;
    float tx, ty, w, rw, sx, sy, half;
    short x0, x1, y0, y1;

    w = x*m[3] + y*m[7] + z*m[11] + m[15];
    if ( w < z_near ) return 0;
    tx = x*m[0] + y*m[4] + z*m[8] + m[12];
    ty = x*m[1] + y*m[5] + z*m[9] + m[13];
    rw = 1.0f / w;
    sx = xresh + tx * rw * xresh;
    sy = yresh - ty * rw * yresh;
    half = 1.0f + 64.0f * rw;
    if ( half > 4.0f ) half = 4.0f;
    x0 = (short) ( sx - half ); x1 = (short) ( sx + half );
    y0 = (short) ( sy - half ); y1 = (short) ( sy + half );
    if ( x1 < 0 || x0 >= (short) ( xresh * 2.0f ) ||
         y1 < 0 || y0 >= (short) ( yresh * 2.0f ) ) return 0;
    if ( x0 < 0 ) x0 = 0;
    if ( y0 < 0 ) y0 = 0;
    if ( x1 >= (short) ( xresh * 2.0f ) ) x1 = (short) ( xresh * 2.0f - 1.0f );
    if ( y1 >= (short) ( yresh * 2.0f ) ) y1 = (short) ( yresh * 2.0f - 1.0f );
    d->x0 = (unsigned short) x0;
    d->wide = (unsigned char) ( x1 - x0 );
    d->y0 = (unsigned char) y0;
    d->y1 = (unsigned char) y1;
    d->z = (unsigned short) ( rw * 65535.0f * z_near );
    d->color = p->color;
    return 1;
}

static void part_raster( DrawPart far *draw, short count, short ymin, short ymax,
                         QSurf dst, QSurf zdc )
{
    unsigned char far *drow;
    unsigned short far *zrow;
    DrawPart far *d;
    short i, px, py, x1;

    for ( py = ymin; py <= ymax; py++ ) {
        drow = (unsigned char far *) qglSfAccessWrEx( dst, py, PART_DST_SLOT );
        zrow = zdc ? (unsigned short far *)
            qglSfAccessWrEx( zdc, py, QGL_Z_SLOT ) : 0;
        if ( !drow || ( zdc && !zrow ) ) continue;
        for ( i = 0; i < count; i++ ) {
            d = &draw[i];
            if ( py < d->y0 || py > d->y1 ) continue;
            x1 = (short) ( d->x0 + d->wide );
            for ( px = (short) d->x0; px <= x1; px++ )
                if ( !zrow || zrow[px] < d->z ) {
                    if ( zrow ) zrow[px] = d->z;
                    drow[px] = d->color;
                }
        }
    }
}

void part_frame( Renderer *rdr, Mat4 *mtx, float xresh, float yresh,
                 float z_near, QSurf dst, QSurf zdc )
{
    Particle far *pool;
    Particle far *p;
    DrawPart far *draw;
    float now = rdr->anim_time;
    float dt = now - rdr->part_last;
    float grav, dvel;
    short i, e, live = 0, drawn = 0, ymin = 32767, ymax = -1;

    rdr->part_last = now;
    if ( dt < 0.0f ) dt = 0.0f;
    if ( dt > 0.1f ) dt = 0.1f;
    if ( !rdr->part_dc || ( !rdr->part_live && !rdr->part_pending_n ) ) {
        rdr->part_drawn = 0;
        return;
    }
    pool = (Particle far *) qglSfAccessWrEx( rdr->part_dc, 0, PART_EMS_SLOT );
    if ( !pool ) return;
    draw = (DrawPart far *) ( pool + PART_MAX );
    for ( e = 0; e < rdr->part_pending_n; e++ )
        part_emit_explosion( rdr, pool, &rdr->part_pending[e] );
    rdr->part_pending_n = 0;

    grav = dt * 800.0f * 0.05f;
    dvel = 4.0f * dt;
    for ( i = 0; i < PART_MAX; i++ ) {
        p = &pool[i];
        if ( !p->live ) continue;
        if ( part_project( p, &draw[drawn], (float *) mtx, xresh, yresh,
                          z_near ) ) {
            if ( draw[drawn].y0 < ymin ) ymin = draw[drawn].y0;
            if ( draw[drawn].y1 > ymax ) ymax = draw[drawn].y1;
            drawn++;
        }
        p->at.x += p->vel.x * dt;
        p->at.y += p->vel.y * dt;
        p->at.z += p->vel.z * dt;
        if ( p->kind == PART_EXPLODE ) {
            p->ramp = (unsigned char) ( p->ramp + (unsigned char) ( dt * 160.0f ) );
            if ( p->ramp >= 128 ) { p->live = 0; continue; }
            p->color = ramp1[p->ramp >> 4];
            p->vel.x += p->vel.x * dvel;
            p->vel.y += p->vel.y * dvel;
            p->vel.z += p->vel.z * dvel;
        } else {
            p->ramp = (unsigned char) ( p->ramp + (unsigned char) ( dt * 240.0f ) );
            if ( p->ramp >= 128 ) { p->live = 0; continue; }
            p->color = ramp2[p->ramp >> 4];
            p->vel.x -= p->vel.x * dt;
            p->vel.y -= p->vel.y * dt;
            p->vel.z -= p->vel.z * dt;
        }
        p->vel.z -= grav;
        live++;
    }
    if ( drawn ) part_raster( draw, drawn, ymin, ymax, dst, zdc );
    rdr->part_live = live;
    rdr->part_drawn = drawn;
}
