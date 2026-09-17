/*
 * sc.c -- the surface cache. C port of d_surf.bas's sc_* slice.
 *
 * A pool of EMS DCs, one per cached surface, and the shape of that is
 * dictated by the filler that has to read them. 8plxtp derives its
 * texel addressing from the DC itself, reaching texels with 16-bit
 * registers, so the width and height must be powers of two.
 *
 * The filler calls rdAccess exactly once, before the span loop, and
 * for an EMS DC that is a single EMS_MAP of one logical page -- 16K.
 * Anything past it is simply never mapped, so a cached surface has to
 * fit 16,384 bytes entire: 128x128, or 256x64, and no more.
 *
 * DCs are pooled by size class and recycled rather than deleted,
 * because a miss should cost a build, not a qglNew. sc_flush hands
 * every DC back to its class and clears every face's chain.
 *
 * Reclaim is keyed on a block's SIZE, not its class shape: 2^a * 2^b
 * is 2^(a+b), so a 4096-byte block serves (4,8), (5,7), (6,6), (7,5)
 * and (8,4) identically. Everything below sc_alloc's class lookup is a
 * buddy allocator over SIZE: split a larger free block when a small
 * one is wanted, and on free, merge with the buddy at gran XOR 2^order
 * so the large sizes can come back.
 */

#include <alloc.h>
#include <stddef.h>

#include <string.h>
#include "sc.h"
#include "qgl.h"

#define SC_HINDEX_MASK 0x07FF
#define SC_HSTATE_SHIFT 11
#define SC_HSTATE_HIGH 0x8000
#define SC_GRAN_MASK 0x3FFF
#define SC_DYNAMIC 0x4000
#define SC_OWNER_MASK 0x1FFF
#define SC_MIP_SHIFT 13

static short sc_hindex( unsigned short h )
{
    short b = (short) ( h & SC_HINDEX_MASK );
    return b == SC_NULL_INDEX ? -1 : b;
}

static short sc_hstate( SurfCache far *sc, unsigned short h )
{
    short b = sc_hindex( h );
    return (short) ( ( h >> SC_HSTATE_SHIFT ) |
                     ( ( sc->bown[b] & SC_HSTATE_HIGH ) ? 32 : 0 ) );
}

static unsigned short sc_handle( SurfCache far *sc, short b, short state )
{
    sc->bown[b] = (unsigned short) ( ( sc->bown[b] & ~SC_HSTATE_HIGH ) |
                                     ( ( state & 32 ) ? SC_HSTATE_HIGH : 0 ) );
    return (unsigned short) ( ( ( state & 31 ) << SC_HSTATE_SHIFT ) | b );
}

static short sc_owner( SurfCache far *sc, short b )
{
    return sc->bown[b] == 0xFFFF ? -1 : (short) ( sc->bown[b] & SC_OWNER_MASK );
}

static short sc_mip( SurfCache far *sc, short b )
{
    return (short) ( ( sc->bown[b] >> SC_MIP_SHIFT ) & 3 );
}

static void sc_set_owner( SurfCache far *sc, short b, short face, short mip )
{
    sc->bown[b] = (unsigned short) ( face | ( mip << SC_MIP_SHIFT ) );
}

static short sc_gran( SurfCache far *sc, short b )
{
    return (short) ( sc->bgrn[b] & SC_GRAN_MASK );
}

static short sc_next_get( SurfCache far *sc, short b )
{
    short bit = (short) ( b * 3 );
    short off = (short) ( bit >> 3 );
    short shift = (short) ( bit & 7 );
    unsigned short hi = (unsigned short) sc->bnext[SC_NBLK + off]
                      | ( (unsigned short) sc->bnext[SC_NBLK + off + 1] << 8 );
    unsigned short v = (unsigned short) sc->bnext[b]
                     | (unsigned short) ( ( ( hi >> shift ) & 7 ) << 8 );
    return v == SC_NULL_INDEX ? -1 : (short) v;
}

static void sc_next_set( SurfCache far *sc, short b, short next )
{
    short bit = (short) ( b * 3 );
    short off = (short) ( bit >> 3 );
    short shift = (short) ( bit & 7 );
    unsigned short mask = (unsigned short) ( 7 << shift );
    unsigned short hi = (unsigned short) sc->bnext[SC_NBLK + off]
                      | ( (unsigned short) sc->bnext[SC_NBLK + off + 1] << 8 );
    unsigned short n = next < 0 ? SC_NULL_INDEX : (unsigned short) next;
    sc->bnext[b] = (unsigned char) n;
    hi = (unsigned short) ( ( hi & ~mask ) | ( ( ( n >> 8 ) << shift ) & mask ) );
    sc->bnext[SC_NBLK + off] = (unsigned char) hi;
    sc->bnext[SC_NBLK + off + 1] = (unsigned char) ( hi >> 8 );
}

/* Smallest power-of-two shift that covers v, clamped to the classes
   the filler can address. */
short sc_shift( short v )
{
    short s = SC_MINSH, p = 16;
    while ( p < v && s < SC_MAXSH ) { p *= 2; s++; }
    return s;
}

short sc_mipfloor( short extw, short exth )
{
    short m, w, h;

    for ( m = 0; m < 4; m++ ) {
        w = extw >> m;
        h = exth >> m;
        if ( w < 1 ) w = 1;
        if ( h < 1 ) h = 1;
        if ( SC_SHIFT( w ) + SC_SHIFT( h ) <= SC_MAXSUM ) return m;
    }
    return 3;
}

/* Block records, recycled. A merge consumes one record (two blocks
   become one), a split produces one, so the count churns and must not
   just walk bcnt upward forever. */
