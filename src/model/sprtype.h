#ifndef __SPRTYPE_H__
#define __SPRTYPE_H__

#define SPR_MODEL_BUBBLE 0
#define SPR_MODEL_EXPLOD 1
#define SPR_MODEL_LIGHT  2
#define SPR_MODELS       3
#define SPR_FRAMES       9

typedef struct {
    unsigned short first, count;
} SpriteDef;

typedef struct {
    long ofs;
    short left, right, up, down;
    short tw, th;
} SpriteFrame;

typedef struct {
    long magic;
    unsigned short version, models, frames, header_bytes;
    SpriteDef def[SPR_MODELS];
    SpriteFrame frame[SPR_FRAMES];
} SpriteMeta;

typedef char rec_sprite_frame_ok[ sizeof(SpriteFrame) == 16 ? 1 : -1 ];
typedef char rec_sprite_meta_ok[ sizeof(SpriteMeta) == 168 ? 1 : -1 ];

#endif
