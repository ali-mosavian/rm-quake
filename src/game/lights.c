/*
 * lights.c -- switchable lights: light_use, and which light each style shows.
 */

#include "lights.h"

void ent_lights_use( LightEnt far *l, short n, short *clock, short id )
{
    short k;

    for ( k = 0; k < n; k++ ) {
        if ( l[k].name != id ) continue;
        l[k].on    = (short) !l[k].on;
        l[k].stamp = ++*clock;
    }
}

void ent_lights_apply( LightEnt far *l, short n, LightStyles *ls )
{
    short j, k;

    for ( k = 0; k < n; k++ ) {
        for ( j = 0; j < n; j++ )
            if ( l[j].style == l[k].style && l[j].stamp > l[k].stamp ) break;
        if ( j == n ) ls_switch( ls, l[k].style, l[k].on );
    }
}

void ent_lights_sync( World *world, LightStyles *ls )
{
    ent_lights_apply( world->light, world->light_count, ls );
}

/* Two lights on style 40 and one on 41, the first and third sharing a
   name: the style shows the light used last, whatever the map's order. */
short ent_lights_selftest( void )
{
    LightEnt l[3];
    LightStyles ls;
    short clock = 3, k;

    for ( k = 0; k < 3; k++ ) {
        l[k].name = (short) ( k == 1 ? 2 : 1 );
        l[k].style = (short) ( k == 2 ? 41 : 40 );
        l[k].on = l[k].start_on = (short) ( k != 2 );
        l[k].stamp = k;
    }
    ls_init( &ls );

    ent_lights_apply( l, 3, &ls );
    if ( ls.tab[40].value != LS_NEUTRAL || ls.tab[41].value != 0 ) return -1;

    ent_lights_use( l, 3, &clock, 1 );          /* the first off, the third on */
    ent_lights_apply( l, 3, &ls );
    if ( ls.tab[40].value != 0 || ls.tab[41].value != LS_NEUTRAL ) return -2;

    ent_lights_use( l, 3, &clock, 2 );          /* the second off */
    ent_lights_use( l, 3, &clock, 1 );          /* the first back on, used last */
    ent_lights_apply( l, 3, &ls );
    if ( ls.tab[40].value != LS_NEUTRAL ) return -3;
    return 1;
}