static short sc_brec( SurfCache far *sc )
{
    short b;

    if ( sc->rfree >= 0 ) {
        b = sc->rfree;
        sc->rfree = sc_next_get( sc, b );
        return b;
    }
    if ( sc->bcnt < SC_NBLK ) return sc->bcnt++;
    return -1;
}

static void sc_bput( SurfCache far *sc, short b )
{
    sc_next_set( sc, b, sc->rfree );
    sc->rfree = b;
}

/* The per-order free lists, singly linked through bnext. sc_ftake
   pulls out one specific granule rather than the head -- that is how
   a merge finds its buddy, and the lists are short enough that a walk
   costs nothing. */
static void sc_fpush( SurfCache far *sc, short b, short ord )
{
    sc->bown[b] = 0xFFFF;
    sc_next_set( sc, b, sc->fhead[ord] );
    sc->fhead[ord] = b;
}

static short sc_fpop( SurfCache far *sc, short ord )
{
    short b = sc->fhead[ord];
    if ( b >= 0 ) sc->fhead[ord] = sc_next_get( sc, b );
    return b;
}

static short sc_ftake( SurfCache far *sc, short ord, short gran )
{
    short b = sc->fhead[ord], p = -1;

    while ( b >= 0 ) {
        if ( sc_gran( sc, b ) == gran ) {
            if ( p >= 0 ) sc_next_set( sc, p, sc_next_get( sc, b ) );
            else          sc->fhead[ord] = sc_next_get( sc, b );
            return b;
        }
        p = b;
        b = sc_next_get( sc, b );
    }
    return -1;
}

/*
 * Hands a block back, merging with its buddy for as long as the buddy
 * is also free. The buddy of a block of order o sits at gran XOR 2^o
 * -- the two halves of an aligned 2^(o+1) block differ in exactly that
 * bit -- and the merged block keeps the lower address, which is what
 * keeps it aligned to its new, larger size.
 */
static void sc_bfree( SurfCache far *sc, short blk, short ord )
{
    short b = blk, bud, g;

    sc->bown[b] = 0xFFFF;

    while ( ord < SC_NORD - 1 ) {
        g = sc_gran( sc, b ) ^ (short) (1 << ord);
        bud = sc_ftake( sc, ord, g );
        if ( bud < 0 ) break;

        /* the merged block is the lower of the two, one order bigger,
           which is what keeps it aligned to its new size */
        if ( sc_gran( sc, bud ) < sc_gran( sc, b ) ) {
            sc_bput( sc, b );
            b = bud;
        } else {
            sc_bput( sc, bud );
        }
        ord++;
    }
    sc_fpush( sc, b, ord );
}

/* A free block of exactly this order, made by halving a bigger one
   repeatedly. -1 if nothing larger is free. The upper half of each
   split goes on its own free list, so nothing is lost. */
static short sc_bsplit( SurfCache far *sc, short ord )
{
    short j, b, h;

    for ( j = ord + 1; j < SC_NORD; j++ ) {
        b = sc_fpop( sc, j );
        if ( b < 0 ) continue;

        while ( j > ord ) {
            h = sc_brec( sc );
            if ( h < 0 ) {   /* no record to hold the half */
                sc_fpush( sc, b, j );
                return -1;
            }
            j--;
            sc->bgrn[h] = (unsigned short) ( sc_gran( sc, b ) + (short) (1 << j) );
            sc_fpush( sc, h, j );
        }
        return b;
    }
    return -1;
}

/* Bump-allocates a surface's bytes. Aligned to its own size, which
   keeps it inside one 16K logical page: the fillers map a single page
   and then address the texture flat, so a surface that straddled two
   would read garbage past the seam. -1 when full. */
static long sc_grab( SurfCache far *sc, long sz )
{
    long o = sc->next;
    if ( (o % sz) != 0 ) o += sz - (o % sz);
    if ( o + sz > sc->cap ) return -1;
    sc->next = o + sz;
    return o;
}

/* A block of this order without evicting anything: a free one that
   size, a bigger free one halved, or fresh store -- splitting before
   growing, since the store is the finite resource. -1 when none. */
static short sc_bget( SurfCache far *sc, short bord, long sz )
{
    short blk;
    long ofs;

    blk = sc_fpop( sc, bord );
    if ( blk < 0 ) blk = sc_bsplit( sc, bord );
    if ( blk >= 0 ) return blk;

    blk = sc_brec( sc );
    if ( blk < 0 ) return -1;
    ofs = sc_grab( sc, sz );
    if ( ofs < 0 ) {
        sc_bput( sc, blk );
        return -1;
    }
    sc->bgrn[blk] = (unsigned short) ( ofs / SC_GRAN );
    return blk;
}

/* The FIFO list is singly linked. Unlink walks one class rather than
   spending 2 KB of scarce conventional memory on reverse links. */
static void sc_lru_unlink( SurfCache far *sc, short b, short ord )
{
    short p, n;

    if ( b < 0 ) return;
    p = -1;
    for ( n = sc->lhead[ord]; n >= 0 && n != b; n = sc_next_get( sc, n ) ) p = n;
    if ( n < 0 ) return;
    n = sc_next_get( sc, b );
    if ( p >= 0 ) sc_next_set( sc, p, n ); else sc->lhead[ord] = n;
    if ( n < 0 ) sc->ltail[ord] = p;
    sc_next_set( sc, b, -1 );
}

/* Owned blocks are FIFO within each size. Hits do no bookkeeping. */
static void sc_lru_use( SurfCache far *sc, short b, short ord )
{
    short t;

    if ( b < 0 ) return;
    for ( t = sc->lhead[ord]; t >= 0; t = sc_next_get( sc, t ) )
        if ( t == b ) return;
    t = sc->ltail[ord];
    sc_next_set( sc, b, -1 );
    if ( t >= 0 ) sc_next_set( sc, t, b ); else sc->lhead[ord] = b;
    sc->ltail[ord] = b;
}

