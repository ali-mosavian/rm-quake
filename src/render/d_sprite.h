#ifndef __D_SPRITE_H__
#define __D_SPRITE_H__

#include "world.h"

#define SPR_EXPLOSIONS 8

typedef struct {
    BspVec3 origin;
    float start;
    short live;
} SpriteExplosion;

SpriteExplosion far *d_sprite_map( Renderer *rdr, short slot );
void d_sprite_explosion( Renderer *rdr, BspVec3 *origin );
short d_draw_sprites( World *world, Renderer *rdr, Mat4 *mtx, Mat4 *projection,
                      float xresh, float yresh, float z_near, QSurf dst );

#endif
