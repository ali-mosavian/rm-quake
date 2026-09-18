/* d_beam.c -- id's keyed, short-lived client lightning beam pool. */

#include <mem.h>

#include "d_beam.h"
#include "qgl.h"

#define BEAM_SLOT 2

typedef char BeamPoolFits[
    ( sizeof(Beam) * BEAM_MAX <= RDR_BEAM_BYTES ) ? 1 : -1 ];

Beam far *d_beam_map( Renderer *rdr, short slot )
{
    unsigned char far *row;

    if ( !rdr->frame_dc ) return 0;
    row = (unsigned char far *) qglSfAccessWrEx( rdr->frame_dc, 3, slot );
    return row ? (Beam far *) ( row + RDR_BEAM_OFS ) : 0;
}

short d_beam_find( Renderer *rdr, short model, Beam *found )
{
    Beam far *pool = d_beam_map( rdr, BEAM_SLOT );
    short i;

    if ( !pool ) return 0;
    for ( i = 0; i < BEAM_MAX; i++ ) {
        _fmemcpy( found, &pool[i], sizeof(*found) );
        if ( found->key && found->model == model &&
             found->end_time >= rdr->anim_time ) return -1;
    }
    return 0;
}

void d_beam_spawn( Renderer *rdr, short key, short model,
                   BspVec3 *start, BspVec3 *end )
{
    Beam far *pool;
    Beam b;
    short i, use = -1, active = 0, was_active = 0;

    if ( key <= 0 ) return;
    pool = d_beam_map( rdr, BEAM_SLOT );
    if ( !pool ) return;

    /* CL_ParseBeam: replace this entity first, otherwise take the first
       empty or expired slot. A full pool drops the new beam. */
    for ( i = 0; i < BEAM_MAX; i++ ) {
        _fmemcpy( &b, &pool[i], sizeof(b) );
        if ( b.key && b.end_time >= rdr->anim_time ) active++;
        if ( b.key == key ) { use = i; was_active = (short) ( b.end_time >= rdr->anim_time ); }
    }
    if ( use < 0 ) for ( i = 0; i < BEAM_MAX; i++ ) {
        _fmemcpy( &b, &pool[i], sizeof(b) );
        if ( !b.key || b.end_time < rdr->anim_time ) { use = i; break; }
    }
    if ( use < 0 ) { rdr->beam_live = active; return; }

    b.start = *start;
    b.end = *end;
    b.end_time = rdr->anim_time + BEAM_LIFE;
    b.key = key;
    b.model = model;
    _fmemcpy( &pool[use], &b, sizeof(b) );
    rdr->beam_live = (short) ( active + ( was_active ? 0 : 1 ) );
}

void d_beam_clear( Renderer *rdr )
{
    Beam far *pool = d_beam_map( rdr, BEAM_SLOT );

    if ( pool ) _fmemset( pool, 0, sizeof(Beam) * BEAM_MAX );
    rdr->beam_live = rdr->beam_drawn = rdr->beam_segments = 0;
}
