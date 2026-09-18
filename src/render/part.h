#ifndef __PART_H__
#define __PART_H__

#include "renderer.h"

#define PART_BYTES    4096
#define PART_MAX      112
#define PART_EMS_SLOT 3

short part_init( Renderer *rdr );
void  part_shutdown( Renderer *rdr );
void  part_explosion( Renderer *rdr, BspVec3 *origin );
void  part_frame( Renderer *rdr, Mat4 *mtx, float xresh, float yresh,
                  float z_near, QSurf dst, QSurf zdc );

#endif
