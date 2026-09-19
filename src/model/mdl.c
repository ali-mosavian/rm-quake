/*
 * mdl.c -- loading alias models and the monsters that stand on them.
 * d_mdl.bas's mdl_load, and ent.bas's monster records.
 *
 * A model costs the far heap nothing: an EMS handle holds its vertices,
 * whole frames to a 16K page, then its triangles on the page after, and
 * the skin is its own EMS surface. The .geo header says how many pages
 * and how many frames a page, so it is the statement of what arrived.
 *
 * No AI. A monster stands where the map put it, facing the map's way,
 * on the first frame of its stand set.
 */

#include <mem.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mdl.h"
#include "fight.h"
#include "item.h"
#include "ent.h"
#include "pak.h"
#include "qgl.h"
#include "dbg.h"
#include "sys.h"

#define PAGE_SLOT 2     /* shared with nodes, leaves, the lightmap */
#define MDL_PAGE  16384L

static char *mdl_name[MDL_KINDS] = {
    "soldier", "knight", "dog", "ogre", "demon", "zombie", "wizard", "shambler"
};

/* A model that will not load says why on stderr, and on E9 where the
   gates can read it: run.sh keeps no stderr. */
static void near mdl_refuse( const char *why )
{
    fprintf( stderr, "%s\n", why );
    mark( why );
}

/* The sets a caller indexes hold frames, no count sits past nsets, and
   the counts add up to the frames: an empty set is a divide by zero in
   the AI's cycles, and a count past the frames a read off the pages. */
static short near mdl_sets_ok( const MdlHead *h, short need )
{
    short s, sum = 0;

    if ( h->nsets < need || h->nsets > MDL_NSETS ||
         h->nframe < 1 || h->nframe > 255 ) return 0;
    for ( s = 0; s < MDL_NSETS; s++ ) {
        if ( h->set_n[s] > 31 ) return 0;
        if ( s < need && !h->set_n[s] ) return 0;
        if ( s >= h->nsets && h->set_n[s] ) return 0;
        sum += h->set_n[s];
    }
    return sum == h->nframe;
}

short mdl_set_count( const MdlState *m, short set )
{
    short bit, byte, shift;
    unsigned short value;

    if ( set < 0 || set >= MDL_NSETS ) return 0;
    bit = (short) ( set * 5 ); byte = (short) ( bit >> 3 ); shift = (short) ( bit & 7 );
    value = (unsigned short) m->set_bits[byte] >> shift;
    if ( shift > 3 ) value |= (unsigned short) m->set_bits[byte + 1] << ( 8 - shift );
    return (short) ( value & 31 );
}

void mdl_set_counts( MdlState *m, const unsigned char *counts )
{
    short set, bit, byte, shift;
    unsigned short value;

    memset( m->set_bits, 0, sizeof(m->set_bits) );
    for ( set = 0; set < MDL_NSETS; set++ ) {
        bit = (short) ( set * 5 ); byte = (short) ( bit >> 3 ); shift = (short) ( bit & 7 );
        value = (unsigned short) counts[set] << shift;
        m->set_bits[byte] |= (unsigned char) value;
        if ( shift > 3 ) m->set_bits[byte + 1] |= (unsigned char) ( value >> 8 );
    }
}

static void near mdl_from_head( MdlState *m, const MdlHead *h )
{
    float ex, ey;

    m->ntri = h->ntri; m->nvert = h->nvert; m->nframe = h->nframe;
    m->scale = h->scale; m->origin = h->origin;
    mdl_set_counts( m, h->set_n );
    m->vpages = h->vpages; m->page_frames = h->page_frames;

    ex = (float) fabs( m->origin.x );
    if ( (float) fabs( m->origin.x + 255.0f * m->scale.x ) > ex )
        ex = (float) fabs( m->origin.x + 255.0f * m->scale.x );
    ey = (float) fabs( m->origin.y );
    if ( (float) fabs( m->origin.y + 255.0f * m->scale.y ) > ey )
        ey = (float) fabs( m->origin.y + 255.0f * m->scale.y );
    m->radius = (float) sqrt( ex * ex + ey * ey );
    m->zlo = m->origin.z;
    m->zhi = m->origin.z + 255.0f * m->scale.z;
}

