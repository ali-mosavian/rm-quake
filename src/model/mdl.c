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

    if ( h->nsets < need || h->nsets > MDL_NSETS ) return 0;
    for ( s = 0; s < MDL_NSETS; s++ ) {
        if ( s < need && !h->set_n[s] ) return 0;
        if ( s >= h->nsets && h->set_n[s] ) return 0;
        sum += h->set_n[s];
    }
    return sum == h->nframe;
}

static short near mdl_load_stream( Pak *pak, MdlState *m, char *name, short need,
                                   short fh, short split, QSurf *more, short nskins )
{
    char  path[24], msg[96];    /* path and the longest refusal's numbers */
    MdlHead h;
    short page, p, left;
    long  n, bytes;
    float ex, ey;

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
        sprintf( msg, "%s: not a QMD3 .geo -- rebuild the map's container", path ); mdl_refuse( msg );
        return 0;
    }
    if ( !mdl_sets_ok( &h, need ) ) {
        sprintf( msg, "%s: %d sets, %d needed, or not its %d frames", path, h.nsets, need, h.nframe );
        mdl_refuse( msg );
        return 0;
    }
    m->ntri = h.ntri; m->nvert = h.nvert; m->nframe = h.nframe;
    m->scale = h.scale; m->origin = h.origin;
    memcpy( m->set_n, h.set_n, MDL_NSETS );
    m->vpages = h.vpages; m->page_frames = h.page_frames;

    /* Vertex bytes span 0..255, so this is the box every frame fits in;
       the yaw turns about the origin, so the horizontal reach is a
       radius. What the visibility test asks about instead of 170
       vertices. */
    ex = (float) fabs( m->origin.x );
    if ( (float) fabs( m->origin.x + 255.0f * m->scale.x ) > ex )
        ex = (float) fabs( m->origin.x + 255.0f * m->scale.x );
    ey = (float) fabs( m->origin.y );
    if ( (float) fabs( m->origin.y + 255.0f * m->scale.y ) > ey )
        ey = (float) fabs( m->origin.y + 255.0f * m->scale.y );
    m->radius = (float) sqrt( ex * ex + ey * ey );
    m->zlo = m->origin.z;
    m->zhi = m->origin.z + 255.0f * m->scale.z;

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
#define ITEM_MDL_BACKPACK 20
#define ITEM_MDL_N        21

