/* d_sprite.c -- view-parallel Quake sprites with index 255 transparent. */

#include <mem.h>

#include "d_sprite.h"
#include "sprite.h"
#include "qgl.h"

#define SPR_SLOT 2
#define BUBBLE_OFS 256
#define SPAWNER_OFS ( BUBBLE_OFS + sizeof(SpriteBubble) * SPR_BUBBLES )
#define EMITTER_OFS ( SPAWNER_OFS + sizeof(SpriteBubbleSpawner) * SPR_SPAWNERS )

typedef char SpritePoolFits[
    ( EMITTER_OFS + sizeof(SpriteBubbleEmitter) * SPR_EMITTERS <= RDR_BEAM_OFS ) ? 1 : -1 ];

static QVert qv[4];

SpriteExplosion far *d_sprite_map( Renderer *rdr, short slot )
{
    unsigned char far *row;
    if ( !rdr->frame_dc ) return 0;
    row = (unsigned char far *) qglSfAccessWrEx( rdr->frame_dc, 3, slot );
    return (SpriteExplosion far *) row;
}

SpriteBubble far *d_bubble_map( Renderer *rdr, short slot )
{
    unsigned char far *row;
    if ( !rdr->frame_dc ) return 0;
    row = (unsigned char far *) qglSfAccessWrEx( rdr->frame_dc, 3, slot );
    return row ? (SpriteBubble far *) ( row + BUBBLE_OFS ) : 0;
}

SpriteBubbleSpawner far *d_spawner_map( Renderer *rdr, short slot )
{
    unsigned char far *row;
    if ( !rdr->frame_dc ) return 0;
    row = (unsigned char far *) qglSfAccessWrEx( rdr->frame_dc, 3, slot );
    return row ? (SpriteBubbleSpawner far *) ( row + SPAWNER_OFS ) : 0;
}

