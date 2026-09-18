#ifndef __D_SPRITE_H__
#define __D_SPRITE_H__

#include "world.h"

#define SPR_EXPLOSIONS 8
#define SPR_BUBBLES    64
#define SPR_SPAWNERS   8
#define SPR_EMITTERS   8

typedef struct {
    BspVec3 origin;
    float start;
    short live;
} SpriteExplosion;

typedef struct {
    BspVec3 origin, velocity;
    float next_think;
    short frame, cnt, live;
} SpriteBubble;

typedef struct {
    float next_think;
    short left;
} SpriteBubbleSpawner;

typedef struct {
    BspVec3 origin;
    float next_think;
    short live;
} SpriteBubbleEmitter;

SpriteExplosion far *d_sprite_map( Renderer *rdr, short slot );
SpriteBubble far *d_bubble_map( Renderer *rdr, short slot );
SpriteBubbleSpawner far *d_spawner_map( Renderer *rdr, short slot );
SpriteBubbleEmitter far *d_emitter_map( Renderer *rdr, short slot );
void d_sprite_explosion( Renderer *rdr, BspVec3 *origin );
void d_sprite_bubbles( Renderer *rdr, short count );
void d_sprite_air_init( Renderer *rdr, BspVec3 far *origins, short count );
void d_sprite_air_reset( Renderer *rdr, float now );
void d_sprite_tick( World *world, Renderer *rdr, Player *player, float dt );
short d_draw_sprites( World *world, Renderer *rdr, Mat4 *mtx, Mat4 *projection,
                      float xresh, float yresh, float z_near, QSurf dst );

#endif
