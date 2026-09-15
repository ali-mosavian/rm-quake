#ifndef __EMS_H__
#define __EMS_H__

/*
 * ems -- qglGemAlloc/qglGemMap's contract for the tests: four 16K windows,
 * a record of what each holds, and a map of the page already there costs
 * a compare. ems_dos.c is INT 67h; ems_nat.c copies a page into a window
 * buffer and back out on the next map, so a pointer kept across a remap
 * reads the wrong bytes natively too.
 */

#include "qtypes.h"

#ifdef __TURBOC__
#define EFAR far
#else
#define EFAR
#endif

#define EMS_PAGE   16384L
#define EMS_SLOTS  4

sint16 ems_init( void );                              /* 0 when there is no EMS */
sint16 ems_alloc( long nbytes );                      /* handle, 0 on failure */
void   ems_free( sint16 hnd );
uint8 EFAR *ems_map( sint16 hnd, uint16 page, sint16 slot );   /* 0 on failure */
long   ems_maps( void );                              /* maps that did the work */

#endif