static short sc_lru_oldest( SurfCache far *sc, short ord )
{
    return sc->lhead[ord];
}

/* A face's own blocks, most recently used first, linked through bsib
   from Face.cache. Every eviction goes through sc_chain_drop: the
   face keeps its other surfaces. */
static void sc_chain_drop( SurfCache far *sc, short face, short b )
{
    unsigned short h = sc->faces[face].cache, p = SC_NULL_HANDLE;
    short k = sc_hindex( h );

    while ( k >= 0 && k != b ) { p = h; h = sc->bsib[k]; k = sc_hindex( h ); }
    if ( k < 0 ) return;
    if ( sc_hindex( p ) >= 0 ) sc->bsib[ sc_hindex( p ) ] = sc->bsib[k];
    else sc->faces[face].cache = sc->bsib[k];
    sc->bsib[k] = SC_NULL_HANDLE;
}

static void sc_chain_front( SurfCache far *sc, short face, unsigned short h )
{
    short b = sc_hindex( h );
    if ( sc->faces[face].cache == h ) return;
    sc_chain_drop( sc, face, b );
    sc->bsib[b] = sc->faces[face].cache;
    sc->faces[face].cache = h;
}

static void sc_reset_lists( SurfCache far *sc )
{
    short i;
    for ( i = 0; i < SC_NORD; i++ ) {
        sc->lhead[i] = -1;
        sc->ltail[i] = -1;
        sc->fhead[i] = -1;
    }
    sc->rfree = -1;
}

static short sc_why  = 0;   /* the allocation sc_init stopped on, 1..4 */
static long  sc_want = 0;   /* and the bytes it asked that one for */

void sc_fail( short *step, long *want )
{
    *step = sc_why;
    *want = sc_want;
}

short sc_init( SurfCache far *sc, Face far *faces, short face_count )
{
    short i;

    sc->faces = faces;
    sc->bgrn = 0; sc->bown = 0; sc->bnext = 0; sc->bsib = 0;
    sc->desc = 0;
    sc->hnd = 0;

    sc->flushes = 0;
    sc->peak    = 0;
    sc->made    = 0;
    sc->hits    = 0;
    sc->builds  = 0;
    sc->bpeak   = 0;
    sc->live    = 0;
    sc->evict   = 0;
    sc->tbuilds = 0;
    sc->dlit    = 0;
    sc->nofresh = 0;

    sc_want = (long) SC_NBLK * sizeof(unsigned short);
    sc_why = 1; sc->bgrn = (unsigned short far *) qglMemAlloc( sc_want );
    if ( !sc->bgrn ) { sc_shutdown( sc ); return 0; }
    sc_why = 2; sc->bown = (unsigned short far *) qglMemAlloc( sc_want );
    if ( !sc->bown ) { sc_shutdown( sc ); return 0; }
    sc_why = 3; sc->bsib = (unsigned short far *) qglMemAlloc( sc_want );
    if ( !sc->bsib ) { sc_shutdown( sc ); return 0; }
    sc_want = SC_NEXT_BYTES;
    sc_why = 4; sc->bnext = (unsigned char far *) qglMemAlloc( sc_want );
    if ( !sc->bnext ) { sc_shutdown( sc ); return 0; }
    sc_why = 0;

    sc->desc = 0;
    sc_reset_lists( sc );
    sc->bcnt = 0;
    for ( i = 0; i < face_count; i++ ) sc->faces[i].cache = SC_NULL_HANDLE;

    sc->hnd  = 0;
    sc->next = 0;
    sc->cap  = 0;

    if ( !qglGemInit() ) {
        sc->ok = 0;
        return 1;
    }

    /* The store itself is not claimed here -- nothing needs a surface
       until the first one is built. See sc_store_open. */
    sc->ok = -1;
    return 1;
}

/* Claims the store DC, on first use. Deliberately not in sc_init: the
   store needs a large contiguous EMS allocation, and claiming it
   before vid_init has taken its own share risks losing the race for
   no reason -- nothing needs a surface until the first one is built. */
static short sc_store_open( SurfCache far *sc )
{
    if ( sc->hnd != 0 ) return -1;

    /* One EMS page per row keeps the store's own row table tiny. */
    sc->hnd = qglSfNew( (short) SC_PGBYTES, SC_PAGES, QGL_SURF_EMS );
    if ( sc->hnd == 0 ) {
        sc->cap = 0;
        return 0;
    }
    sc->cap  = SC_STORE;
    sc->next = 0;
    return -1;
}

void sc_reset( SurfCache far *sc, short face_count )
{
    short i;

    for ( i = 0; i < face_count; i++ ) {
        sc->faces[i].cache = SC_NULL_HANDLE;
    }
    sc_reset_lists( sc );
    sc->bcnt = 0;
    sc->next = 0;
}

void sc_shutdown( SurfCache far *sc )
{
    /* views first: they borrow the store's pixels, so the store
       outlives them */
    if ( sc->desc != 0 ) qglDelView( sc->desc );
    sc->desc = 0;
    if ( sc->hnd != 0 ) qglSfFree( sc->hnd );
    sc->hnd  = 0;
    sc->cap  = 0;
    sc->next = 0;
    sc->ok   = 0;

    if ( sc->bgrn )  qglMemFree( (long) (void far *) sc->bgrn );
    if ( sc->bown )  qglMemFree( (long) (void far *) sc->bown );
    if ( sc->bnext ) qglMemFree( (long) (void far *) sc->bnext );
    if ( sc->bsib )  qglMemFree( (long) (void far *) sc->bsib );
    sc->faces = 0; sc->bgrn = 0;
    sc->bown = 0; sc->bnext = 0;
    sc->bsib = 0;
}

