#ifndef __D_BEAM_H__
#define __D_BEAM_H__

#include "world.h"

#define BEAM_MAX 24
#define BEAM_LIFE 0.2f
#define BEAM_TRIGGER_KEY 0x4000

typedef struct {
    BspVec3 start, end;
    float end_time;
    short key, model;
} Beam;

Beam far *d_beam_map( Renderer *rdr, short slot );
short d_beam_find( Renderer *rdr, short model, Beam *found );
void d_beam_spawn( Renderer *rdr, short key, short model,
                   BspVec3 *start, BspVec3 *end );
void d_beam_clear( Renderer *rdr );

#endif
