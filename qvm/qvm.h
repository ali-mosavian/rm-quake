#ifndef __QVM_H__
#define __QVM_H__

#include "qtypes.h"

/*
 * qvm -- a QuakeC bytecode interpreter, sized for real mode.
 *
 * The VM has no operand stack and no registers: every instruction operand
 * is an index into ONE flat array of 4-byte slots, and a function's locals
 * are a window in that same array, saved and restored around each call so
 * recursion works. So the whole machine is a switch plus two small stacks.
 *
 * Only the globals are conventional memory. Statements, function records,
 * strings and edicts live in 16K pages the host maps into four windows:
 *
 *   QVM_W_ST    the running function's statements; functions are placed
 *               whole on a page, so only CALL and RETURN move this window
 *   QVM_W_AUX   function records, string text and its index
 *   QVM_W_ED0   edict page 0: the world and the player
 *   QVM_W_EDX   any other edict page
 *
 * A builtin, or C between two runs, may take any window; the VM maps its
 * own back at the start of a run and after every builtin.
 *
 * Field offsets are in 32-bit WORDS from the start of an edict, as in Quake.
 * An entity value is page << 16 | byte offset in the page, where Quake's is
 * a byte offset from one base: QuakeC only compares entities, tests them
 * against 0 and adds field offsets to them, and all three still hold.
 *
 * A string value is the FNV-1a 32 hash of its text: qchash.py rewrites
 * progs.dat so, and EQ_S compares 32 bits. Null is 0 and "" QVM_STR_EMPTY:
 * EQ_S and NOT_S take them as one, IF takes only null as false, as id's
 * offsets do. The text is only for printing, found through a hash-sorted
 * index. A builtin's string (ftos, vtos) is QVM_STR_TEMP with its text in
 * m->temp: it equals no constant, even one with the same text, where id's
 * EQ_S compares the text. Two temps always compare equal, as id's two
 * results share one buffer.
 */

#ifdef __TURBOC__
#define QFAR far
#else
#define QFAR
#endif

#define QVM_VERSION   0x51480008L  /* qchash.py's; id's 6, the "" == null 0x51480006 and the nameless 0x51480007 are refused */
#define QVM_MAXPARMS  8
#define QVM_DEPTH     32     /* call nesting; Quake ships 32 */
#define QVM_LOCALS    1024   /* saved-locals stack, in slots */
#define QVM_LUMP_MAX  65520L /* the globals are indexed through one far pointer */
#define QVM_PAGE      16384L
#define QVM_FPP       455    /* function records a page: 16384 / 36 */
#define QVM_TEMP      128    /* id's pr_string_temp */
#define QVM_STR_TEMP  1L     /* no text hashes to either; qchash.py refuses one that does */
#define QVM_STR_EMPTY 2L

#define QVM_W_ST      0
#define QVM_W_AUX     1
#define QVM_W_ED0     2
#define QVM_W_EDX     3

#define QOFS_NULL     0
#define QOFS_RETURN   1
#define QOFS_PARM0    4      /* 3 slots per parm, so a vector fits */

#define QG_SELF       28     /* globalvars_t begins at RESERVED_OFS */
#define QG_TIME       31

/* Every value in progs.dat is exactly 32 bits. */
typedef union { float f; sint32 i; } QVal;

typedef char qvm_slot_is_4[ sizeof( QVal ) == 4 ? 1 : -1 ];

/* a, b, c hold UNSIGNED global indices, but IF/IFNOT/GOTO read the same
   fields as SIGNED jump displacements -- hence sint16 plus a cast at use. */
typedef struct { uint16 op; sint16 a, b, c; } QStat;

/* first: negative => builtin, index is -first. Once loaded, a QuakeC
   function's first is its placement: page << 16 | statement in the page. */
typedef struct {
    sint32 first;
    sint32 pstart, locals;
    sint32 prof;
    sint32 s_name, s_file;
    sint32 nparm;
    uint8  psz[QVM_MAXPARMS];
} QFunc;