void sc_flush( SurfCache far *sc, short face_count )
{
    short i;

    sc->next = 0;
    sc->flushes++;
    /* a flush is a mass eviction: every surface in the store at once */
    sc->evict += sc->live;
    sc->live = 0;

    sc_reset_lists( sc );
    for ( i = 0; i < face_count; i++ ) sc->faces[i].cache = SC_NULL_HANDLE;
    sc->bcnt = 0;
}

QSurf sc_find( SurfCache far *sc, short face, short mip, short w, short h, short state, long *aim_ofs )
{
    QSurf dc;
    unsigned short hnd;
    short a, b, blk;

    if ( !sc->ok || state < 0 ) return 0;
    hnd = sc->faces[face].cache;
    for ( blk = sc_hindex( hnd ); blk >= 0; hnd = sc->bsib[blk], blk = sc_hindex( hnd ) )
        if ( sc_mip( sc, blk ) == mip && !( sc->bgrn[blk] & SC_DYNAMIC ) &&
             sc_hstate( sc, hnd ) == state ) break;
    if ( blk < 0 ) return 0;

    /* The view is the CURRENT mip's shape, which is not the block's: a
       block is sized once at the face's finest mip, and a coarser mip
       just uses less of it. Aiming a smaller view at a larger block's
       offset is safe -- the bigger alignment implies the smaller one. */
    a = SC_SHIFT( w );
    b = SC_SHIFT( h );
    if ( a + b > SC_MAXSUM ) return 0;

    dc = sc->desc;
    if ( dc == 0 ) return 0;
    if ( !qglSfViewResize( dc, sc->hnd, (short) (1 << a), (short) (1 << b),
                          (short) (1 << a) ) ) return 0;
    /* bgrn * SC_GRAN without the long-multiply helper: bgrn is under
       16384, so the long is two 16-bit halves of it. */
    ((unsigned short *) aim_ofs)[0] = (unsigned short) sc_gran( sc, blk ) << SC_MINORD;
    ((unsigned short *) aim_ofs)[1] = (unsigned short) ( sc_gran( sc, blk ) >> ( 16 - SC_MINORD ) );
    /* Row 0 only: a hit is drawn, never built into, and the texture
       fetch reads nothing past row 0. sc_alloc aims every row. */
    if ( !qglAimView( dc, *aim_ofs ) ) return 0;
    sc->hits++;
    if ( sc->faces[face].cache != hnd ) sc_chain_front( sc, face, hnd );
    return dc;
}

QSurf sc_alloc( SurfCache far *sc, short face, short mip, short w, short h, short fw, short fh,
              short state, short face_count, long *aim_ofs )
{
    short a, b, bord;
    QSurf dc;
    unsigned short hnd, newh;
    short vic, blk, j, b2, k, last, n, fresh;
    long ofs, sz;

    (void) fw; (void) fh;   /* kept in the signature for a future
                                shrink-on-demand -- see sc.h */

    if ( !sc->ok || w <= 0 || h <= 0 ) return 0;
    if ( sc->hnd == 0 ) {
        if ( !sc_store_open( sc ) ) {
            sc->ok = 0;   /* no store, no cache */
            return 0;
        }
    }

    a = SC_SHIFT( w );
    b = SC_SHIFT( h );
    if ( a + b > SC_MAXSUM ) return 0;   /* past one EMS page, and
                                             rdAccess maps only one */
    /* One tableless view for every class, reshaped and re-aimed for the
       current surface. Its stride is the physical power-of-two width. */
    if ( sc->desc == 0 ) {
        dc = qglSfViewNew( sc->hnd, (short) (1 << a), (short) (1 << b),
                          (short) (1 << a) );
        if ( dc == 0 ) return 0;
        sc->desc = dc;
        sc->made++;
    } else {
        dc = sc->desc;
        if ( !qglSfViewResize( dc, sc->hnd, (short) (1 << a), (short) (1 << b),
                              (short) (1 << a) ) ) return 0;
    }

    /* The block is sized at the mip being drawn NOW, and grows only if
       a finer mip is later wanted -- sizing at the face's floor mip
       instead made every block as big as the face could ever need. */
    bord = (a + b) - SC_MINORD;
    sz   = (long) (1L << a) * (1L << b);

    /* Quake keeps one cache spot per mip. Within this mip, write over one
       holding this key (a find that missed on it wanted a bigger class),
       one a dynamic light built (its key never comes back), or the least
       recently used after SC_VARIANTS light keys. Other mips stay resident. */
    blk  = -1;
    last = -1;
    n    = 0;
    hnd = sc->faces[face].cache;
    for ( k = sc_hindex( hnd ); k >= 0; hnd = sc->bsib[k], k = sc_hindex( hnd ) ) {
        if ( sc_mip( sc, k ) != mip ) continue;
        if ( blk < 0 && ( ( sc->bgrn[k] & SC_DYNAMIC ) ||
                          ( state >= 0 && sc_hstate( sc, hnd ) == state ) ) ) blk = k;
        last = k;
        n++;
    }
    if ( blk < 0 && ( state < 0 || n >= SC_VARIANTS ) ) blk = last;
    fresh = -1;
    if ( blk < 0 && last >= 0 ) {
        /* Another surface for the face, but only out of free store:
           with the records spent, taking one would evict a surface in
           view, and the next frame would evict for that one. */
        fresh = sc_bget( sc, bord, sz );
        if ( fresh < 0 ) { blk = last; sc->nofresh++; }
    }

    if ( fresh < 0 && blk >= 0 ) {
        newh = sc_handle( sc, blk, state < 0 ? 0 : state );
        sc_chain_front( sc, face, newh );
    } else {
        /* Then an eviction, and only as the very last thing, giving up. */
        blk = fresh >= 0 ? fresh : sc_bget( sc, bord, sz );
        if ( blk < 0 ) {                               /* evict FIFO of this size */
            blk = sc_lru_oldest( sc, bord );
            if ( blk >= 0 ) {
                vic = sc_owner( sc, blk );
                if ( vic >= 0 ) {
                    sc_chain_drop( sc, vic, blk );
                    sc->live--;
                    sc->evict++;
                }
                sc_lru_unlink( sc, blk, bord );
            }
        }
        if ( blk < 0 ) {
            /* Nothing of this size anywhere, so evict the least
               recently used LARGER block and split it down -- what
               stops one size starving while another holds the store. */
            for ( j = bord + 1; j < SC_NORD; j++ ) {
                vic = sc_lru_oldest( sc, j );
                if ( vic < 0 ) continue;
                b2 = sc_owner( sc, vic );
                if ( b2 >= 0 ) {
                    sc_chain_drop( sc, b2, vic );
                    sc->live--;
                    sc->evict++;
                }
                sc_lru_unlink( sc, vic, j );
                sc_bfree( sc, vic, j );
                blk = sc_bsplit( sc, bord );
                if ( blk >= 0 ) break;
            }
        }
        if ( blk < 0 ) {
            /* the backstop, and it should now be unreachable */
            sc_flush( sc, face_count );
            return 0;
        }

        sc_set_owner( sc, blk, face, mip );
        sc_next_set( sc, blk, -1 );
        newh = sc_handle( sc, blk, state < 0 ? 0 : state );
        sc->bsib[blk] = sc->faces[face].cache;
        sc->faces[face].cache = newh;
        sc->live++;
    }
    sc->bgrn[blk] = (unsigned short) ( sc_gran( sc, blk ) | ( state < 0 ? SC_DYNAMIC : 0 ) );
    ofs = (long) sc_gran( sc, blk ) * SC_GRAN;
    if ( sc->next > sc->peak ) sc->peak = sc->next;

    sc_lru_use( sc, blk, bord );

    /* aim it at the bytes just claimed, ready for the builder to write;
       unaimed, nothing is built, so the block must not read as built */
    *aim_ofs = ofs;
    if ( !qglSetView( dc, ofs ) ) {
        sc->bgrn[blk] |= SC_DYNAMIC;
        return 0;
    }

    return dc;
}

