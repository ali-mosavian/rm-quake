#ifndef __QTYPES_H__
#define __QTYPES_H__

#include <limits.h>

/*
 * Explicit integer widths.
 *
 * Keyed off UINT_MAX rather than a compiler macro, so a 16-bit DOS
 * compiler and a 32/64-bit host both land on the right pick without
 * knowing which compiler is in use: "long" is 32 bits under Borland
 * 16-bit but 64 on an LP64 host, and "int" is 16 under Borland.
 *
 * Getting this wrong is not a warning. A 4-byte record read through an
 * 8-byte type doubles the stride and silently mis-addresses every one
 * after the first -- which is exactly how sizeof(QVal) reached 8 here.
 */

typedef signed char    sint8;
typedef unsigned char  uint8;

typedef short          sint16;
typedef unsigned short uint16;

#if UINT_MAX == 0xFFFFU          /* 16-bit int: DOS real mode */
typedef long           sint32;
typedef unsigned long  uint32;
#define QT_NO_64
#else
typedef int            sint32;
typedef unsigned int   uint32;
#endif

/* No 64-bit integer exists under Borland 16-bit, so nothing in cport may
   depend on one. Declared only where the compiler has it. */
#ifndef QT_NO_64
typedef long long          sint64;
typedef unsigned long long uint64;
#endif

/* A negative array bound is an error every C compiler reports, including
   the 16-bit ones, so these fail the BUILD rather than the run. */
typedef char qt_check8 [ sizeof( uint8  ) == 1 ? 1 : -1 ];
typedef char qt_check16[ sizeof( uint16 ) == 2 ? 1 : -1 ];
typedef char qt_check32[ sizeof( uint32 ) == 4 ? 1 : -1 ];

#endif