typedef struct { uint16 type, ofs; sint32 s_name; } QDef;

/* sorted by hash as unsigned */
typedef struct { sint32 hash, ofs; } QStrIdx;

typedef char qvm_func_is_36[ sizeof( QFunc ) == 36 ? 1 : -1 ];

struct Qvm;
typedef void ( *QBuiltin )( struct Qvm *m );

/* a window's key packs its page and handle in a byte each, as gem's qgl$gem_key does */
#define QVM_MAXPAGES  255L
#define QVM_MAXHND    254

/* What the VM needs from the program running it. map answers the window's
   base, 0 on failure, and a window's base never changes: a host whose base
   depends on the page, as a conventional cache's would, cannot publish a
   key. key is the host's
   own record per window, window w being the host's slot w: page << 8 | the
   handle's low byte, 0xFFFF when nothing is known, forgotten on a release.
   The VM compares it inline and calls map only when it differs. read reads
   the progs.dat image, 0 on success. */
typedef struct {
    uint8 QFAR *( *map )( sint16 win, sint16 hnd, uint16 page );
    sint16 ( *alloc )( sint32 bytes );
    void ( *release )( sint16 hnd );
    sint16 ( *read )( void QFAR *dst, sint32 ofs, uint16 n );
    const uint16 QFAR *key;
} QHost;

typedef struct Qvm {
    const QHost *host;
    QVal  QFAR *glob;
    sint32 nstat, nfunc, nglob;
    sint32 strbase, nstr;          /* strings start at this byte of the aux pages */
    sint32 idxbase, nidx;          /* and their index at this one, a page boundary */
    sint32 fldbase, nfldname;      /* the field and function names' indexes follow it, {hash, def number}: */
    sint32 fnbase, nfnname;        /* 8-byte records from an 8-byte boundary, so none straddles a page */
    uint16 nmstr, mstr_low;        /* texts from outside: one page, index up from 0, text down from its end */
    sint32 ofs_fld, nfld;          /* fielddefs, read from the image when asked */

    sint16 h_st, h_aux, h_ed, h_mstr;
    uint16 st_pages;

    sint16 edsz;                   /* bytes per edict */
    sint16 edpp;                   /* edicts per page */
    sint16 nedict;
    uint16 edpages;
    uint8  QFAR *ed0;              /* ED0's base, taken again at run start and after a builtin */
    uint8  QFAR *wbase[4];         /* each window's base, from the VM's first map of it */

    QBuiltin QFAR *bi;             /* index 0 unused, like Quake */
    sint16 nbi;

    sint16 f_frame, f_nextthink, f_think;   /* found at load, for OP_STATE */

    sint16 argc;                   /* arg count of the call in progress */
    sint16 bnum;                   /* and the builtin's number, so one C function can serve several */
    sint16 trap;                   /* nonzero = run error, see QVM_ERR_* */
    sint16 depth, lused;
    sint32 x_pstart;               /* the running function's locals window */
    sint16 x_locals;

    char temp[QVM_TEMP];           /* QVM_STR_TEMP's text */

    sint32 lstk[QVM_LOCALS];
    /* the caller's return placement and its locals window: a return needs
       no function record, whose page may be mapped out by then */
    struct { sint32 ret, pstart; sint16 locals; } cstk[QVM_DEPTH];
} Qvm;

#define QVM_ERR_OPCODE   1
#define QVM_ERR_NULLFN   2
#define QVM_ERR_BUILTIN  3
#define QVM_ERR_DEPTH    4
#define QVM_ERR_LOCALS   5
#define QVM_ERR_VERSION  6
#define QVM_ERR_SIZE     7
#define QVM_ERR_RUNAWAY  8
#define QVM_ERR_EMS      9
#define QVM_ERR_READ     10
#define QVM_ERR_ORDER    11

#define QVM_RUNAWAY      100000L  /* statements a run, id's limit */

