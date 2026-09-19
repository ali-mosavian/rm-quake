#ifndef __WORLD_H__
#define __WORLD_H__

#include "bsptypes.h"
#include "rng.h"
#include "pak.h"
#include "sound.h"
#include "sprtype.h"

/* Cell heights are powers of two and a cell is at most one EMS page,
   so 1..16384 -- nine of them can matter and the rest cannot exist. */
#define TEX_HEIGHTS 15
#define WORLD_EMS_PAGE_SHIFT 14
#define WORLD_EMS_PAGE_BYTES ( 1L << WORLD_EMS_PAGE_SHIFT )
#define WORLD_EMS_PAGE_MASK  ( ( 1U << WORLD_EMS_PAGE_SHIFT ) - 1U )
#include "mdltypes.h"

/* A light of style 32 or more: light_use toggles the style between "m"
   and "a", and START_OFF starts it "a". stamp orders the uses: of two
   lights on one style the later used draws, and unused, the later in
   the map. */
typedef struct {
    short name, style, on, start_on, stamp;
} LightEnt;

/* One makestatic flame model. These records live in EMS; set selects
   flame2.mdl's small or large group. */
typedef struct {
    BspVec3 pos;
    unsigned char model, set;
} FlameEnt;
typedef char rec_flameent_ok[ sizeof(FlameEnt) == 14 ? 1 : -1 ];

/*
 * World -- the loaded map. Grows a field at a time as the module that
 * owns it gets ported; this is ent.bas's, pl_move.bas's and r_bsp.bas's
 * slice (models/nodes/planes/clip/leaves/lfc/PVS store, plus the entity
 * arrays ent.bas owns), plus faces/texinfo/miptex -- sb_build.c (d_surf.bas)
 * needs them, even though model.bas's own loader that would fill them
 * isn't ported yet. The surface cache itself is not here: SurfCache is
 * its own context struct (sc.h), not part of the map.
 *
 * Arrays are far pointers + counts rather than fixed-size members:
 * every one of these is sized at map-load time (REDIM in the old
 * BASIC), and nothing here should guess a bound before model.bas's
 * loader exists to allocate them.
 */