void sc_forget( SurfCache far *sc, short face )
{
    short blk;
    if ( sc->ok && ( blk = sc_hindex( sc->faces[face].cache ) ) >= 0 )
        sc->bgrn[blk] |= SC_DYNAMIC;
}

void sc_stats( SurfCache far *sc, CacheStats *s )
{
    /* Same reasoning as sc_ready: the overlay draws on a run whose cache
       never allocated, and zeroes are the honest reading there. */
    if ( !sc ) { memset( s, 0, sizeof(*s) ); return; }
    s->hits    = sc->hits;
    s->builds  = sc->builds;
    s->bpeak   = sc->bpeak;
    s->made    = sc->made;
    s->live    = sc->live;
    s->evict   = sc->evict;
    s->flushes = sc->flushes;
    s->peak    = sc->peak;
    s->total_builds = sc->tbuilds;
    s->dlit    = sc->dlit;
    s->nofresh = sc->nofresh;
    s->blocks  = sc->bcnt;
}

short sc_frame_end( SurfCache far *sc )
{
    short built = sc->builds;
    if ( sc->builds > sc->bpeak ) sc->bpeak = sc->builds;
    sc->hits   = 0;
    sc->builds = 0;
    return built;
}

short sc_ready( SurfCache far *sc )
{
    /* NULL is a real caller state, not a contract violation: main marks
       "sc_init FAILED" and carries on so a map too big for the cache
       still draws, just unlit. It used to dereference straight through
       and take the program with it -- on e1m1, only once an unrelated
       memory change let the run get this far. */
    if ( !sc ) return 0;
    return sc->ok;
}

void sc_note_build( SurfCache far *sc )
{
    sc->builds++;
    sc->tbuilds++;
}

void sc_note_dlit( SurfCache far *sc )
{
    sc->dlit++;
}

/*
 * name: sc_selftest
 * desc: Proves the pool behaves: sizes round up to the class the
 *       filler needs, a recycled DC comes back rather than a new one,
 *       distinct faces get distinct surfaces, a flush retires every
 *       chain, and bytes written to a cached DC read back -- then FIFO
 *       reuse, single-surface eviction, buddy merge/split, and finally
 *       that a light-state change forces a rebuild.
 *
 *       10 faces (indices 0, 1, 2, 5 and 9 are used below) is the
 *       throwaway world this runs against -- never the caller's.
 */
