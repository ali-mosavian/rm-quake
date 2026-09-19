#ifndef __PAK_H__
#define __PAK_H__

/*
 * pak.h -- QUAKE.QPK's map directories and member reader.
 *
 * QUAKE.QPK holds every converted map and the title demos behind a map
 * index. Its selected map has the old flat directory of (name, offset,
 * size), so every loader still reads through one open DOS handle. Payloads
 * shared by maps have one physical copy; their per-map directory entries
 * point to that same byte range.
 *
 * The file is opened ONCE and stays open. A member is not opened at
 * all: qglFileSeek puts the one handle at the member's offset and the
 * read takes its size, which is why there is no container driver in
 * the file layer for this format. That also means the handle is
 * SHARED and positioned by the last seek -- a loader reads its member
 * through before another loader asks for one.
 *
 * Nothing clamps a read to the member either: a read longer than the
 * directory says takes the next member's bytes. The size in the
 * directory is the contract. pak_load and pak_load_whole take it from
 * there; a caller reading in pieces (the geometry store, a row at a
 * time) clamps its last one itself.
 */

/* QPAK's top-level index and its map-local member directories.
   tools/mkqpak.py writes these exact fixed records. */
#define QPAK_SIG   0x4B415051L      /* "QPAK" */
#define QPAK_VER   1L
#define QPAK_FILE  "quake.qpk"
#define QPAK_MAP_NAME 8
#define QPAK_MAP_ENT  16
#define QPAK_MAX_MAPS 16
#define QMAP_NAME  16
#define QMAP_ENT   24
/* Members a map may carry. 19 lumps, three atlases and the palette,
   the two bar pieces, the two sound files, three files per monster
   kind the map spawns, three per view weapon, map-specific pickup models,
   and one projectile-model bundle. tools/mkassets.py refuses to write
   more than this. */
#define QMAP_MAX   80

typedef struct {
    char name[QMAP_NAME];
    long ofs, size;
} QmapEnt;

typedef struct {
    char  name[QPAK_MAP_NAME];
    short n;
    long  dirofs;
    short pad;
} QpakMap;

#ifndef QCPORT_NATIVE_TEST
typedef char rec_qmapent_ok[ sizeof(QmapEnt) == QMAP_ENT ? 1 : -1 ];
typedef char rec_qpakmap_ok[ sizeof(QpakMap) == QPAK_MAP_ENT ? 1 : -1 ];
#endif

typedef struct {
    short   fh;                 /* 0 until pak_open */
    short   n;
    QmapEnt dir[QMAP_MAX];
} Pak;

/* Opens QUAKE.QPK and reads one map's member directory. Fatal if the
   archive or requested map is missing, or if their format is wrong. */
void pak_open( Pak *pak, char *map );
void pak_close( Pak *pak );

/* Opens another archive handle at a member found in any map directory.
   Demos use it so their stream cannot move the world's shared handle.
   Returns 0 when no map carries member; the caller closes *out_fh. */
short pak_open_member( char *member, short *out_fh, long *out_offset,
                       long *out_bytes );

/* Puts the shared handle at member's first byte and answers the handle,
   its size through *out_bytes. Fatal if there is no such member. The
   two loaders below are this with an allocation and a read on top. */
short pak_seek( Pak *pak, char *member, long *out_bytes );

/* Positions at a byte range within a member. The range is checked
   against the directory before the shared handle moves. */
short pak_seek_part( Pak *pak, char *member, long offset, long bytes );

/* Is member in the directory? The only non-fatal question this layer
   answers: a container built without the PAK has no sounds, and that
   is a quiet run rather than a dead one. */
short pak_has( const Pak *pak, char *member );

/* qglMemAlloc(bytes), then read exactly that many bytes of the member
   into it. Fatal on a missing member, a short read or a failed
   allocation: nothing here can run without its map data. */
unsigned char far *pak_load( Pak *pak, char *member, long bytes );

/* Same, but bytes comes from the directory -- the visibility lump's
   own shape. */
unsigned char far *pak_load_whole( Pak *pak, char *member, long *out_bytes );

#endif
