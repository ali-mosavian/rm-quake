/*
 * dbg.h -- the debug log, on port E9h, and the stack low-water probe.
 *
 * DOSBox-X logs what is written to E9h with "bochs debug port e9=true",
 * one line per '\n'; tools/run.sh extracts those lines into cstep.txt.
 * On a machine without the port an OUT to it goes nowhere.
 *
 * mark() is the load and exit trace every gate reads. With QR_LOG=0 it
 * and its string literals vanish, which is DGROUP the build needs.
 */
#ifndef DBG_H
#define DBG_H

#include "qrcfg.h"

#if QR_LOG
void dbg_puts( const char *line );
#define mark( what )   dbg_puts( what )
#else
#define mark( what )   ( (void) 0 )
#endif

/* the painted span, near offsets in DGROUP */
typedef struct { unsigned lo, hi; } DbgProbe;

/* Paints everything between the near heap's break and SP. The heap grows
   up to the break and the stack down to SP, so nothing lives in between,
   and whatever still holds the paint at exit was never touched. */
void dbg_probe_paint( DbgProbe *p );

/* "stack_untouched N of M" on E9 */
void dbg_probe_report( const DbgProbe *p );

#endif
