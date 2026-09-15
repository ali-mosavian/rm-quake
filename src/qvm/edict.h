#ifndef __EDICT_H__
#define __EDICT_H__

/*
 * edict -- pr_edict.c's ED_Alloc, ED_Free, ED_ParseEdict and ED_LoadFromFile
 * over the paged VM, for one client and no deathmatch. Free flags and free
 * times are engine state, kept in the caller's far arrays and never in an
 * edict. id's fatal errors come back as ED_ERR_*.
 */

#include "qvm.h"

#define ED_ERR_EOF       1   /* EOF without closing brace */
#define ED_ERR_BRACE     2   /* a closing brace for a value, or no opening brace */
#define ED_ERR_VALUE     3   /* a short vector, or a field or function value naming nothing */
#define ED_ERR_STRING    4   /* a text the string table refused */
#define ED_ERR_LONG      5   /* a key past id's 256 bytes or an angle past its 32, which id overruns */
#define ED_ERR_NOEDICTS  6
#define ED_ERR_TRAP      7   /* a spawn function trapped: m->trap says why */
#define ED_ERR_FIELDS    8   /* the program lacks a field the allocator writes, or the VM has too few edicts */
#define ED_ERR_NULLFN    9   /* a classname of "": id finds function 0, whose name is "", and running it is fatal */
#define ED_ERR_EMS       10  /* an edict's page would not map */
#define ED_ERR_ENTITY    11  /* an entity value past the edicts, id's EDICT_NUM error */

/* where ed_load calls hook: an edict parsed, its spawn function about to run with self set, and run */
#define ED_PARSED    0
#define ED_SPAWNING  1
#define ED_SPAWNED   2

typedef struct EdWorld {
    Qvm *m;
    sint16 num, max;             /* sv.num_edicts, and the edicts there are */
    double time;                 /* sv.time, a double: ED_Alloc's reuse test subtracts in double */
    sint16 skill;
    float QFAR *freetime;        /* max of each, the caller's */
    uint8 QFAR *isfree;
    void ( *hook )( struct EdWorld *w, sint16 stage, sint16 n );
    sint16 f_classname, f_spawnflags, f_model, f_takedamage, f_modelindex, f_colormap,
           f_skin, f_frame, f_origin, f_angles, f_nextthink, f_solid;
} EdWorld;

/*
 * name: ed_init
 * desc: A world of max edicts on m, which must have made at least that many,
 *       none free and num at the first after the client. Answers 0 or
 *       ED_ERR_FIELDS.
 */
sint16 ed_init( EdWorld *w, Qvm *m, sint16 max, float QFAR *freetime, uint8 QFAR *isfree );

/* name: ed_alloc  desc: ED_Alloc: a cleared edict's number, -1 when none is left, -2 when its page would not map. */
sint16 ed_alloc( EdWorld *w );

/* name: ed_free  desc: ED_Free, less the unlink, which is the engine's. */
void ed_free( EdWorld *w, sint16 n );

/*
 * name: ed_parse_edict
 * desc: ED_ParseEdict: the pairs after an opening brace into edict n, which
 *       is cleared first unless it is the world. *data moves past the
 *       closing brace. Answers 0 or ED_ERR_*.
 */
sint16 ed_parse_edict( EdWorld *w, const char QFAR **data, sint16 n );

/*
 * name: ed_load
 * desc: ED_LoadFromFile: every entity of text parsed, filtered by skill, and
 *       its spawn function run. Answers 0 or ED_ERR_*.
 */
sint16 ed_load( EdWorld *w, const char QFAR *text );

#endif
