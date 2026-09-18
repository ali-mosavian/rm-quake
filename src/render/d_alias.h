#ifndef __D_ALIAS_H__
#define __D_ALIAS_H__

#include "world.h"
#include "fight.h"
#include "renderer.h"
#include "qgltypes.h"
#include "pl_move.h"   /* Player */

/*
 * d_alias.h -- alias models and the other geometry outside the world BSP.
 */

short d_alias_selftest( void );

/*
 * name: d_draw_items
 * desc: Every pickup still there, spinning and bobbing on Quake's
 *       EF_ROTATE clock. Depth-tested, so one behind a wall is
 *       behind it -- nothing orders these against the world walk.
 *       Returns the quads drawn.
 */
short d_draw_items( World *world, Renderer *rdr, Player *player,
                     DiskPlane far *frustum, Mat4 *mtx_fin,
                     float xresh, float yresh, float z_near, QSurf dst );

/*
 * name: d_draw_models
 * desc: Every monster the map spawned, on the frame it is holding.
 *       Depth tested, like the pickups. Returns the triangles handed
 *       to the rasteriser.
 */
short d_draw_models( World *world, Renderer *rdr, DiskPlane far *frustum,
                      Mat4 *mtx_fin, float xresh, float yresh, float z_near,
                      QSurf dst );

/* Chthon, once boss_awake has raised him from the lava. */
short d_draw_boss( World *world, Renderer *rdr, DiskPlane far *frustum,
                    Mat4 *mtx_fin, float xresh, float yresh, float z_near,
                    QSurf dst );

/*
 * name: d_draw_view
 * desc: The weapon in hand, at the eye, turned with the view and drawn
 *       last with depth off -- Quake's own order for it. Nothing in
 *       the intermission. Returns the triangles drawn.
 */
short d_draw_view( World *world, Renderer *rdr, Camera *cam, Player *player,
                    Fight *fight, Mat4 *mtx_fin, float xresh, float yresh,
                    float z_near, QSurf dst );

/*
 * name: d_draw_spikes
 * desc: Everything in flight, using its original alias model. Depth
 *       tested like the rest and not gated by -nomdl.
 */
short d_draw_spikes( World *world, Renderer *rdr, Fight *fight,
                      Mat4 *mtx_fin, float xresh, float yresh,
                      float z_near, QSurf dst );

/* ThrowGib/ThrowHead alias models, after their bounce tick. */
short d_draw_gibs( World *world, Renderer *rdr, DiskPlane far *frustum,
                    Mat4 *mtx_fin, float xresh, float yresh,
                    float z_near, QSurf dst );

/* bolt.mdl/bolt3.mdl segments for every live keyed beam. */
short d_draw_beams( World *world, Renderer *rdr, DiskPlane far *frustum,
                     Mat4 *mtx_fin, float xresh, float yresh,
                     float z_near, QSurf dst );

#endif
