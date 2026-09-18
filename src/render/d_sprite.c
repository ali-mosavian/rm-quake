/* d_sprite.c -- view-parallel Quake sprites with index 255 transparent. */

#include <mem.h>

#include "d_sprite.h"
#include "sprite.h"
#include "qgl.h"

#define SPR_SLOT 2

typedef char SpritePoolFits[
    ( sizeof(SpriteExplosion) * SPR_EXPLOSIONS <= RDR_SPRITE_BYTES ) ? 1 : -1 ];

static QVert qv[4];

SpriteExplosion far *d_sprite_map( Renderer *rdr, short slot )
{
    unsigned char far *row;
    if ( !rdr->frame_dc ) return 0;
    row = (unsigned char far *) qglSfAccessWrEx( rdr->frame_dc, 3, slot );
    return (SpriteExplosion far *) row;
}

void d_sprite_explosion( Renderer *rdr, BspVec3 *origin )
{
    SpriteExplosion far *pool;
    SpriteExplosion ex;
    short i, use = -1;
    float oldest = 1.0e30f;

    pool = d_sprite_map( rdr, SPR_SLOT );
    if ( !pool ) return;
    for ( i = 0; i < SPR_EXPLOSIONS; i++ ) {
        _fmemcpy( &ex, &pool[i], sizeof(ex) );
        if ( !ex.live || rdr->anim_time - ex.start >= 0.6f ) { use = i; break; }
        if ( ex.start < oldest ) { oldest = ex.start; use = i; }
    }
    ex.origin = *origin;
    ex.start = rdr->anim_time;
    ex.live = -1;
    _fmemcpy( &pool[use], &ex, sizeof(ex) );
    rdr->sprite_live = 1;
}

short d_draw_sprites( World *world, Renderer *rdr, Mat4 *mtx, Mat4 *projection,
                      float xresh, float yresh, float z_near, QSurf dst )
{
    SpriteExplosion far *pool;
    SpriteExplosion ex;
    SpriteExplosion work[SPR_EXPLOSIONS];
    SpriteFrame sf;
    QSurf tex;
    float *m = (float *) mtx;
    float cx, cy, cw, rw, pu, pv;
    float u1, v1;
    short i, frame, drawn = 0, live = 0;

    rdr->sprite_drawn = 0;
    if ( rdr->sprite_live <= 0 ) return 0;
    pool = d_sprite_map( rdr, SPR_SLOT );
    if ( !pool ) return 0;
    _fmemcpy( work, pool, sizeof(work) );
    qglSfZMode( dst, QGL_Z_TEST );
    for ( i = 0; i < SPR_EXPLOSIONS; i++ ) {
        ex = work[i];
        if ( !ex.live ) continue;
        frame = (short) ( ( rdr->anim_time - ex.start ) * 10.0f );
        if ( frame < 0 || frame >= 6 ) {
            work[i].live = 0;
            continue;
        }
        live++;
        cw = ex.origin.x*m[3] + ex.origin.z*m[7] + ex.origin.y*m[11] + m[15];
        if ( cw < z_near ) continue;
        cx = ex.origin.x*m[0] + ex.origin.z*m[4] + ex.origin.y*m[8] + m[12];
        cy = ex.origin.x*m[1] + ex.origin.z*m[5] + ex.origin.y*m[9] + m[13];
        tex = sprite_frame( world, SPR_MODEL_EXPLOD, frame, &sf );
        if ( !tex ) continue;
        rw = 1.0f / cw;
        pu = projection->m11;
        pv = projection->m22;
        u1 = (float) ( sf.right - sf.left ) / (float) sf.tw * rw;
        v1 = (float) ( sf.up - sf.down ) / (float) sf.th * rw;

        qv[0].x = xresh + ( cx + sf.left  * pu ) * rw * xresh;
        qv[0].y = yresh - ( cy + sf.up    * pv ) * rw * yresh;
        qv[1].x = xresh + ( cx + sf.right * pu ) * rw * xresh;
        qv[1].y = qv[0].y;
        qv[2].x = qv[1].x;
        qv[2].y = yresh - ( cy + sf.down  * pv ) * rw * yresh;
        qv[3].x = qv[0].x;
        qv[3].y = qv[2].y;
        qv[0].u = 0.0f; qv[0].v = 0.0f;
        qv[1].u = u1;   qv[1].v = 0.0f;
        qv[2].u = u1;   qv[2].v = v1;
        qv[3].u = 0.0f; qv[3].v = v1;
        qv[0].z = qv[1].z = qv[2].z = qv[3].z = rw;
        if ( qglRsPoly( dst, (void far *) qv, 4, QGL_M_MATEX, tex ) > 0 ) drawn++;
    }
    pool = d_sprite_map( rdr, SPR_SLOT );
    if ( pool ) _fmemcpy( pool, work, sizeof(work) );
    rdr->sprite_live = live;
    rdr->sprite_drawn = drawn;
    return drawn;
}
