/*
 * mathlib.c -- the vector helpers the movers share. Stays -Ox: a door's
 * position on a given tick rides on these bits.
 */

#include "mathlib.h"

/*
 * Quake's fast inverse square root, kept from ent_door.c: this build
 * has an 8087 and could call sqrt, but the arrival test below is done
 * on the SQUARE, exactly, so only the size of an intermediate step
 * rides on the root -- and by/d is by*rsqrt(l2), so the divide goes
 * with it. Changing it would move where a door is on a given tick.
 */
static float q_rsqrt( float n )
{
    union { float f; long l; } u;
    float half = n * 0.5f;

    u.f = n;
    u.l = 0x5f3759dfL - ( u.l >> 1 );
    u.f = u.f * ( 1.5f - half * u.f * u.f );
    u.f = u.f * ( 1.5f - half * u.f * u.f );
    return u.f;
}

/* toward goal by at most `by`; 1 when it arrives */
short ent_step_to( BspVec3 far *p, BspVec3 far *goal, float by )
{
    float dx = goal->x - p->x;
    float dy = goal->y - p->y;
    float dz = goal->z - p->z;
    float l2 = dx*dx + dy*dy + dz*dz;
    float s;

    if ( l2 <= by * by ) {
        p->x = goal->x; p->y = goal->y; p->z = goal->z;
        return 1;
    }
    s = by * q_rsqrt( l2 );
    p->x += dx * s;
    p->y += dy * s;
    p->z += dz * s;
    return 0;
}
