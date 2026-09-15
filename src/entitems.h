#ifndef __ENTITEMS_H__
#define __ENTITEMS_H__

#include "world.h"

/*
 * name: ent_load_items
 * desc: ents.bin's item records, from the reader's own offset. Reads
 *       forward and leaves *ofs past the last one, like every other
 *       loader in ent.c.
 */
void ent_load_items( World *world, unsigned char far *buf, long *ofs, short count );

/*
 * name: ent_load_crates
 * desc: The b_*.bsp boxes, straight out of ents.bin -- they sit between
 *       the corners and the messages, and only d_alias.c reads them.
 */
void ent_load_crates( World *world, unsigned char far *buf, long *ofs, short count );

/*
 * name: ent_items_drop
 * desc: Every item onto the floor under it, once, at load.
 */
void ent_items_drop( World *world );

#endif