static short near mdl_load_stream( Pak *pak, MdlState *m, char *name, short need,
                                   short fh, short split, QSurf *more, short nskins )
{
    char  path[24], msg[96];    /* path and the longest refusal's numbers */
    MdlHead h;
    short page, p, left;
    long  n, bytes;

    m->loaded = 0;
    m->vtx_hnd = 0;
    m->skin = 0;

    if ( split ) {
        sprintf( path, "%s.geo", name );
        fh = pak_seek( pak, path, &n );
    } else {
        strcpy( path, name );
    }
    if ( qglFileRead( fh, (long) (void far *) &h, (long) sizeof(h) ) != (long) sizeof(h) ) {
        sprintf( msg, "%s: header short read", path ); mdl_refuse( msg );
        return 0;
    }
    if ( h.magic != MDL_MAGIC ) {
        sprintf( msg, "%s: not a QMD4 .geo -- rebuild the map's container", path ); mdl_refuse( msg );
        return 0;
    }
    if ( !mdl_sets_ok( &h, need ) ) {
        sprintf( msg, "%s: %d sets, %d needed, or not its %d frames", path, h.nsets, need, h.nframe );
        mdl_refuse( msg );
        return 0;
    }
    /* Vertex bytes span 0..255, so this is the box every frame fits in;
       the yaw turns about the origin, so the horizontal reach is a radius. */
    mdl_from_head( m, &h );

    if ( m->nvert > MDL_MAXV + 1 || m->ntri < 1 || m->nvert < 1 || m->nframe < 1 ||
         m->vpages < 1 || m->page_frames < 1 ||
         (long) m->page_frames * m->nvert * 3 > MDL_PAGE ||
         (long) m->vpages * m->page_frames < (long) m->nframe ||
         (long) ( m->vpages - 1 ) * m->page_frames >= (long) m->nframe ) {
        sprintf( msg, "%s: %d verts %d tris %d frames %d pages -- will not fit",
                 path, m->nvert, m->ntri, m->nframe, m->vpages ); mdl_refuse( msg );
        return 0;
    }
    bytes = (long) m->ntri * MDL_TRI_BYTES;
    if ( bytes > MDL_PAGE ) { sprintf( msg, "%s: triangles past one page", path ); mdl_refuse( msg ); return 0; }

    /* one handle: pages 0..vpages-1 the vertices, page vpages the triangles */
    m->vtx_hnd = qglGemAlloc( (long) ( m->vpages + 1 ) * MDL_PAGE );
    if ( !m->vtx_hnd ) {
        sprintf( msg, "%s: no EMS for %d pages", path, m->vpages + 1 ); mdl_refuse( msg );
        return 0;
    }

    /* the triangles, from where the header left the file */
    page = qglGemMap( m->vtx_hnd, m->vpages, PAGE_SLOT );
    if ( !page || qglFileRead( fh, (long) page << 16, bytes ) != bytes ) {
        sprintf( msg, "%s: triangles would not load", path ); mdl_refuse( msg );
        qglGemFree( m->vtx_hnd ); m->vtx_hnd = 0; return 0;
    }

    /* A bundled model continues into its vertices. Split monster files
       reopen the .vtx member here. */
    if ( split ) {
        sprintf( path, "%s.vtx", name );
        fh = pak_seek( pak, path, &n );
    }
    for ( p = 0; p < m->vpages; p++ ) {
        left = (short) ( m->nframe - p * m->page_frames );
        if ( left > m->page_frames ) left = m->page_frames;
        bytes = (long) left * m->nvert * 3;
        page = qglGemMap( m->vtx_hnd, p, PAGE_SLOT );
        if ( !page || qglFileRead( fh, (long) page << 16, bytes ) != bytes ) {
            sprintf( msg, "%s: vertex page %d would not load", path, p ); mdl_refuse( msg );
            qglGemFree( m->vtx_hnd ); m->vtx_hnd = 0; return 0;
        }
    }

    /* The skin: one EMS page at most, because qglRsPoly refuses a
       texture crossing two -- the texel base is an immediate the filler
       patches into itself and never remaps mid-polygon. mkmdl.py's
       resample keeps it true and writes the size it chose into the
       header, so the two cannot drift. */
    if ( split ) {
        sprintf( path, "%s.skn", name );
        fh = pak_seek( pak, path, &n );
    }
    for ( p = 0; p < nskins; p++ ) {
        QSurf skin = qglSfNew( h.skin_w, h.skin_h, QGL_SURF_EMS );
        if ( !skin || !qglSfLoadFh( skin, fh ) ) {
            sprintf( msg, "%s: skin %d would not load", path, p ); mdl_refuse( msg );
            if ( skin ) qglSfFree( skin );
            if ( m->skin ) { qglSfFree( m->skin ); m->skin = 0; }
            while ( --p > 0 ) { qglSfFree( more[p - 1] ); more[p - 1] = 0; }
            qglGemFree( m->vtx_hnd ); m->vtx_hnd = 0; return 0;
        }
        if ( p == 0 ) m->skin = skin;
        else more[p - 1] = skin;
    }

    m->loaded = 1;
    return -1;
}

