#include <dos.h>
#include <string.h>

#include "ems.h"

static uint16 frame;
static sint16 key_hnd[EMS_SLOTS];
static uint16 key_pg[EMS_SLOTS];
static long   nmaps;
uint16 ems_key[EMS_SLOTS] = { 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF };

sint16 ems_init( void )
{
    union REGS r;
    sint16 i;

    /* the EMM's device name at offset 10 of whatever INT 67h points into */
    char far *dev = (char far *) MK_FP( FP_SEG( getvect( 0x67 ) ), 10 );
    if ( _fmemcmp( dev, "EMMXXXX0", 8 ) ) return 0;
    r.h.ah = 0x41;
    int86( 0x67, &r, &r );
    if ( r.h.ah ) return 0;
    frame = r.x.bx;
    for ( i = 0; i < EMS_SLOTS; i++ ) key_hnd[i] = 0;
    return 1;
}

sint16 ems_alloc( long nbytes )
{
    union REGS r;

    r.h.ah = 0x43;
    r.x.bx = (uint16) ( ( nbytes + EMS_PAGE - 1 ) / EMS_PAGE );
    int86( 0x67, &r, &r );
    return r.h.ah ? 0 : (sint16) r.x.dx;
}

void ems_free( sint16 hnd )
{
    union REGS r;
    sint16 i;

    for ( i = 0; i < EMS_SLOTS; i++ )
        if ( key_hnd[i] == hnd ) { key_hnd[i] = 0; ems_key[i] = 0xFFFF; }
    r.h.ah = 0x45;
    r.x.dx = hnd;
    int86( 0x67, &r, &r );
}

uint8 far *ems_map( sint16 hnd, uint16 page, sint16 slot )
{
    union REGS r;

    if ( key_hnd[slot] != hnd || key_pg[slot] != page ) {
        r.h.ah = 0x44;
        r.h.al = (uint8) slot;
        r.x.bx = page;
        r.x.dx = hnd;
        int86( 0x67, &r, &r );
        if ( r.h.ah ) { key_hnd[slot] = 0; ems_key[slot] = 0xFFFF; return 0; }
        key_hnd[slot] = hnd;
        key_pg[slot] = page;
        ems_key[slot] = (uint16) ( ( page << 8 ) | ( hnd & 0xFF ) );
        nmaps++;
    }
    return (uint8 far *) MK_FP( frame + slot * 0x400, 0 );
}

long ems_maps( void ) { return nmaps; }