typedef struct {
    Rng         rng;             /* every die the game rolls, in order; first,
                                     so &world->rng is world itself */
    short       model_count;
    Submodel    far *models;
    Node        far *nodes;
    short       node_count;      /* the WORLD hud panel's own -- nothing in
                                     the walk itself needs a bound */
    short       vert_count;      /* likewise: the hud panel's WORLD stats */
    short       edge_count;
    Plane       far *planes;
    BrushModel  far *brush;      /* one per submodel, brush[0] is the world */

    ClipNode    far *clip;       /* hull 1: pre-expanded for the player box,
                                     walked by pl_trace -- no count needed,
                                     pl_trace never indexes past a child it
                                     was handed */

    short       leaf_count;       /* r_mark_leaves' PVS decompression loop
                                      bound -- a run can carry past the
                                      last leaf, and in real mode that
                                      writes over whatever follows the
                                      array if this isn't checked */
    Leaf        far *leaves;
    short       lfc_hnd;          /* one EMS page: leaf marksurface indices */
    short       pvs_hnd;         /* EMS: the run-length compressed PVS blob */
    long        pvs_bytes;       /* leaf.vis_list is a byte offset into it */

    short       face_count;      /* r_draw_world's frame_stamp-reset loop
                                     bound; the walk itself never needs
                                     this, only the once-per-32,767-frames
                                     reset does */
    Face        far *faces;
    TexInfo     far *texinfo;    /* indexed by faces[f].tex_info_id */
    MipTex      far *miptex;     /* indexed by texinfo[t].mip_tex */
    short       far *anim_tab;   /* every animation chain's texture ids,
                                     back to back; MipTex.anim_base is an
                                     offset into THIS, not a texture id --
                                     chains are not contiguous in the
                                     miptex lump (see mod_link_anims) */

    short       far *pt_idx;     /* portal adjacency, mkportals.py's own
                                     output: one entry per leaf plus one
                                     past the end, so leaf l's refs are
                                     pt_idx[l]..pt_idx[l+1] */
    short       far *pt_ref;     /* PT_REF_SHORTS (7) shorts per entry --
                                     neighbour leaf, then the portal's
                                     bsp-space bounding box */

    short       tele_count;
    Teleporter  far *tele;
    short       mon_teles;       /* monsters a teleporter moved */
    short       mon_jumps;       /* trigger_monsterjump throws that flew */
    BspVec3     fog_org[ENT_FOGS];
    float       fog_at[ENT_FOGS];  /* 0 free */

    short       plat_count;
    short       plat_max;        /* func_trains share the plat array */
    PlatEnt     far *plat;

    short       door_count;
    DoorEnt     far *door;

    short       trig_count;
    TrigEnt     far *trig;

    short       corner_count;
    PathCorner  far *corner;

    short       light_count;
    short       light_clock;     /* the last stamp handed out */
    LightEnt    far *light;

    short       item_count;
    short       item_max;        /* the map's own plus a backpack a monster */
    ItemEnt     far *item;
    short       item_mdl_count;
    short       backpack_mdl;
    AliasModel  far *item_mdl;
    signed char far *item_mdl_slot; /* per ItemEnt: model slot or -1 */
    signed char far *item_skin;     /* armor.mdl's green/yellow/red skin */

    short       fx_mdl_count;
    short       fx_mdl_hnd;          /* EMS page of effects.bin model descriptors */
    short       fx_geom_hnd;
    QSurf       fx_skin_atlas, fx_skin_view;
    short       fx_skin_aim;
    short       flame_count, flame_hnd;

    short       crate_count;
    CrateModel  far *crate;

    short       mon_count;
    MdlEnt      far *mon;
    MdlState    mdl[MDL_KINDS];   /* one per kind, only the map's own loaded */

    /* Only the view weapon in hand is resident. */
    MdlState    vmdl;
    short       vmdl_id;

    /* Chthon's, loaded with a map that has monster_boss */
    MdlState    boss;

    /* NUL string pool at the end of ents.bin. Entity message fields are
       16-bit byte offsets into it; zero names the empty first byte. */
    short       msg_size;
    char        far *msgs;
    char        far *title;       /* worldspawn message in the same pool */

    /* Which submodel owns a face is Face.side >> 1, written by
       ent_load_brushes -- see its own note. There is no array. */

    /* The three EMS-resident stores mod.c owns and hands scanlines/rows
       out of through mod_lm_map/mod_cm_map/mod_geom_map (sb_build.c
       already calls these, with exactly this World-taking signature,
       since before mod.c existed to implement them). */
    QSurf   geom_dc;      /* one face record per lookup, through mod_geom_map */
    short geom_rows;

    QSurf   light_atlas;  /* every luxel in the map, one 8-bit EMS dc */
    long  light_size;   /* bytes on disk */
    long  light_loaded; /* bytes that actually arrived */

    QSurf   cmap_dc;       /* the 64-shade table sb_build/hud_shade shade through */
    long  cmap_size;

    /* Every texture in two atlases: raw indices and row-0-shaded. */
    QSurf   tex_raw, tex_shaded;
    /* One tableless strided view per atlas, reshaped and re-aimed to the
       current cell. Consecutive faces usually reuse the same key. */
    QSurf   tex_v_raw, tex_v_shaded;
    short  tex_aim_raw, tex_aim_shd;
    long  tex_ofs[1024];    /* [id*4 + level] -> byte offset in the atlas */
    SpriteMeta far *sprite_meta;
    QSurf sprite_atlas, sprite_view;
    short sprite_aim;
    QSurf sky_layers, sky_dc;   /* d_sky.c's: back and front, composed */
    short sky_shift;            /* the front shift sky_dc holds, -1 none */
    short skill;                /* 0..3: which ents<N>.bin loaded */
    short serverflags;          /* svs.serverflags: the runes held as the map loaded */
    Pak   pak;                  /* the map's container, open for the run */
    Sound snd;
} World;

#endif