void mdl_load( Pak *pak, MdlState *m, char *name, short need )
{
    mdl_load_stream( pak, m, name, need, 0, -1, 0, 1 );
}

void mdl_unload( MdlState *m )
{
    if ( m->skin ) qglSfFree( m->skin );
    if ( m->vtx_hnd ) qglGemFree( m->vtx_hnd );
    memset( m, 0, sizeof(*m) );
}

#define ITEM_MDL_ARMOR    0
#define ITEM_MDL_LIGHTNING 20
#define ITEM_MDL_BACKPACK 21
#define ITEM_MDL_N        22

static char *item_mdl_name[ITEM_MDL_N] = {
    "armor", "g_shot", "g_nail", "quaddama", "suit",
    "g_rock", "g_nail2", "g_rock2", "invulner", "invisibl",
    "w_s_key", "w_g_key", "m_s_key", "m_g_key", "b_s_key", "b_g_key",
    "end1", "end2", "end3", "end4", "g_light", "backpack"
};

static short near item_mdl_id( ItemEnt far *it, short worldtype )
{
    if ( it->crate >= 0 || it->kind == ENT_ITEM_EXPLOBOX ) return -1;
    switch ( it->kind ) {
    case ENT_ITEM_ARMOR1:
    case ENT_ITEM_ARMOR2:
    case ENT_ITEM_ARMOR3: return ITEM_MDL_ARMOR;
    case ENT_ITEM_SSG: return 1;
    case ENT_ITEM_NAILGUN: return 2;
    case ENT_ITEM_QUAD: return 3;
    case ENT_ITEM_SUIT: return 4;
    case ENT_ITEM_GL: return 5;
    case ENT_ITEM_SNG: return 6;
    case ENT_ITEM_RL: return 7;
    case ENT_ITEM_PENT: return 8;
    case ENT_ITEM_INVIS: return 9;
    case ENT_ITEM_LIGHTNING: return ITEM_MDL_LIGHTNING;
    case ENT_ITEM_KEY1: return (short) ( 10 + worldtype * 2 );
    case ENT_ITEM_KEY2: return (short) ( 11 + worldtype * 2 );
    case ENT_ITEM_SIGIL:
        if ( it->amount & 2 ) return 17;
        if ( it->amount & 4 ) return 18;
        if ( it->amount & 8 ) return 19;
        return 16;
    }
    return -1;
}

static short near item_mdl_slot( World *world, short id )
{
    short i;
    for ( i = 0; i < world->item_mdl_count; i++ )
        if ( world->item_mdl[i].id == id ) return i;
    return -1;
}

