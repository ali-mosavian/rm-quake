#ifndef __LIGHTS_H__
#define __LIGHTS_H__

#include "world.h"
#include "ls.h"

/* light_use on every light named id, stamping each from *clock. */
void ent_lights_use( LightEnt far *l, short n, short *clock, short id );

/* Each style from the light on it stamped last; ent_lights_sync is this
   on the map's lights, once a tick. */
void ent_lights_apply( LightEnt far *l, short n, LightStyles *ls );
void ent_lights_sync( World *world, LightStyles *ls );

/* 1, or the negative number of the check that failed. */
short ent_lights_selftest( void );

#endif