SpriteBubbleEmitter far *d_emitter_map( Renderer *rdr, short slot )
{
    unsigned char far *row;
    if ( !rdr->frame_dc ) return 0;
    row = (unsigned char far *) qglSfAccessWrEx( rdr->frame_dc, 3, slot );
    return row ? (SpriteBubbleEmitter far *) ( row + EMITTER_OFS ) : 0;
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

void d_sprite_bubbles( Renderer *rdr, short count )
{
    SpriteBubbleSpawner far *spawner;
    short i;

    if ( count <= 0 ) return;
    spawner = d_spawner_map( rdr, SPR_SLOT );
    if ( !spawner ) return;
    for ( i = 0; i < SPR_SPAWNERS; i++ ) if ( spawner[i].left == 0 ) {
        spawner[i].left = count;
        spawner[i].next_think = rdr->anim_time + 0.1f;
        rdr->bubble_spawn_left = (short) ( rdr->bubble_spawn_left + count );
        return;
    }
}

void d_sprite_air_init( Renderer *rdr, BspVec3 far *origins, short count )
{
    SpriteBubbleEmitter far *emitter;
    SpriteBubbleEmitter e;
    short i;

    if ( count < 0 ) count = 0;
    if ( count > SPR_EMITTERS ) count = SPR_EMITTERS;
    emitter = d_emitter_map( rdr, SPR_SLOT );
    if ( !emitter ) return;
    _fmemset( &e, 0, sizeof(e) );
    for ( i = 0; i < SPR_EMITTERS; i++ ) {
        if ( i < count ) {
            _fmemcpy( &e.origin, &origins[i], sizeof(e.origin) );
            e.next_think = rdr->anim_time + 1.0f;
            e.live = -1;
        } else {
            _fmemset( &e, 0, sizeof(e) );
        }
        _fmemcpy( &emitter[i], &e, sizeof(e) );
    }
    rdr->bubble_emitters = count;
}

void d_sprite_air_reset( Renderer *rdr, float now )
{
    SpriteBubbleEmitter far *emitter;
    short i;

    emitter = d_emitter_map( rdr, SPR_SLOT );
    if ( !emitter ) return;
    for ( i = 0; i < SPR_EMITTERS; i++ )
        if ( emitter[i].live ) emitter[i].next_think = now + 1.0f;
}

static short bubble_free( SpriteBubble far *bubble )
{
    short i;
    for ( i = 0; i < SPR_BUBBLES; i++ ) if ( !bubble[i].live ) return i;
    return -1;
}

void d_sprite_tick( World *world, Renderer *rdr, Player *player, float dt )
{
    SpriteBubble far *pool;
    SpriteBubbleSpawner far *spawner;
    SpriteBubbleEmitter far *emitter;
    SpriteBubble bubble, child;
    SpriteBubbleSpawner spawn;
    SpriteBubbleEmitter emit;
    float r1, r2, r3, think_time, tick_end = rdr->anim_time + dt;
    short i, n, live, pending;

    if ( rdr->bubble_live <= 0 && rdr->bubble_spawn_left <= 0 &&
         rdr->bubble_emitters <= 0 ) return;
    pool = d_bubble_map( rdr, SPR_SLOT );
    if ( !pool ) return;

    spawner = d_spawner_map( rdr, SPR_SLOT );
    if ( !spawner ) return;
    for ( i = 0; i < SPR_SPAWNERS; i++ ) {
        _fmemcpy( &spawn, &spawner[i], sizeof(spawn) );
        if ( spawn.left <= 0 || spawn.next_think > tick_end ) continue;
        think_time = spawn.next_think;
        if ( think_time < rdr->anim_time ) think_time = rdr->anim_time;
        if ( player->water_level != 3 ) spawn.left = 0;
        else if ( ( n = bubble_free( pool ) ) >= 0 ) {
            _fmemset( &bubble, 0, sizeof(bubble) );
            bubble.origin = player->pos;
            bubble.origin.z += 24.0f;
            bubble.velocity.z = 15.0f;
            bubble.next_think = think_time + 0.5f;
            bubble.live = -1;
            _fmemcpy( &pool[n], &bubble, sizeof(bubble) );
            spawn.left--;
        }
        spawn.next_think = think_time + 0.1f;
        _fmemcpy( &spawner[i], &spawn, sizeof(spawn) );
    }

    emitter = d_emitter_map( rdr, SPR_SLOT );
    if ( !emitter ) return;
    for ( i = 0; i < SPR_EMITTERS; i++ ) {
        _fmemcpy( &emit, &emitter[i], sizeof(emit) );
        if ( !emit.live || emit.next_think > tick_end ) continue;
        think_time = emit.next_think;
        if ( think_time < rdr->anim_time ) think_time = rdr->anim_time;
        if ( ( n = bubble_free( pool ) ) >= 0 ) {
            _fmemset( &bubble, 0, sizeof(bubble) );
            bubble.origin = emit.origin;
            bubble.velocity.z = 15.0f;
            bubble.next_think = think_time + 0.5f;
            bubble.live = -1;
            _fmemcpy( &pool[n], &bubble, sizeof(bubble) );
            emit.next_think = think_time + 0.5f + rng_float( &world->rng );
        } else {
            emit.next_think = think_time + 0.1f;
        }
        _fmemcpy( &emitter[i], &emit, sizeof(emit) );
    }

    for ( i = 0; i < SPR_BUBBLES; i++ ) {
        _fmemcpy( &bubble, &pool[i], sizeof(bubble) );
        if ( !bubble.live ) continue;
        if ( bubble.next_think <= tick_end ) {
            think_time = bubble.next_think;
            if ( think_time < rdr->anim_time ) think_time = rdr->anim_time;
            bubble.cnt++;
            if ( bubble.cnt == 4 ) {
                child = bubble;
                child.frame = 1; child.cnt = 10;
                child.next_think = think_time + 0.5f;
                if ( ( n = bubble_free( pool ) ) >= 0 )
                    _fmemcpy( &pool[n], &child, sizeof(child) );
                bubble.frame = 1;
                bubble.cnt = 10;
                /* MOVETYPE_NOCLIP never runs SV_CheckWater, so id's
                   waterlevel test removes the original after splitting. */
                bubble.live = 0;
            }
            if ( bubble.cnt == 20 ) bubble.live = 0;
            r1 = bubble.velocity.x + ( -10.0f + rng_float( &world->rng ) * 20.0f );
            r2 = bubble.velocity.y + ( -10.0f + rng_float( &world->rng ) * 20.0f );
            r3 = bubble.velocity.z + 10.0f + rng_float( &world->rng ) * 10.0f;
            if ( r1 > 10.0f ) r1 = 5.0f; else if ( r1 < -10.0f ) r1 = -5.0f;
            if ( r2 > 10.0f ) r2 = 5.0f; else if ( r2 < -10.0f ) r2 = -5.0f;
            if ( r3 < 10.0f ) r3 = 15.0f; else if ( r3 > 30.0f ) r3 = 25.0f;
            bubble.velocity.x = r1; bubble.velocity.y = r2; bubble.velocity.z = r3;
            bubble.next_think = think_time + 0.5f;
        }
        if ( bubble.live ) {
            bubble.origin.x += bubble.velocity.x * dt;
            bubble.origin.y += bubble.velocity.y * dt;
            bubble.origin.z += bubble.velocity.z * dt;
        }
        _fmemcpy( &pool[i], &bubble, sizeof(bubble) );
    }
    live = 0;
    for ( i = 0; i < SPR_BUBBLES; i++ ) if ( pool[i].live ) live++;
    pending = 0;
    for ( i = 0; i < SPR_SPAWNERS; i++ ) pending += spawner[i].left;
    rdr->bubble_live = live;
    rdr->bubble_spawn_left = pending;
}

static short sprite_draw_one( World *world, BspVec3 *origin, short model, short frame,
                              float *m, Mat4 *projection, float xresh, float yresh,
                              float z_near, QSurf dst )
{
    SpriteFrame sf;
    QSurf tex;
    float cx, cy, cw, rw, pu, pv;
    float u1, v1;

    cw = origin->x*m[3] + origin->z*m[7] + origin->y*m[11] + m[15];
    if ( cw < z_near ) return 0;
    cx = origin->x*m[0] + origin->z*m[4] + origin->y*m[8] + m[12];
    cy = origin->x*m[1] + origin->z*m[5] + origin->y*m[9] + m[13];
    tex = sprite_frame( world, model, frame, &sf );
    if ( !tex ) return 0;
    rw = 1.0f / cw;
    pu = projection->m11; pv = projection->m22;
    u1 = (float) ( sf.right - sf.left ) / (float) sf.tw * rw;
    v1 = (float) ( sf.up - sf.down ) / (float) sf.th * rw;
    qv[0].x = xresh + ( cx + sf.left  * pu ) * rw * xresh;
    qv[0].y = yresh - ( cy + sf.up    * pv ) * rw * yresh;
    qv[1].x = xresh + ( cx + sf.right * pu ) * rw * xresh; qv[1].y = qv[0].y;
    qv[2].x = qv[1].x; qv[2].y = yresh - ( cy + sf.down * pv ) * rw * yresh;
    qv[3].x = qv[0].x; qv[3].y = qv[2].y;
    qv[0].u = 0.0f; qv[0].v = 0.0f;
    qv[1].u = u1;   qv[1].v = 0.0f;
    qv[2].u = u1;   qv[2].v = v1;
    qv[3].u = 0.0f; qv[3].v = v1;
    qv[0].z = qv[1].z = qv[2].z = qv[3].z = rw;
    return (short) ( qglRsPoly( dst, (void far *) qv, 4, QGL_M_MATEX, tex ) > 0 );
}

short d_draw_sprites( World *world, Renderer *rdr, Mat4 *mtx, Mat4 *projection,
                      float xresh, float yresh, float z_near, QSurf dst )
{
    SpriteExplosion far *epool;
    SpriteBubble far *bpool;
    SpriteExplosion explosion[SPR_EXPLOSIONS];
    SpriteBubble bubble;
    float *m = (float *) mtx;
    short i, frame, elive = 0, edrawn = 0, bdrawn = 0, bleft = rdr->bubble_live;

    rdr->sprite_drawn = rdr->bubble_drawn = 0;
    if ( rdr->sprite_live <= 0 && rdr->bubble_live <= 0 ) return 0;
    epool = d_sprite_map( rdr, SPR_SLOT );
    if ( !epool ) return 0;
    _fmemcpy( explosion, epool, sizeof(explosion) );
    qglSfZMode( dst, QGL_Z_TEST );
    for ( i = 0; i < SPR_EXPLOSIONS; i++ ) {
        if ( !explosion[i].live ) continue;
        frame = (short) ( ( rdr->anim_time - explosion[i].start ) * 10.0f );
        if ( frame < 0 || frame >= 6 ) { explosion[i].live = 0; continue; }
        elive++;
        edrawn += sprite_draw_one( world, &explosion[i].origin, SPR_MODEL_EXPLOD,
                    frame, m, projection, xresh, yresh, z_near, dst );
    }
    for ( i = 0; i < SPR_BUBBLES && bleft > 0; i++ ) {
        bpool = d_bubble_map( rdr, SPR_SLOT );
        if ( !bpool ) break;
        _fmemcpy( &bubble, &bpool[i], sizeof(bubble) );
        if ( bubble.live ) {
            bleft--;
            bdrawn += sprite_draw_one( world, &bubble.origin, SPR_MODEL_BUBBLE,
                        bubble.frame, m, projection, xresh, yresh, z_near, dst );
        }
    }
    epool = d_sprite_map( rdr, SPR_SLOT );
    if ( epool ) _fmemcpy( epool, explosion, sizeof(explosion) );
    rdr->sprite_live = elive; rdr->sprite_drawn = edrawn;
    rdr->bubble_drawn = bdrawn;
    return (short) ( edrawn + bdrawn );
}