void mdl_load_items( World *world, short worldtype )
{
    short used[ITEM_MDL_N];
    short i, id, n = 0, fh;
    long bytes;
    char path[24];

    memset( used, 0, sizeof(used) );
    for ( i = 0; i < world->item_count; i++ ) {
        id = item_mdl_id( &world->item[i], worldtype );
        if ( id >= 0 ) used[id] = 1;
    }
    for ( i = 0; i < world->mon_count; i++ )
        if ( world->mon[i].kind == MDL_KIND_ARMY || world->mon[i].kind == MDL_KIND_OGRE )
            used[ITEM_MDL_BACKPACK] = 1;
    for ( i = 0; i < ITEM_MDL_N; i++ ) if ( used[i] ) n++;

    world->item_mdl_count = n;
    world->backpack_mdl = -1;
    world->item_mdl = (AliasModel far *) qglMemAlloc(
        (long) ( n ? n : 1 ) * sizeof(AliasModel) );
    world->item_mdl_slot = (signed char far *) qglMemAlloc( world->item_max ? world->item_max : 1 );
    world->item_skin = (signed char far *) qglMemAlloc( world->item_max ? world->item_max : 1 );
    if ( !world->item_mdl || !world->item_mdl_slot || !world->item_skin )
        sys_error( "items: out of memory for models" );

    n = 0;
    for ( id = 0; id < ITEM_MDL_N; id++ ) if ( used[id] ) {
        AliasModel im;
        memset( &im, 0, sizeof(im) );
        im.id = id;
        sprintf( path, "%s.itm", item_mdl_name[id] );
        fh = pak_seek( &world->pak, path, &bytes );
        mdl_load_stream( &world->pak, &im.state, path, 1, fh, 0,
                          &im.skin1, (short) ( id == ITEM_MDL_ARMOR ? 3 : 1 ) );
        _fmemcpy( &world->item_mdl[n], &im, sizeof(im) );
        if ( id == ITEM_MDL_BACKPACK ) world->backpack_mdl = n;
        n++;
    }

    mdl_rebind_items( world, worldtype );
    /* Slots after the map's items are reserved for dropped backpacks.
       Demo snapshots use those slots directly rather than pl_item_add. */
    for ( i = world->item_count; i < world->item_max; i++ ) {
        world->item_mdl_slot[i] = (signed char) world->backpack_mdl;
        world->item_skin[i] = 0;
    }
}

void mdl_rebind_items( World *world, short worldtype )
{
    short i, id;

    for ( i = 0; i < world->item_count; i++ ) {
        ItemEnt far *it = &world->item[i];
        if ( it->crate <= ENT_BACKPACK_CRATE ) {
            world->item_mdl_slot[i] = (signed char) world->backpack_mdl;
            world->item_skin[i] = 0;
            continue;
        }
        id = item_mdl_id( it, worldtype );
        world->item_mdl_slot[i] = (signed char) ( id < 0 ? -1 : item_mdl_slot( world, id ) );
        world->item_skin[i] = (signed char) ( it->kind == ENT_ITEM_ARMOR2 ? 1 :
                                              it->kind == ENT_ITEM_ARMOR3 ? 2 : 0 );
    }
}

void mdl_unload_items( World *world )
{
    short i;
    for ( i = 0; i < world->item_mdl_count; i++ ) {
        AliasModel im;
        _fmemcpy( &im, &world->item_mdl[i], sizeof(im) );
        if ( im.skin1 ) qglSfFree( im.skin1 );
        if ( im.skin2 ) qglSfFree( im.skin2 );
        mdl_unload( &im.state );
    }
    if ( world->item_mdl ) qglMemFree( (long) world->item_mdl );
    if ( world->item_mdl_slot ) qglMemFree( (long) world->item_mdl_slot );
    if ( world->item_skin ) qglMemFree( (long) world->item_skin );
    world->item_mdl = 0;
    world->item_mdl_slot = 0;
    world->item_skin = 0;
    world->item_mdl_count = 0;
    world->backpack_mdl = -1;
}

typedef struct {
    char magic[4];
    short count, skin_w, skin_h;
    long geom_bytes;
} FxBankHead;

typedef struct {
    char name[8];
    MdlHead head;
    unsigned short vtx_ofs, tri_ofs;
    long skin_ofs;
} FxBankEnt;

typedef char rec_fxhead_ok[ sizeof(FxBankHead) == 14 ? 1 : -1 ];
typedef char rec_fxmodel_ok[ sizeof(FxBankEnt) == 76 ? 1 : -1 ];

