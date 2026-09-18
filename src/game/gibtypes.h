#ifndef __GIBTYPES_H__
#define __GIBTYPES_H__

#include "bsptypes.h"

#define GIB_MAX 64
#define GIB_HEAD   1
#define GIB_GROUND 2

/* Transient ThrowGib/ThrowHead state in frame_dc's one EMS page. */
typedef struct {
    BspVec3 pos, vel, ang;
    short   avel[3];
    float   die_at;             /* zero: a monster head stays for the map */
    unsigned char model;
    unsigned char flags;
} GibEnt;

#endif