static char *item_mdl_name[ITEM_MDL_N] = {
    "armor", "g_shot", "g_nail", "quaddama", "suit",
    "g_rock", "g_nail2", "g_rock2", "invulner", "invisibl",
    "w_s_key", "w_g_key", "m_s_key", "m_g_key", "b_s_key", "b_g_key",
    "end1", "end2", "end3", "end4", "backpack"
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

    for ( i = 0; i < world->item_count; i++ ) {
        ItemEnt far *it = &world->item[i];
        id = item_mdl_id( it, worldtype );
        world->item_mdl_slot[i] = (signed char) ( id < 0 ? -1 : item_mdl_slot( world, id ) );
        world->item_skin[i] = (signed char) ( it->kind == ENT_ITEM_ARMOR2 ? 1 :
                                              it->kind == ENT_ITEM_ARMOR3 ? 2 : 0 );
    }
    /* Slots after the map's items are reserved for dropped backpacks.
       Demo snapshots use those slots directly rather than pl_item_add. */
    for ( i = world->item_count; i < world->item_max; i++ ) {
        world->item_mdl_slot[i] = (signed char) world->backpack_mdl;
        world->item_skin[i] = 0;
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

typedef struct { char name[8]; long size; } FxModelEnt;
typedef char rec_fxmodel_ok[ sizeof(FxModelEnt) == 12 ? 1 : -1 ];

static char *fx_mdl_name[MDL_FX_N] = {
    "spike", "s_spike", "grenade", "missile", "lavaball", "w_spike", "zom_gib"
};

void mdl_load_effects( World *world )
{
    struct { char magic[4]; short count; } head;
    FxModelEnt ent;
    AliasModel model;
    char name[9], msg[80];
    short fh, i;
    long bytes, left;

    fh = pak_seek( &world->pak, "effects.bin", &bytes );
    if ( qglFileRead( fh, (long) (void far *) &head, sizeof(head) ) != sizeof(head) ||
         memcmp( head.magic, "QFX1", 4 ) || head.count != MDL_FX_N )
        sys_error( "effects.bin: bad header" );

    left = bytes - sizeof(head);
    world->fx_mdl_count = head.count;
    world->fx_mdl = (AliasModel far *) qglMemAlloc( (long) head.count * sizeof(AliasModel) );
    if ( !world->fx_mdl ) sys_error( "effects.bin: out of memory for models" );

    for ( i = 0; i < head.count; i++ ) {
        if ( left < sizeof(ent) ||
             qglFileRead( fh, (long) (void far *) &ent, sizeof(ent) ) != sizeof(ent) )
            sys_error( "effects.bin: directory short read" );
        left -= sizeof(ent);
        memcpy( name, ent.name, 8 ); name[8] = 0;
        if ( strcmp( name, fx_mdl_name[i] ) || ent.size < sizeof(MdlHead) || ent.size > left )
            sys_error( "effects.bin: bad model record" );
        memset( &model, 0, sizeof(model) );
        model.id = i;
        if ( !mdl_load_stream( &world->pak, &model.state, name, 1, fh, 0, 0, 1 ) ) {
            sprintf( msg, "effects.bin: %s would not load", name );
            sys_error( msg );
        }
        _fmemcpy( &world->fx_mdl[i], &model, sizeof(model) );
        left -= ent.size;
    }
    if ( left ) sys_error( "effects.bin: trailing bytes" );
}

void mdl_unload_effects( World *world )
{
    short i;
    for ( i = 0; i < world->fx_mdl_count; i++ ) {
        AliasModel model;
        _fmemcpy( &model, &world->fx_mdl[i], sizeof(model) );
        mdl_unload( &model.state );
    }
    if ( world->fx_mdl ) qglMemFree( (long) world->fx_mdl );
    world->fx_mdl = 0;
    world->fx_mdl_count = 0;
}

unsigned char far *mdl_frame_verts( const MdlState *m, short frame, short slot )
{
    unsigned char far *vb = (unsigned char far *)
        ((unsigned long) qglGemMap( m->vtx_hnd, (short) ( frame / m->page_frames ), slot ) << 16);

    return vb + (long) ( frame % m->page_frames ) * m->nvert * 3;
}

MdlTri far *mdl_tris( const MdlState *m, short slot )
{
    return (MdlTri far *) ((unsigned long) qglGemMap( m->vtx_hnd, m->vpages, slot ) << 16);
}

short mdl_set_frame( const MdlState *m, short set, short anim )
{
    short s;

    for ( s = 0; s < set; s++ ) anim += m->set_n[s];
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
        if ( !m->set_n[s] ) continue;
        for ( sum = 0, i = 0, f = 0; f < m->set_n[s]; f++ )
            sum = mdl_sum_frame( m, mdl_set_frame( m, s, f ), sum, &i );
        sprintf( buf, "mdl_set %.8s %d %d %08lX", name, (int) s, (int) m->set_n[s], sum );
        mark( buf );
    }
}

void mdl_report_sums( World *world )
{
    short i;

    for ( i = 0; i < MDL_KINDS; i++ ) mdl_report_one( &world->mdl[i], mdl_name[i] );
    for ( i = 0; i < PL_VIEW_N; i++ ) mdl_report_one( &world->vmdl[i], pl_view_name( i ) );
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
    short i, kinds[MDL_KINDS];

    for ( i = 0; i < MDL_KINDS; i++ ) { kinds[i] = 0; world->mdl[i].loaded = 0; }

    world->mon_count = count;
    world->mon = (MdlEnt far *) qglMemAlloc( (long) ( count ? count : 1 ) * sizeof(MdlEnt) );
    if ( !world->mon ) sys_error( "ents: out of memory for monsters" );

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
        if ( mr.kind >= 0 && mr.kind < MDL_KINDS ) kinds[ mr.kind ] = 1;
    }

    /* Only the kinds this map spawns: a model the map has none of is
       EMS pages and a skin for nothing, and mkassets ships only these
       in the container anyway. */
    for ( i = 0; i < MDL_KINDS; i++ )
        if ( kinds[i] ) mdl_load( &world->pak, &world->mdl[i], mdl_name[i], MDL_MON_NEED );
}