void mdl_load_effects( World *world )
{
    FxBankHead head;
    FxBankEnt ent;
    EffectModel model;
    EffectModel far *models;
    short fh, i, page, seg;
    long bytes, left, chunk, skin_bytes;

    fh = pak_seek( &world->pak, "effects.bin", &bytes );
    if ( qglFileRead( fh, (long) (void far *) &head, sizeof(head) ) != sizeof(head) ||
         memcmp( head.magic, "QFX3", 4 ) || head.count != MDL_FX_N ||
         head.skin_w < 1 || head.skin_h < 1 || head.geom_bytes < 1 )
        sys_error( "effects.bin: bad header" );

    skin_bytes = (long) head.skin_w * head.skin_h;
    if ( bytes != sizeof(head) + (long) head.count * sizeof(ent) +
                  head.geom_bytes + skin_bytes )
        sys_error( "effects.bin: bad size" );

    world->fx_mdl_count = head.count;
    world->fx_mdl_hnd = qglGemAlloc( (long) head.count * sizeof(EffectModel) );
    if ( !world->fx_mdl_hnd ) sys_error( "effects.bin: no EMS for models" );
    seg = qglGemMap( world->fx_mdl_hnd, 0, PAGE_SLOT );
    if ( !seg ) sys_error( "effects.bin: model map failed" );
    models = (EffectModel far *) ((long) seg << 16);

    for ( i = 0; i < head.count; i++ ) {
        if ( qglFileRead( fh, (long) (void far *) &ent, sizeof(ent) ) != sizeof(ent) )
            sys_error( "effects.bin: directory short read" );
        if ( !ent.name[0] || !mdl_sets_ok( &ent.head, 1 ) ||
             ent.head.nvert < 1 || ent.head.nvert > MDL_MAXV + 1 || ent.head.ntri < 1 ||
             (long) ent.vtx_ofs + (long) ent.head.nvert * ent.head.nframe * 3 > head.geom_bytes ||
             (long) ent.tri_ofs + (long) ent.head.ntri * MDL_TRI_BYTES > head.geom_bytes ||
             ent.vtx_ofs / MDL_PAGE !=
                 ( ent.vtx_ofs + (long) ent.head.nvert * ent.head.nframe * 3 - 1 ) / MDL_PAGE ||
             ent.tri_ofs / MDL_PAGE !=
                 ( ent.tri_ofs + (long) ent.head.ntri * MDL_TRI_BYTES - 1 ) / MDL_PAGE ||
             ent.skin_ofs < 0 || ent.skin_ofs + (long) ent.head.skin_w * ent.head.skin_h > skin_bytes ||
             ent.skin_ofs % MDL_PAGE + (long) ent.head.skin_w * ent.head.skin_h > MDL_PAGE )
            sys_error( "effects.bin: bad model record" );
        memset( &model, 0, sizeof(model) );
        mdl_from_head( &model.state, &ent.head );
        model.state.page_frames = 0;
        /* A bank model owns neither field: vpages holds its vertex byte
           offset and skin its triangle byte offset while page_frames=0. */
        model.state.vpages = (short) ent.vtx_ofs;
        model.state.skin = (QSurf) ent.tri_ofs;
        model.skin_ofs = ent.skin_ofs;
        model.skin_w = ent.head.skin_w;
        model.skin_h = ent.head.skin_h;
        _fmemcpy( &models[i], &model, sizeof(model) );
    }

    world->fx_geom_hnd = qglGemAlloc( head.geom_bytes );
    if ( !world->fx_geom_hnd ) sys_error( "effects.bin: no EMS for geometry" );
    left = head.geom_bytes;
    for ( page = 0; left > 0; page++ ) {
        chunk = left > MDL_PAGE ? MDL_PAGE : left;
        seg = qglGemMap( world->fx_geom_hnd, page, PAGE_SLOT );
        if ( !seg || qglFileRead( fh, (long) seg << 16, chunk ) != chunk )
            sys_error( "effects.bin: geometry short read" );
        left -= chunk;
    }
    world->fx_skin_atlas = qglSfNewEx( head.skin_w, head.skin_h, head.skin_w, QGL_SURF_EMS );
    if ( !world->fx_skin_atlas || !qglSfLoadFh( world->fx_skin_atlas, fh ) )
        sys_error( "effects.bin: skin atlas would not load" );
    world->fx_skin_view = 0;
    world->fx_skin_aim = -1;
    seg = qglGemMap( world->fx_mdl_hnd, 0, PAGE_SLOT );
    if ( !seg ) sys_error( "effects.bin: model remap failed" );
    models = (EffectModel far *) ((long) seg << 16);
    for ( i = 0; i < head.count; i++ ) {
        models[i].state.vtx_hnd = world->fx_geom_hnd;
        models[i].state.loaded = 1;
    }
}

