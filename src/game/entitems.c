/*
 * entitems.c -- the pickups at load: ents.bin's item and crate records, and
 * the drop onto the floor.
 */

#include <mem.h>    /* _fmemcpy */
#include <stdio.h>
#include <stdlib.h>

#include "entitems.h"
#include "ent.h"
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
 * The trace starts PL_FEET ABOVE the origin, not at it:
 * SV_ClipMoveToEntity offsets the hull's clip_mins against the item's
 * mins, and an item the map put within 24 units of its floor otherwise
 * starts inside hull 1's grown solid. Where a room lay under it within
 * 256 it fell through; where none did it stopped 24 above the floor and
 * floated.
 */
void ent_items_drop( World *world )
{
    short i;
    BspVec3 org, fin;
    TraceResult tr;

    for ( i = 0; i < world->item_count; i++ ) {
        org = world->item[i].pos;
        org.z += PL_FEET;
        fin = org;
        fin.z -= 256.0f;
        pl_trace( world, &org, &fin, &tr );
        if ( tr.frac < 1.0f && !tr.all_solid ) {
            world->item[i].pos = tr.end_pos;
            world->item[i].pos.z -= PL_FEET;
        }
    }
}