static short sc_selftest_run( SurfCache far *sc )
{
    QSurf d0, d1, d2;
    short made0;
    unsigned char wr[32], rd[32];
    long ofs0, live0, flush0, next0;
    long aim;
    short i, built;

    if ( !sc->ok ) return -1;

    sc_reset( sc, 10 );

    /* 224 rounds to 256, 112 to 128, 20 to 32 */
    if ( sc_shift( 224 ) != 8 ) return -2;
    if ( sc_shift( 112 ) != 7 ) return -3;
    if ( sc_shift( 20 )  != 5 ) return -4;
    if ( sc_shift( 16 )  != 4 ) return -5;
    for ( i = -1; i <= 4096; i++ ) if ( SC_SHIFT( i ) != sc_shift( i ) ) return -90;

    /* 224x224 pads to 256x256 = 64K, four pages: it must be refused */
    if ( sc_alloc( sc, 9, 0, 224, 224, 224, 224, 0, 10, &aim ) != 0 ) return -6;
    /* 112x112 pads to 128x128 = 16,384, exactly one page: it must not be */
    d0 = sc_alloc( sc, 0, 0, 112, 112, 112, 112, 0, 10, &aim );
    d1 = sc_alloc( sc, 1, 0, 112, 96,  112, 96,  0, 10, &aim );
    if ( d0 == 0 ) return -7;
    if ( d1 == 0 ) return -8;
    /* both round to 128x128, so they share a view and differ only in
       where it points -- the whole point of the store */
    if ( d0 != d1 ) return -18;
    if ( sc_hindex( sc->faces[0].cache ) == sc_hindex( sc->faces[1].cache ) ) return -21;
    if ( sc->hnd == 0 ) return -22;

    /* and the floor it implies: 224 needs mip 1, 112 does not */
    if ( sc_mipfloor( 224, 224 ) != 1 ) return -19;
    if ( sc_mipfloor( 112, 112 ) != 0 ) return -20;

    if ( sc_find( sc, 0, 0, 112, 112, 0, &aim ) != d0 ) return -9;
    if ( aim != (long) sc_gran( sc, sc_hindex( sc->faces[0].cache ) ) * SC_GRAN ) return -91;
    {   /* a granule past 255 reaches the offset's high word */
        short hb = sc_hindex( sc->faces[0].cache );
        short keep = sc->bgrn[hb];
        sc->bgrn[hb] = 300;
        if ( sc_find( sc, 0, 0, 112, 112, 0, &aim ) != d0 || aim != 300L * SC_GRAN ) return -92;
        sc->bgrn[hb] = keep;
    }
    if ( sc_find( sc, 1, 0, 112, 96,  0, &aim ) != d1 ) return -10;
    if ( sc_find( sc, 1, 1, 112, 96,  0, &aim ) != 0 )  return -11;

    /* a write into the last row of the largest class, the 16K page edge.
       sc_find aims row 0 alone, so every row is aimed here first. */
    if ( !qglSetView( d0, (long) sc_gran( sc, sc_hindex( sc->faces[0].cache ) ) * SC_GRAN ) ) return -85;
    for ( i = 0; i < 32; i++ ) { wr[i] = (unsigned char) ((i * 7 + 3) & 255); rd[i] = 0; }
    /* through a far cast: (long) on a near array drops DS, and the row
       went to and from segment 0 */
    qglRowWrite( d0, 0, 127, 32, QGL_FMT_8BIT, (long) (void far *) wr );
    qglRowRead( d0, 0, 127, 32, QGL_FMT_8BIT, (long) (void far *) rd );
    for ( i = 0; i < 32; i++ ) if ( rd[i] != wr[i] ) return -12;

    /* a flush must retire the slots and hand the DCs back, not make more */
    made0 = sc->made;
    sc_flush( sc, 10 );
    if ( sc_find( sc, 0, 0, 112, 112, 0, &aim ) != 0 ) return -14;

    /* a flush rewinds the store and makes no new view */
    d2 = sc_alloc( sc, 5, 0, 112, 112, 112, 112, 0, 10, &aim );
    if ( d2 != d0 ) return -15;
    if ( sc->made != made0 ) return -16;
    if ( sc_hindex( sc->faces[5].cache ) < 0 ) return -23;
    if ( sc_gran( sc, sc_hindex( sc->faces[5].cache ) ) != 0 ) return -33;

    /*
     * ---- FIFO reuse ----------------------------------------------
     * Everything above would pass just as well with the bump-and-flush
     * this replaced, so prove the part that is actually new: that
     * reuse happens, that a HIT changes who gets evicted, and that
     * eviction takes exactly one surface rather than the whole store.
     */
    sc_reset( sc, 10 );

    /* fill one class: three faces, three distinct blocks */
    d0 = sc_alloc( sc, 0, 0, 112, 112, 112, 112, 0, 10, &aim );
    d1 = sc_alloc( sc, 1, 0, 112, 112, 112, 112, 0, 10, &aim );
    d2 = sc_alloc( sc, 2, 0, 112, 112, 112, 112, 0, 10, &aim );
    if ( d0 == 0 || d1 == 0 || d2 == 0 ) return -24;
    if ( sc->bcnt != 3 ) return (short) -(2000 + sc->bcnt);
    if ( sc_hindex( sc->faces[0].cache ) < 0 ) return -2999;
    if ( sc_hindex( sc->faces[0].cache ) == sc_hindex( sc->faces[1].cache ) ) return -25;
    if ( sc_hindex( sc->faces[1].cache ) == sc_hindex( sc->faces[2].cache ) ) return -26;

    /* FIFO deliberately does no work on a hit: face 0 remains the victim. */
    if ( sc_find( sc, 0, 0, 112, 112, 0, &aim ) == 0 ) return -27;
    {
        short ord = SC_SHIFT( 112 ) * 2 - SC_MINORD;
        if ( sc_lru_oldest( sc, ord ) < 0 ) return -28;
        if ( sc_owner( sc, sc_lru_oldest( sc, ord ) ) != 0 ) return -93;

        /* The victim is the middle link in insertion order, which is
           the case the singly-linked unlink has to walk to reach. */
        next0 = sc->cap;
        sc->cap = sc->next;
        built = sc_alloc( sc, 3, 0, 112, 112, 112, 112, 0, 10, &aim ) != 0;
        sc->cap = next0;
        if ( !built || sc_find( sc, 0, 0, 112, 112, 0, &aim ) != 0 ) return -101;
        if ( !sc_find( sc, 1, 0, 112, 112, 0, &aim ) ) return -102;
        if ( !sc_find( sc, 2, 0, 112, 112, 0, &aim ) ) return -103;
        if ( !sc_find( sc, 3, 0, 112, 112, 0, &aim ) ) return -104;
    }

    /* A face keeps a distinct block per mip, as Quake's cachespots[4].
       Building mip 1 must leave mip 0 resident and cost no flush. */
    sc_reset( sc, 10 );
    if ( sc_alloc( sc, 0, 0, 112, 112, 112, 112, 0, 10, &aim ) == 0 ) return -105;
    ofs0   = sc_hindex( sc->faces[0].cache );
    live0  = sc->live;
    flush0 = sc->flushes;
    if ( sc_alloc( sc, 0, 1, 56, 56, 112, 112, 0, 10, &aim ) == 0 ) return -29;
    if ( sc_hindex( sc->faces[0].cache ) == ofs0 ) return -30;
    if ( sc->live != live0 + 1 ) return -31;
    if ( sc_find( sc, 0, 0, 112, 112, 0, &aim ) == 0 ) return -34;
    if ( sc_find( sc, 0, 1, 56, 56, 0, &aim ) == 0 ) return -35;

    /* and a mip change must not have cost a flush */
    if ( sc->flushes != flush0 ) return -32;

    /*
     * ---- buddy merge ------------------------------------------------
     * Two 16x16 blocks are order 0 and land at granules 0 and 1 --
     * buddies, since 0 XOR 1 is 2^0. Growing both faces frees both,
     * and the second free must merge them into one order-1 block.
     */
    sc_reset( sc, 10 );
    {
        short b0 = sc_bget( sc, 0, 256L );
        short b1 = sc_bget( sc, 0, 256L );
        if ( b0 < 0 ) return -40;
        if ( b1 < 0 ) return -41;
        sc_bfree( sc, b0, 0 );
        sc_bfree( sc, b1, 0 );
        next0 = sc->next;
        if ( sc_bget( sc, 1, 512L ) < 0 ) return -44;
        if ( sc->next != next0 ) return -45;
    }

    /*
     * ---- buddy split --------------------------------------------
     * A freed order-2 block must be halved to serve an order-0
     * request rather than the store being grown again.
     */
    sc_reset( sc, 10 );
    {
        short b0 = sc_bget( sc, 2, 1024L );
        if ( b0 < 0 ) return -46;
        sc_bfree( sc, b0, 2 );
        next0 = sc->next;
        if ( sc_bget( sc, 0, 256L ) < 0 ) return -48;
        if ( sc->next != next0 ) return -49;
    }

    /*
     * ---- state: a light-style change must force a rebuild --------
     * Everything above passes whether or not sc_find checks the state,
     * so this block proves that a different six-bit state misses.
     */
    sc_reset( sc, 10 );
    if ( sc_alloc( sc, 0, 0, 112, 112, 112, 112, 5, 10, &aim ) == 0 ) return -50;
    if ( sc_find( sc, 0, 0, 112, 112, 5, &aim ) == 0 ) return -51;
    if ( sc_find( sc, 0, 0, 112, 112, 62, &aim ) != 0 ) return -52;

    /* building at the new state keeps the old one: a style flickering
       back to 5 hits instead of rebuilding */
    live0 = sc->live;
    if ( sc_alloc( sc, 0, 0, 112, 112, 112, 112, 62, 10, &aim ) == 0 ) return -53;
    if ( sc_find( sc, 0, 0, 112, 112, 5, &aim ) == 0 ) return -54;
    if ( sc_find( sc, 0, 0, 112, 112, 62, &aim ) == 0 ) return -55;
    if ( sc->live != live0 + 1 ) return -56;

    /* FIFO eviction removes the first inserted state and leaves the other. */
    if ( sc_find( sc, 0, 0, 112, 112, 5, &aim ) == 0 ) return -57;
    next0 = sc->cap;
    sc->cap = sc->next;
    if ( sc_alloc( sc, 1, 0, 112, 112, 112, 112, 0, 10, &aim ) == 0 ) { sc->cap = next0; return -58; }
    sc->cap = next0;
    if ( sc_find( sc, 0, 0, 112, 112, 5, &aim ) != 0 ) return -59;
    if ( sc_find( sc, 0, 0, 112, 112, 62, &aim ) == 0 ) return -60;
    if ( sc_find( sc, 1, 0, 112, 112, 0, &aim ) == 0 ) return -61;

    /* The light-state cap is per mip. Adding one past SC_VARIANTS gives
       up only mip 0's oldest face-chain state; all mip 1 states stay. */
    sc_reset( sc, 10 );
    live0 = sc->live;
    for ( i = 1; i <= SC_VARIANTS; i++ )
        if ( sc_alloc( sc, 3, 0, 16, 16, 16, 16, i, 10, &aim ) == 0 ) return -62;
    for ( i = 1; i <= SC_VARIANTS; i++ )
        if ( sc_alloc( sc, 3, 1, 16, 16, 16, 16, i + 10, 10, &aim ) == 0 ) return -99;
    if ( sc_find( sc, 3, 0, 16, 16, 1, &aim ) == 0 ) return -63;
    if ( sc_alloc( sc, 3, 0, 16, 16, 16, 16, 9, 10, &aim ) == 0 ) return -64;
    if ( sc_find( sc, 3, 0, 16, 16, 2, &aim ) != 0 ) return -65;
    for ( i = 1; i <= SC_VARIANTS; i++ )
        if ( i != 2 && sc_find( sc, 3, 0, 16, 16, i, &aim ) == 0 ) return -66;
    if ( sc_find( sc, 3, 0, 16, 16, 9, &aim ) == 0 ) return -67;
    for ( i = 1; i <= SC_VARIANTS; i++ )
        if ( sc_find( sc, 3, 1, 16, 16, i + 10, &aim ) == 0 ) return -100;
    if ( sc->live != live0 + SC_VARIANTS * 2 ) return -68;

    /* a dynamic light's key never comes back, so it writes over what
       the face has rather than taking another block */
    sc_reset( sc, 10 );
    live0 = sc->live;
    if ( sc_alloc( sc, 2, 0, 112, 112, 112, 112, 5, 10, &aim ) == 0 ) return -69;
    if ( sc_alloc( sc, 2, 0, 112, 112, 112, 112, -3, 10, &aim ) == 0 ) return -70;
    if ( sc_alloc( sc, 2, 0, 112, 112, 112, 112, -4, 10, &aim ) == 0 ) return -71;
    if ( sc_alloc( sc, 2, 0, 112, 112, 112, 112, 5, 10, &aim ) == 0 ) return -72;
    if ( sc->live != live0 + 1 ) return -73;

    /* a new key under a full store writes over the face's own least
       recently used surface, not the store's -- here face 1's */
    sc_reset( sc, 10 );
    live0 = sc->live;
    if ( sc_alloc( sc, 0, 0, 112, 112, 112, 112, 5, 10, &aim ) == 0 ) return -74;
    if ( sc_alloc( sc, 0, 0, 112, 112, 112, 112, 6, 10, &aim ) == 0 ) return -75;
    if ( sc_alloc( sc, 1, 0, 112, 112, 112, 112, 0, 10, &aim ) == 0 ) return -76;
    if ( sc_find( sc, 0, 0, 112, 112, 5, &aim ) == 0 ) return -77;
    if ( sc_find( sc, 0, 0, 112, 112, 6, &aim ) == 0 ) return -78;
    next0 = sc->cap;
    sc->cap = sc->next;
    built = sc_alloc( sc, 0, 0, 112, 112, 112, 112, 7, 10, &aim ) != 0;
    sc->cap = next0;
    if ( !built ) return -79;
    if ( sc_find( sc, 1, 0, 112, 112, 0, &aim ) == 0 ) return -80;
    if ( sc_find( sc, 0, 0, 112, 112, 5, &aim ) != 0 ) return -81;
    if ( sc_find( sc, 0, 0, 112, 112, 7, &aim ) == 0 ) return -82;
    if ( sc->live != live0 + 3 ) return -83;

    /* a surface the builder refused is not one */
    sc_forget( sc, 0 );
    if ( sc_find( sc, 0, 0, 112, 112, 7, &aim ) != 0 ) return -84;

    /* A split takes the oldest larger block. Hits do not reorder FIFO. */
    sc_reset( sc, 10 );
    if ( !sc_alloc( sc, 0, 0, 112, 112, 112, 112, 0, 10, &aim ) ||
         !sc_alloc( sc, 1, 0, 112, 112, 112, 112, 0, 10, &aim ) ||
         !sc_find( sc, 0, 0, 112, 112, 0, &aim ) ) return -98;
    next0 = sc->cap;
    sc->cap = sc->next;
    built = sc_alloc( sc, 3, 0, 16, 16, 16, 16, 0, 10, &aim ) != 0;
    sc->cap = next0;
    if ( !built || sc_hindex( sc->faces[0].cache ) >= 0 ||
         sc_hindex( sc->faces[1].cache ) < 0 ) return -96;

    sc_reset( sc, 10 );
    return 1;
}