void mdl_unload_effects( World *world )
{
    if ( world->fx_skin_view ) qglDelView( world->fx_skin_view );
    if ( world->fx_skin_atlas ) qglSfFree( world->fx_skin_atlas );
    if ( world->fx_geom_hnd ) qglGemFree( world->fx_geom_hnd );
    if ( world->fx_mdl_hnd ) qglGemFree( world->fx_mdl_hnd );
    world->fx_mdl_hnd = 0;
    world->fx_mdl_count = 0;
    world->fx_geom_hnd = 0;
    world->fx_skin_atlas = world->fx_skin_view = 0;
    world->fx_skin_aim = -1;
}

short mdl_effect_model( World *world, short id, EffectModel *out )
{
    EffectModel far *models;
    short seg;

    if ( id < 0 || id >= world->fx_mdl_count || !world->fx_mdl_hnd ) return 0;
    seg = qglGemMap( world->fx_mdl_hnd, 0, PAGE_SLOT );
    if ( !seg ) return 0;
    models = (EffectModel far *) ((long) seg << 16);
    _fmemcpy( out, &models[id], sizeof(*out) );
    return -1;
}

QSurf mdl_effect_skin( World *world, short id, const EffectModel *model )
{
    if ( id < 0 || id >= world->fx_mdl_count ) return 0;
    if ( !world->fx_skin_view ) {
        world->fx_skin_view = qglSfViewNew( world->fx_skin_atlas,
                                             model->skin_w, model->skin_h, model->skin_w );
        if ( !world->fx_skin_view ) return 0;
        world->fx_skin_aim = -1;
    }
    if ( world->fx_skin_aim != id ) {
        if ( !qglSfViewResize( world->fx_skin_view, world->fx_skin_atlas,
                               model->skin_w, model->skin_h, model->skin_w ) ||
             !qglSfViewAim( world->fx_skin_view, model->skin_ofs ) ) return 0;
        world->fx_skin_aim = id;
    }
    return world->fx_skin_view;
}

unsigned char far *mdl_frame_verts( const MdlState *m, short frame, short slot )
{
    unsigned char far *vb;
    long ofs;

    if ( !m->page_frames ) {
        ofs = (long) (unsigned short) m->vpages + (long) frame * m->nvert * 3;
        vb = (unsigned char far *) ((unsigned long)
            qglGemMap( m->vtx_hnd, (short) ( ofs / MDL_PAGE ), slot ) << 16);
        return vb + (unsigned short) ( ofs % MDL_PAGE );
    }
    vb = (unsigned char far *)
        ((unsigned long) qglGemMap( m->vtx_hnd, (short) ( frame / m->page_frames ), slot ) << 16);

    return vb + (long) ( frame % m->page_frames ) * m->nvert * 3;
}

MdlTri far *mdl_tris( const MdlState *m, short slot )
{
    if ( !m->page_frames ) {
        unsigned long ofs = (unsigned long) m->skin;
        unsigned char far *base = (unsigned char far *) ((unsigned long)
            qglGemMap( m->vtx_hnd, (short) ( ofs / MDL_PAGE ), slot ) << 16);
        return (MdlTri far *) ( base + ofs % MDL_PAGE );
    }
    return (MdlTri far *) ((unsigned long) qglGemMap( m->vtx_hnd, m->vpages, slot ) << 16);
}

short mdl_set_frame( const MdlState *m, short set, short anim )
{
    short s;
    short count = mdl_set_count( m, set );

    if ( !count ) return 0;
    if ( anim < 0 ) anim = 0;
    if ( anim >= count ) anim = (short) ( count - 1 );
    for ( s = 0; s < set; s++ ) anim += mdl_set_count( m, s );
    return anim;
}

