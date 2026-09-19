#include <assert.h>
#include <math.h>
#include <string.h>

#include "viewfx.h"

static void close_to( float got, float want )
{
    assert( fabs( got - want ) < 0.0001f );
}

int main( void )
{
    Camera base;
    Camera view;
    Player player;
    Vec3 up;

    memset( &base, 0, sizeof(base) );
    memset( &player, 0, sizeof(player) );
    base.pos.x = 1.0f; base.pos.y = 2.0f; base.pos.z = 3.0f;
    base.look_at.x = 1.0f; base.look_at.y = 2.0f; base.look_at.z = 4.0f;
    player.vel.x = 200.0f;

    close_to( v_view_bob( &player, 0.15f ), 4.0f );
    close_to( v_view_bob( &player, 0.45f ), -1.6f );
    close_to( v_view_roll( &base, &player ), 2.0f );

    v_render_effects( &view, &up, &base, &player, 0, 22.0f, 22.0f, 0.15f, 0.0f );
    close_to( view.pos.x, 1.03125f );
    close_to( view.pos.y, 6.03125f );
    close_to( view.pos.z, 3.03125f );
    close_to( view.look_at.z - view.pos.z, 1.0f );
    assert( up.x > 0.03f && up.y > 0.99f && fabs( up.z ) < 0.0001f );

    v_render_effects( &view, &up, &base, &player, -1, 22.0f, -8.0f, 0.15f, 0.0f );
    close_to( view.pos.y, -23.96875f );
    close_to( view.look_at.y, -23.96875f );
    assert( up.x > 0.98f && up.y > 0.17f && fabs( up.z ) < 0.0001f );

    v_render_effects( &view, &up, &base, &player, -1, 22.0f, 8.0f, 0.15f, 0.0f );
    close_to( view.pos.y, -7.96875f );
    close_to( view.look_at.y, -7.96875f );

    /* QDS1 already stores player.qc's viewheight, but still needs roll. */
    v_render_effects( &view, &up, &base, &player, -1, 22.0f, 22.0f, 0.15f, 0.0f );
    close_to( view.pos.y, 6.03125f );
    assert( up.x > 0.98f && up.y > 0.17f );

    v_render_effects( &view, &up, &base, &player, 0, 22.0f, 22.0f, 0.15f, -2.0f );
    assert( view.look_at.y - view.pos.y > 0.03f );
    return 0;
}
