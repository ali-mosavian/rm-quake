#ifndef __MDL_H__
#define __MDL_H__


#include "pak.h"
#include "world.h"
#include "mdltypes.h"

/*
 * mdl.h -- one alias (.mdl) model, and the monsters spawned from it.
 * d_mdl.bas's MdlState/MdlEnt, minus the AI, which is not ported yet:
 * a monster here stands where the map put it, facing the map's way.
 *
 * The model's vertices live in EMS, whole frames to a 16K page over as
 * many pages as the frames take, and its triangles on the page after --
 * nothing in the far heap. mkmdl.py writes the page count and the frames
 * a page into the .geo header, so neither the loader nor the fetch
 * derives the layout.
 */
/*
 * name: mdl_load
 * desc: <name>.geo/.vtx/.skn out of the map container. Leaves
 *       m->loaded 0 and says why on stderr if the model will not fit
 *       or is not there.
 */
void mdl_load( Pak *pak, MdlState *m, char *name );

/* A frame's nvert*3 vertex bytes, mapped through slot. The one place
   that knows the layout: the draw and -mdlsum both fetch through it. */
unsigned char far *mdl_frame_verts( const MdlState *m, short frame, short slot );

/* The triangles, on the page after the last vertex page. */
MdlTri far *mdl_tris( const MdlState *m, short slot );

/* -mdlsum: "mdl_sum <name> <frames> <pages> <sum>" on E9 for every
   loaded model, each frame through mdl_frame_verts, every byte weighted
   by (its index in the frame-major stream % 251) + 1. */
void mdl_report_sums( World *world );

/*
 * name: mdl_load_monsters
 * desc: ents.bin's monster records, from the reader's own offset, and
 *       the models the kinds among them need.
 */
void mdl_load_monsters( World *world, unsigned char far *buf, long *ofs, short count );

/* The model for a kind, or NULL when the map spawns none of it. */
MdlState *mdl_of( World *world, short kind );

/* Which view model a PL_IT_* weapon holds: the shotgun's when the bit
   is one no gun has a model for. */
short pl_view_of( short weapon );

/*
 * name: host_view_load
 * desc: The view weapon in hand, loaded the first time it is held and
 *       never before -- a pickup, a key, the carry. Six at init is six
 *       EMS handles and six skins for the one that draws.
 */
void host_view_load( World *world, short weapon );

#endif
