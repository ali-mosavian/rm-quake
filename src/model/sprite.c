/* sprite.c -- the three stock Quake sprite models in one EMS atlas. */

#include <mem.h>

#include "sprite.h"
#include "qgl.h"
#include "qglsurf.h"
#include "sys.h"

#define SPR_MAGIC 0x31505351L
#define SPR_ATLAS_W 256
#define SPR_SLOT 0

void sprite_load( World *world )
{
    SpriteMeta far *meta;
    unsigned char far *row;
    long bytes;

    world->sprite_atlas = qgl_surf_from_member( &world->pak, "sprites.bin",
                                                SPR_ATLAS_W, QGL_SURF_EMS, &bytes );
    if ( !world->sprite_atlas ) sys_error( "sprites.bin would not load" );
    meta = (SpriteMeta far *) qglMemAlloc( sizeof(SpriteMeta) );
    if ( !meta ) sys_error( "out of memory for sprite metadata" );
    row = (unsigned char far *) qglSfAccessRdEx( world->sprite_atlas, 0, SPR_SLOT );
    if ( !row ) sys_error( "sprite metadata would not map" );
    _fmemcpy( meta, row, sizeof(SpriteMeta) );
    if ( meta->magic != SPR_MAGIC || meta->version != 1 ||
         meta->models != SPR_MODELS || meta->frames != SPR_FRAMES ||
         meta->header_bytes != SPR_ATLAS_W )
        sys_error( "sprites.bin has the wrong format" );
    world->sprite_meta = meta;
    world->sprite_aim = -1;
}

void sprite_unload( World *world )
{
    if ( world->sprite_view ) qglDelView( world->sprite_view );
    if ( world->sprite_atlas ) qglSfFree( world->sprite_atlas );
    if ( world->sprite_meta ) qglMemFree( (long) world->sprite_meta );
    world->sprite_view = world->sprite_atlas = 0;
    world->sprite_meta = 0;
    world->sprite_aim = -1;
}

QSurf sprite_frame( World *world, short model, short frame, SpriteFrame *out )
{
    SpriteDef def;
    SpriteFrame sf;
    short key;

    if ( !world->sprite_meta || model < 0 || model >= SPR_MODELS ) return 0;
    def = world->sprite_meta->def[model];
    if ( frame < 0 || frame >= def.count ) return 0;
    key = (short) ( def.first + frame );
    sf = world->sprite_meta->frame[key];
    if ( !world->sprite_view ) {
        world->sprite_view = qglSfViewNew( world->sprite_atlas, sf.tw, sf.th, sf.tw );
        if ( !world->sprite_view ) return 0;
    }
    if ( world->sprite_aim != key ) {
        if ( !qglSfViewResize( world->sprite_view, world->sprite_atlas,
                               sf.tw, sf.th, sf.tw ) ||
             !qglSfViewAim( world->sprite_view, sf.ofs ) ) return 0;
        world->sprite_aim = key;
    }
    *out = sf;
    return world->sprite_view;
}
