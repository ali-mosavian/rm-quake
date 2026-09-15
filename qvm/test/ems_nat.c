#include <stdlib.h>
#include <string.h>

#include "ems.h"

#define NHND 16

static uint8 *store[NHND];
static long   npages[NHND];
static uint8  win[EMS_SLOTS][EMS_PAGE];
static sint16 key_hnd[EMS_SLOTS];
static uint16 key_pg[EMS_SLOTS];
static long   nmaps;

sint16 ems_init( void ) { return 1; }

sint16 ems_alloc( long nbytes )
{
    sint16 h;

    for ( h = 1; h < NHND; h++ )
        if ( !store[h] ) break;
    if ( h == NHND ) return 0;
    npages[h] = ( nbytes + EMS_PAGE - 1 ) / EMS_PAGE;
    store[h] = (uint8 *) calloc( (size_t) npages[h], EMS_PAGE );
    return store[h] ? h : 0;
}

/* a window's bytes go back to their page, as the EMM's would be the page itself */
static void unmap( sint16 slot )
{
    if ( key_hnd[slot] )
        memcpy( store[key_hnd[slot]] + key_pg[slot] * EMS_PAGE, win[slot], EMS_PAGE );
    key_hnd[slot] = 0;
}

void ems_free( sint16 hnd )
{
    sint16 i;

    for ( i = 0; i < EMS_SLOTS; i++ )
        if ( key_hnd[i] == hnd ) key_hnd[i] = 0;
    free( store[hnd] );
    store[hnd] = 0;
}

uint8 *ems_map( sint16 hnd, uint16 page, sint16 slot )
{
    if ( hnd < 1 || hnd >= NHND || !store[hnd] || page >= npages[hnd] ) return 0;
    if ( key_hnd[slot] != hnd || key_pg[slot] != page ) {
        unmap( slot );
        memcpy( win[slot], store[hnd] + page * EMS_PAGE, EMS_PAGE );
        key_hnd[slot] = hnd;
        key_pg[slot] = page;
        nmaps++;
    }
    return win[slot];
}

long ems_maps( void ) { return nmaps; }
