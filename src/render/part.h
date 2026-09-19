#ifndef __PART_H__
#define __PART_H__

#include "renderer.h"

#define PART_MAX             2048
#define PART_CHUNK            512
#define PART_PAGES              4
#define PART_SURF_ROWS          8
#define PART_STATE_PAGE_BYTES 16384
#define PART_STATE_STRIDE     8192
#define PART_EMS_SLOT            3
#define PART_DRAW_SLOT           0

short part_init( Renderer *rdr );
void  part_shutdown( Renderer *rdr );
short part_signed_selftest( void );
void  part_explosion( Renderer *rdr, BspVec3 *origin );
void  part_effect( Renderer *rdr, BspVec3 *origin, BspVec3 *direction,
                   short color, short count );
void  part_trail( Renderer *rdr, BspVec3 *start, BspVec3 *end, short kind );
void  part_teleport( Renderer *rdr, BspVec3 *origin );
void  part_lava( Renderer *rdr, BspVec3 *origin );
void  part_frame( Renderer *rdr, Mat4 *mtx, float xresh, float yresh,
                  float z_near, QSurf dst, QSurf zdc );

#endif