/* sc_init refused part-way must return 0 and keep nothing. The struct is
   0xFF, as uncleared qglMemAlloc bytes can be: the pointers sc_init never
   reached passed its null test, it returned 1, and what it did allocate
   was never given back. Memory is filled with blocks one short array wide
   and two non-adjacent ones freed, so two short arrays fit and the third
   does not. Everything allocated before the failure has to go back. */
static short sc_selftest_short( SurfCache far *sc )
{
    long hog[256];   /* on the stack: static is 1K of DGROUP */
    Face faces[10];
    long before, want;
    short n = 0, k, r, ok, step;

    qglMemAvail( QGL_MEM_LARGEST );   /* merge, or TOTAL reads short */
    before = qglMemAvail( QGL_MEM_TOTAL );

    while ( n < 256 && ( hog[n] = qglMemAlloc( (long) SC_NBLK * (long) sizeof(short) ) ) != 0 ) n++;
    if ( n < 4 ) {
        for ( k = 0; k < n; k++ ) qglMemFree( hog[k] );
        return -86;
    }
    qglMemFree( hog[0] );
    qglMemFree( hog[2] );

    _fmemset( sc, 0xFF, sizeof(SurfCache) );
    r = sc_init( sc, (Face far *) faces, 10 );
    ok = sc->ok;
    sc_fail( &step, &want );

    for ( k = 1; k < n; k++ ) if ( k != 2 ) qglMemFree( hog[k] );
    if ( r != 0 ) return -87;
    if ( ok != 0 ) return -88;
    if ( step < 3 ) return -86;   /* no array allocated: not armed */
    qglMemAvail( QGL_MEM_LARGEST );
    if ( qglMemAvail( QGL_MEM_TOTAL ) != before ) return -89;
    return 1;
}

short sc_selftest( void )
{
    SurfCache far *sc;
    Face faces[10];
    short face_count = 10;
    short result;

    sc = (SurfCache far *) qglMemAlloc( (long) sizeof(SurfCache) );
    if ( !sc ) return -1;

    if ( !sc_init( sc, (Face far *) faces, face_count ) ) {
        qglMemFree( (long) (void far *) sc );
        return -1;
    }

    result = sc_selftest_run( sc );

    sc_shutdown( sc );
    if ( result == 1 ) result = sc_selftest_short( sc );
    qglMemFree( (long) (void far *) sc );
    return result;
}
