#ifndef __PAK_H__
#define __PAK_H__

/*
 * pak.h -- the map container, and reading members out of it.
 *
 * A converted map is ONE file: mkassets.py's .qmp, holding every lump,
 * both texture atlases, the palette, the font and the colormap behind a
 * flat directory of (name, offset, size). Five files used to be staged
 * together -- the .bsp, assets.zip and three loose atlases -- and
 * nothing tied them to each other: e1m1's zip over dm3ish's atlases
 * drew every texture as some other one and ran perfectly happily,
 * because the offset table indexes whatever atlas is there.
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

/* The .qmp's own header and directory. QMAP_NAME and QMAP_ENT are
   mkassets.py's write_qmap; the guard below is the other half. */
#define QMAP_SIG   0x50414D51L      /* "QMAP" */
#define QMAP_VER   10L
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

#ifndef QCPORT_NATIVE_TEST
typedef char rec_qmapent_ok[ sizeof(QmapEnt) == QMAP_ENT ? 1 : -1 ];
#endif

typedef struct {
    short   fh;                 /* 0 until pak_open */
    short   n;
    QmapEnt dir[QMAP_MAX];
} Pak;

/* Opens the map container and reads its directory. Fatal if the file is
   missing, is not a .qmp, or is a version this build does not read --
   each said by name, because "wrong map" used to be a silent wrong
   picture. */
void pak_open( Pak *pak, char *qmp );
void pak_close( Pak *pak );

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