/* One frame into sum, i the byte's place in the stream so far. */
static unsigned long near mdl_sum_frame( const MdlState *m, short frame, unsigned long sum, unsigned long *i )
{
    unsigned char far *vb = mdl_frame_verts( m, frame, PAGE_SLOT );
    short b, nb = (short) ( m->nvert * 3 );

    for ( b = 0; b < nb; b++, (*i)++ )
        sum += (unsigned long) vb[b] * ( *i % 251 + 1 );
    return sum;
}

static void near mdl_report_one( const MdlState *m, const char *name )
{
    char buf[48];
    unsigned long sum = 0, i = 0;
    short f, s;

    if ( !m->loaded ) return;
    for ( f = 0; f < m->nframe; f++ ) sum = mdl_sum_frame( m, f, sum, &i );
    sprintf( buf, "mdl_sum %s %d %d %08lX", name, (int) m->nframe, (int) m->vpages, sum );
    mark( buf );
    /* each set as the draw picks its frames, through mdl_set_frame */
    for ( s = 0; s < MDL_NSETS; s++ ) {
        short count = mdl_set_count( m, s );
        if ( !count ) continue;
        for ( sum = 0, i = 0, f = 0; f < count; f++ )
            sum = mdl_sum_frame( m, mdl_set_frame( m, s, f ), sum, &i );
        sprintf( buf, "mdl_set %.8s %d %d %08lX", name, (int) s, (int) count, sum );
        mark( buf );
    }
}

void mdl_report_sums( World *world )
{
    short i;

    for ( i = 0; i < MDL_KINDS; i++ ) mdl_report_one( &world->mdl[i], mdl_name[i] );
    mdl_report_one( &world->vmdl, pl_view_name( world->vmdl_id ) );
    mdl_report_one( &world->boss, "boss" );
}

void mdl_load_boss( World *world )
{
    short k;

    world->boss.loaded = 0;
    for ( k = 0; k < world->trig_count; k++ )
        if ( world->trig[k].kind == ENT_TRIG_BOSS ) {
            mdl_load( &world->pak, &world->boss, "boss", MDL_BOSS_SETS );
            return;
        }
}

MdlState *mdl_of( World *world, short kind )
{
    if ( kind < 0 || kind >= MDL_KINDS ) return 0;
    if ( !world->mdl[kind].loaded ) return 0;
    return &world->mdl[kind];
}

void mdl_load_monsters( World *world, unsigned char far *buf, long *ofs, short count )
{
    short i;

    world->mon_count = count;
    world->mon = (MdlEnt far *) qglMemAlloc( (long) ( count ? count : 1 ) * sizeof(MdlEnt) );
    if ( !world->mon ) sys_error( "ents: out of memory for monsters" );
    _fmemset( world->mon, 0, (unsigned) ( count ? count : 1 ) * sizeof(MdlEnt) );

    for ( i = 0; i < count; i++ ) {
        EntsMon mr;
        MdlEnt far *e = &world->mon[i];

        _fmemcpy( &mr, buf + *ofs, sizeof(EntsMon) ); *ofs += sizeof(EntsMon);

        e->kind   = mr.kind;
        e->order  = mr.order;
        e->pos    = mr.org;
        e->yaw    = mr.angle;   /* a model's yaw is Quake's, CCW from +x: no mirror */
        e->patrol = mr.first;   /* its path_corner, -1 for none */
        e->cruc   = mr.cruc;
    }
}

void mdl_load_monster_models( World *world )
{
    short i, kinds[MDL_KINDS];

    for ( i = 0; i < MDL_KINDS; i++ ) { kinds[i] = 0; world->mdl[i].loaded = 0; }
    for ( i = 0; i < world->mon_count; i++ )
        if ( world->mon[i].kind >= 0 && world->mon[i].kind < MDL_KINDS )
            kinds[ world->mon[i].kind ] = 1;

    /* Only the kinds this map spawns: a model the map has none of is
       EMS pages and a skin for nothing, and mkassets ships only these
       in the container anyway. */
    for ( i = 0; i < MDL_KINDS; i++ )
        if ( kinds[i] ) mdl_load( &world->pak, &world->mdl[i], mdl_name[i], MDL_MON_NEED );
}
