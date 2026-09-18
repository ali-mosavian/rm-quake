#ifndef __PL_TRACE_H__
#define __PL_TRACE_H__

#include "renderer.h"
#include "world.h"
#include "pl_move.h"    /* TraceResult */

/*
 * name: pl_trace
 * desc: Sweeps the player hull from start to fin, against the world
 *       and every solid brush entity, leaving the closest hit in tr.
 */
void pl_trace( World *world, BspVec3 *start, BspVec3 *fin, TraceResult *tr );

/* Hull-0 point line, including solid brush entities. */
void pl_trace_point( World *world, BspVec3 *start, BspVec3 *fin, TraceResult *tr );

/* Merge live exploding-box BBOXes into an existing brush trace. */
void pl_trace_items( World *world, BspVec3 *start, BspVec3 *fin,
                     BspVec3 *mover_mins, BspVec3 *mover_maxs,
                     short ignore_item, TraceResult *tr, short *hit_item );

/* Hull-1 movement against brushes and live exploding boxes. */
void pl_trace_move( World *world, BspVec3 *start, BspVec3 *fin,
                    short ignore_item, TraceResult *tr, short *hit_item );

#endif
