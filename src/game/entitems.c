/*
 * entitems.c -- the pickups at load: ents.bin's item and crate records, and
 * the drop onto the floor.
 */

#include <mem.h>    /* _fmemcpy */
#include <stdio.h>
#include <stdlib.h>

#include "entitems.h"
#include "ent.h"
#include "item.h"
#include "pl_trace.h"
#include "qgl.h"
#include "sys.h"

void ent_load_items( World *world, unsigned char far *buf, long *ofs, short count )
{
    short i;

    /* Room for the map's own plus one backpack a monster: a dead
       soldier drops one, and nothing else is ever added. */
    world->item_count = count;
    world->item_max = (short) ( count + world->mon_count );
    world->item = (ItemEnt far *) qglMemAlloc(
                     (long) ( world->item_max ? world->item_max : 1 ) * sizeof(ItemEnt) );
    if ( !world->item ) {
        sys_error( "ents: out of memory for items" );
    }

    for ( i = 0; i < count; i++ ) {
        EntsItem it;
        ItemEnt far *p = &world->item[i];

        _fmemcpy( &it, buf + *ofs, sizeof(EntsItem) ); *ofs += sizeof(EntsItem);

        p->kind   = it.kind;
        p->amount = it.amount;
        p->target = it.target;
        p->crate  = it.crate;
        p->order  = it.order;
        p->pos    = it.org;
        p->gone   = 0;
        p->rot_at = 0.0f;
    }
}

void ent_load_crates( World *world, unsigned char far *buf, long *ofs, short count )
{
    world->crate_count = count;
    world->crate = (CrateModel far *) qglMemAlloc( (long) ( count ? count : 1 ) * sizeof(CrateModel) );
    if ( !world->crate ) {
        sys_error( "ents: out of memory for crates" );
    }
    if ( count ) _fmemcpy( world->crate, buf + *ofs, (long) count * sizeof(CrateModel) );
    *ofs += (long) count * sizeof(CrateModel);
}

/*
 * The trace starts PL_FEET ABOVE PlaceItem's six-unit lift, not at it:
 * SV_ClipMoveToEntity offsets the hull's clip_mins against the item's
 * mins, and an item the map put within 24 units of its floor otherwise
 * starts inside hull 1's grown solid. The entity's own mins.z then puts
 * centered keys and powerups 24 units above the floor, while crates and
 * weapon models keep their origin on it.
 */
void ent_items_drop( World *world )
{
    short i;
    BspVec3 org, fin, mins, maxs;
    BspVec3 shift, mover_mins, mover_maxs;
    TraceResult tr;

    for ( i = 0; i < world->item_count; i++ ) {
        item_bounds( world->item[i].kind, world->item[i].crate, &mins, &maxs );
        shift.x = ( mins.x + maxs.x ) * 0.5f;
        shift.y = ( mins.y + maxs.y ) * 0.5f;
        shift.z = mins.z + PL_FEET;
        org = world->item[i].pos;
        org.x += shift.x;
        org.y += shift.y;
        org.z += shift.z + ( world->item[i].kind == ENT_ITEM_EXPLOBOX ? 2.0f : 6.0f );
        fin = org;
        fin.z -= 256.0f;
        pl_trace( world, &org, &fin, &tr );
        mover_mins.x = mins.x - shift.x;
        mover_mins.y = mins.y - shift.y;
        mover_mins.z = mins.z - shift.z;
        mover_maxs.x = maxs.x - shift.x;
        mover_maxs.y = maxs.y - shift.y;
        mover_maxs.z = maxs.z - shift.z;
        pl_trace_items( world, &org, &fin, &mover_mins, &mover_maxs,
                        i, &tr, (short *) 0 );
        if ( tr.frac < 1.0f && !tr.all_solid ) {
            if ( world->item[i].kind == ENT_ITEM_EXPLOBOX &&
                 org.z - tr.end_pos.z > 250.0f ) {
                world->item[i].gone = -1;
                continue;
            }
            world->item[i].pos = tr.end_pos;
            world->item[i].pos.x -= shift.x;
            world->item[i].pos.y -= shift.y;
            world->item[i].pos.z -= shift.z;
        } else if ( world->item[i].kind == ENT_ITEM_EXPLOBOX )
            world->item[i].pos.z += 2.0f;
        else
            world->item[i].gone = -1;
    }
}
