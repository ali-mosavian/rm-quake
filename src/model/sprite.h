#ifndef __SPRITE_H__
#define __SPRITE_H__

#include "world.h"

void sprite_load( World *world );
void sprite_unload( World *world );
QSurf sprite_frame( World *world, short model, short frame, SpriteFrame *out );

#endif
