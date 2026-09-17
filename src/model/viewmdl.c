/* viewmdl.c -- the one view weapon model resident at a time. */

#include "mdl.h"
#include "fight.h"

/* q_pl.bi's weapon bits to this project's own view-model order. */
short pl_view_of( short weapon )
{
    switch ( weapon ) {
    case PL_IT_SSG:     return PL_VIEW_SSG;
    case PL_IT_NAILGUN: return PL_VIEW_NAIL;
    case PL_IT_GL:      return PL_VIEW_GL;
    case PL_IT_SNG:     return PL_VIEW_SNG;
    case PL_IT_RL:      return PL_VIEW_RL;
    }
    return PL_VIEW_SHOT;
}

static char *vmdl_name[PL_VIEW_N] = {
    "v_shot", "v_shot2", "v_nail", "v_rock", "v_nail2", "v_rock2" };

char *pl_view_name( short view ) { return vmdl_name[view]; }

void host_view_load( World *world, short weapon )
{
    short i, active = pl_view_of( weapon );

    for ( i = 0; i < PL_VIEW_N; i++ )
        if ( i != active && ( world->vmdl[i].loaded || world->vmdl[i].ntri ) )
            mdl_unload( &world->vmdl[i] );

    /* loaded stays 0 when the model is not in the container, and
       mdl_load says so once; asking again every frame would say it
       every frame, so a failed load is remembered as a try. */
    if ( world->vmdl[active].loaded || world->vmdl[active].ntri ) return;
    mdl_load( &world->pak, &world->vmdl[active], pl_view_name( active ), MDL_VIEW_SETS );
    if ( !world->vmdl[active].loaded ) world->vmdl[active].ntri = -1;
}
