#ifndef __QVMI_H__
#define __QVMI_H__

/*
 * qvmi -- what qvm.c and qstr.c share and nothing outside the VM may use.
 */

#include "qvm.h"

/* A window already on the page costs a compare against the host's record
   and no call. A hit on a window this VM has not mapped since qvm_load
   answers base 0 and traps as QVM_ERR_EMS, never another page; it takes a
   host that kept a freed handle's key. Mapping every window at load to
   rule it out aliases one page into four, which a copying host corrupts. */
#define WKEY( h, pg ) ( (uint16) ( ( (uint16) (pg) << 8 ) | ( (uint16) (h) & 0xFF ) ) )
#define MAP( m, w, h, pg ) ( (m)->host->key[w] == WKEY( h, pg ) ? (m)->wbase[w] : qvm_win_map( m, w, h, (uint16) (pg) ) )

uint8 QFAR *qvm_win_map( Qvm *m, sint16 w, sint16 h, uint16 pg );

/*
 * name: qvm_idx_find
 * desc: The entry for key in an index of n 8-byte records from byte base of
 *       handle hnd, sorted by hash as unsigned; 0 when absent. Good until
 *       AUX maps again.
 */
QStrIdx QFAR *qvm_idx_find( Qvm *m, sint16 hnd, sint32 base, sint32 n, sint32 key );

/* name: qvm_str_is  desc: whether the string lump's text at ofs is s. */
sint16 qvm_str_is( Qvm *m, sint32 ofs, const char *s );

#endif