/* entity number to QuakeC value */
#define qvm_entval( m, n ) \
    ( ( (sint32) ( (n) / (m)->edpp ) << 16 ) | (uint16) ( (n) % (m)->edpp * (m)->edsz ) )

/*
 * name: qvm_load
 * desc: Reads progs.dat through host->read: the globals into glob (globcap
 *       bytes), everything else into EMS pages from host->alloc. Returns 0,
 *       or a QVM_ERR_*: VERSION for an image qchash.py did not rewrite, SIZE
 *       for globals past globcap or a function past a page, ORDER when
 *       functions' first statements fall with their number.
 */
sint16 qvm_load( Qvm *m, const QHost *host, QVal QFAR *glob, sint32 globcap );

/* name: qvm_free  desc: gives the pages back. */
void qvm_free( Qvm *m );

/*
 * name: qvm_edicts
 * desc: nedict zeroed edicts, as many to a 16K page as fit.
 */
sint16 qvm_edicts( Qvm *m, sint16 nedict );

/*
 * name: qvm_ent_get / qvm_ent_set
 * desc: One slot of an edict, for C. No pointer leaves the VM: the next map
 *       of a window would leave it reading another page. A field past the
 *       edict reads 0 and writes nothing.
 */
QVal qvm_ent_get( Qvm *m, sint32 ent, sint16 fld );
void qvm_ent_set( Qvm *m, sint32 ent, sint16 fld, QVal v );

/* name: qvm_ent_zero  desc: every slot of an edict 0. Answers 0, or -1 for a map that failed. */
sint16 qvm_ent_zero( Qvm *m, sint32 ent );

/*
 * name: qvm_field
 * desc: Field offset by name, in 32-bit words, or -1. The offsets are read
 *       from the image rather than hardcoded from progdefs.h: deriving them
 *       by hand off that header gave 122 for "frame" where the file says 30.
 */
sint16 qvm_field( Qvm *m, char *name );

/* name: qvm_field_def  desc: the fielddef of a field by name into d; 0, or -1 for none. */
sint16 qvm_field_def( Qvm *m, char *name, QDef *d );

/*
 * name: qvm_func
 * desc: Function number by name, or 0 (which is never callable).
 */
sint16 qvm_func( Qvm *m, char *name );

/*
 * name: qvm_func_str
 * desc: Function number named by a string value from the program or qvm_str_add, or 0. No
 *       text compare: progs texts have distinct hashes and an add sharing one is refused.
 */
sint16 qvm_func_str( Qvm *m, sint32 str );

/* name: qvm_hash  desc: a string's value: FNV-1a 32 of its text, "" QVM_STR_EMPTY. */
sint32 qvm_hash( const char QFAR *s );

/*
 * name: qvm_str_text
 * desc: A string value's text into buf, cut to cap - 1 bytes. Answers its
 *       length, or -1 for a value that is no string.
 */
sint16 qvm_str_text( Qvm *m, sint32 str, char QFAR *buf, uint16 cap );

/* name: qvm_str_temp  desc: text into the temp buffer; answers QVM_STR_TEMP. */
sint32 qvm_str_temp( Qvm *m, const char QFAR *text );

/*
 * name: qvm_str_add
 * desc: A text from outside the program -- the map's, the engine's -- as a
 *       string value, kept on a page of its own. A text the program or an
 *       earlier add has answers its value and adds nothing. Answers 0 for a
 *       text sharing a hash with another, hashing to a reserved value, or
 *       past the page's room. text must not lie in a VM window: the add maps AUX.
 */
sint32 qvm_str_add( Qvm *m, const char QFAR *text );

/*
 * name: qvm_run
 * desc: Executes function fnum to completion. Args must already be in
 *       QOFS_PARM0..; the result lands in QOFS_RETURN. On a trap (m->trap,
 *       cleared at entry) it unwinds to the depth it was called at.
 */
void qvm_run( Qvm *m, sint16 fnum );

#endif
