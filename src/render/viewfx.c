/* id's V_CalcBob and V_CalcRoll, kept apart from movement because neither
   changes the player: both are render-only offsets of the physical camera. */

#include <math.h>

#include "viewfx.h"

#define V_PI         3.14159265f
#define V_BOB        0.02f
#define V_BOBCYCLE   0.6f
#define V_BOBUP      0.5f
#define V_ROLLSPEED  200.0f
#define V_ROLLANGLE  2.0f
#define V_NUDGE      0.03125f
#define V_DEAD_ROLL  80.0f

static void v_forward_right( const Camera *cam, Vec3 *forward, Vec3 *right )
{
    float length;

    forward->x = cam->look_at.x - cam->pos.x;
    forward->y = cam->look_at.y - cam->pos.y;
    forward->z = cam->look_at.z - cam->pos.z;
    length = (float) sqrt( forward->x * forward->x + forward->y * forward->y +
                           forward->z * forward->z );
    if ( length < 0.0001f ) {
        forward->x = 0.0f; forward->y = 0.0f; forward->z = 1.0f;
    } else {
        forward->x /= length; forward->y /= length; forward->z /= length;
    }

    right->x = forward->z;
    right->y = 0.0f;
    right->z = -forward->x;
    length = (float) sqrt( right->x * right->x + right->z * right->z );
    if ( length < 0.0001f ) {
        right->x = 1.0f; right->y = 0.0f; right->z = 0.0f;
    } else {
        right->x /= length; right->z /= length;
    }
}

float v_view_bob( const Player *player, float time )
{
    float cycle, bob;

    cycle = time - (float) ( (long) ( time / V_BOBCYCLE ) ) * V_BOBCYCLE;
    cycle /= V_BOBCYCLE;
    if ( cycle < V_BOBUP ) cycle = V_PI * cycle / V_BOBUP;
    else cycle = V_PI + V_PI * ( cycle - V_BOBUP ) / ( 1.0f - V_BOBUP );

    bob = (float) sqrt( player->vel.x * player->vel.x + player->vel.y * player->vel.y ) * V_BOB;
    bob = bob * 0.3f + bob * 0.7f * (float) sin( cycle );
    if ( bob > 4.0f ) return 4.0f;
    if ( bob < -7.0f ) return -7.0f;
    return bob;
}

float v_view_roll( const Camera *cam, const Player *player )
{
    Vec3 forward, right;
    float side;

    v_forward_right( cam, &forward, &right );
    side = player->vel.x * right.x + player->vel.y * right.z;
    if ( side < -V_ROLLSPEED ) return -V_ROLLANGLE;
    if ( side > V_ROLLSPEED ) return V_ROLLANGLE;
    return side * V_ROLLANGLE / V_ROLLSPEED;
}

void v_render_effects( Camera *out, Vec3 *up, const Camera *base,
                       const Player *player, short dead, float base_height,
                       float dead_height, float time )
{
    Vec3 forward, right, upright;
    float bob, roll, cs, sn;

    *out = *base;
    bob = v_view_bob( player, time );
    if ( dead ) bob += dead_height - base_height;
    out->pos.x += V_NUDGE; out->pos.y += bob + V_NUDGE; out->pos.z += V_NUDGE;
    out->look_at.x += V_NUDGE; out->look_at.y += bob + V_NUDGE; out->look_at.z += V_NUDGE;

    v_forward_right( out, &forward, &right );
    upright.x = forward.y * right.z - forward.z * right.y;
    upright.y = forward.z * right.x - forward.x * right.z;
    upright.z = forward.x * right.y - forward.y * right.x;
    roll = ( dead ? V_DEAD_ROLL : v_view_roll( base, player ) ) * V_PI / 180.0f;
    cs = (float) cos( roll );
    sn = (float) sin( roll );
    up->x = upright.x * cs + right.x * sn;
    up->y = upright.y * cs + right.y * sn;
    up->z = upright.z * cs + right.z * sn;
}
