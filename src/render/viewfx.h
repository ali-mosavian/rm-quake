#ifndef __VIEWFX_H__
#define __VIEWFX_H__

#include "renderer.h"

float v_view_bob( const Player *player, float time );
float v_view_roll( const Camera *cam, const Player *player );
void  v_render_effects( Camera *out, Vec3 *up, const Camera *base,
                        const Player *player, short dead, float base_height,
                        float dead_height, float time );

#endif
