/*
 * d_alias.c -- alias models and other non-world geometry: pickups,
 * monsters, projectiles and the view weapon.
 *
 * Everything here is DEPTH TESTED. A box is an object of its own with
 * no place in the BSP order -- the walk cannot sort it against the
 * world the way it sorts the world against itself -- so 1/z decides,
 * which is what the depth buffer is for.
 */

#include <math.h>
#include <mem.h>   /* _fmemset -- the leaf caches, once */

#include "qrcfg.h"
#include "d_alias.h"
#include "mdl.h"
#include "mdl_ai.h"
#include "weapons.h"
#include "item.h"
#include "gib.h"
#include "d_beam.h"
#include "qgl.h"
#include "mod.h"
#include "mod_tex.h"

/* The EMS slots a model reads through: PAGE_SLOT is what nodes,
   leaves and the lightmap already share, mapped fresh and never held;
   CM_SLOT is the colormap's, idle while a model draws, and the next
   surface build re-maps it for itself. */
#define PAGE_SLOT    2
#define CM_SLOT      3
#define MDL_CLIPV    9          /* five planes add at most one corner each */
#define MDL_UV_SCALE 32767.0f


/* Near, in DGROUP, like d_faces.c's scratch: 3 KB. Far statics here
   made bcc emit es:okv[bx], which TASM 4.1 rejects. */
static float near vx[MDL_MAXV + 1];
static float near vy[MDL_MAXV + 1];
static float near vw[MDL_MAXV + 1];
static char  near okv[MDL_MAXV + 1];    /* in front of the near plane */
/* Inside all five planes, and if so its projection, once per vertex
   rather than once per corner of every triangle that shares it. A
   triangle whose three corners are all inside skips the clip outright,
   which is nearly every triangle of a model that is on screen. */
static char  near inv[MDL_MAXV + 1];
static float near sx[MDL_MAXV + 1];
static float near sy[MDL_MAXV + 1];
static float near srw[MDL_MAXV + 1];

/*
 * The ping-pong clip rings and the projected polygon.
 *
 * ONE ARRAY OF A RECORD, and two pointers swapped -- not five arrays
 * subscripted [ring][k]. A two-dimensional subscript costs bcc a
 * multiply and an add at every one of the ~100 accesses a plane makes,
 * which is what made a clip 115us against the 30-odd float operations
 * it actually performs.
 */
typedef struct { float x, y, w, u, v; } ClipV;
static ClipV near cba[MDL_CLIPV], cbb[MDL_CLIPV];
static float near cd[MDL_CLIPV];
static QVert near qv[MDL_CLIPV];

/* Times the clip-space winding disagreed with the projected one on a
   triangle that reached both. It is the identity the pre-clip backface
   test rests on, checked on live data every frame for one compare, and
   any number but zero says the test is throwing away front faces
   somewhere it is not being watched. Read by -bench through
   mdl_bf_bad(). */
static long near bf_bad;

long mdl_bf_bad( void ) { return bf_bad; }

/* id's alias light points along world -x. qgl's alias path has no light
   vertex attribute yet, so this is one colormap row from the face normal. */
static short near mdl_lit_row( const MdlTri *t, unsigned char far *vb,
                               BspVec3 *scale, BspVec3 *origin,
                               float cyaw, float syaw, float cpitch, float spitch,
                               float croll, float sroll, short yaw_only )
{
    short i[3], k;
    float x[3], y[3], z[3], ax, ay, az, bx, by, bz, nx, ny, nz, len, bright;
    float rx, ry, rz, px, pz;

    i[0] = t->a; i[1] = t->b; i[2] = t->c;
    for ( k = 0; k < 3; k++ ) {
        rx = (float) vb[i[k]*3] * scale->x + origin->x;
        ay = (float) vb[i[k]*3 + 1] * scale->y + origin->y;
        az = (float) vb[i[k]*3 + 2] * scale->z + origin->z;
        if ( yaw_only ) {
            x[k] = rx * cyaw - ay * syaw;
            y[k] = rx * syaw + ay * cyaw;
            z[k] = az;
        } else {
            ry = ay * croll - az * sroll;
            rz = ay * sroll + az * croll;
            px = rx * cpitch + rz * spitch;
            pz = rz * cpitch - rx * spitch;
            x[k] = px * cyaw - ry * syaw;
            y[k] = px * syaw + ry * cyaw;
            z[k] = pz;
        }
    }
    ax = x[1] - x[0]; ay = y[1] - y[0]; az = z[1] - z[0];
    bx = x[2] - x[0]; by = y[2] - y[0]; bz = z[2] - z[0];
    nx = ay * bz - az * by;
    ny = az * bx - ax * bz;
    nz = ax * by - ay * bx;
    len = (float) sqrt( nx * nx + ny * ny + nz * nz );
    bright = 96.0f;
    if ( len > 0.001f && -nx / len < 0.0f ) bright += 64.0f * ( -nx / len );
    if ( bright < 0.0f ) bright = 0.0f;
    if ( bright > 128.0f ) bright = 128.0f;
    return (short) ( ( 128.0f - bright ) * 0.5f );
}

static short near mdl_lit_map( World *world, short row )
{
    unsigned char far *cmap = mod_cm_map( world );

    if ( !cmap ) return 0;
    return qglRsLitMap( (void far *) ( cmap + (long) row * 256L ) );
}

static short mdl_draw_tris(
    World      *world,
    short     ntri,
    short     nvert,
    short     frame,
    BspVec3  *org,              /* the monster's, BSP space, Z up */
    float     cyaw,             /* cos and sin of the yaw */
    float     syaw,
    float     cpitch,           /* and of the pitch, positive nose down */
    float     spitch,
    float     croll,
    float     sroll,
    BspVec3  *scale,            /* vertex byte -> model unit */
    BspVec3  *origin,
    const MdlState *ms,         /* its vertex pages and triangles */
    QSurf     skin,
    float    *m,                /* Mat4, 16 floats */
    float     xresh,
    float     yresh,
    float     z_near,
    QSurf     dst,
    short     zmode )           /* QGL_Z_TEST for a soldier, OFF for the view weapon */
{
    MdlTri far *tri;
    MdlTri t;
    unsigned char far *vb;
    short v, j, k, k2, cp, nin, nout, ns, nd, drawn = 0, lit;
    short bf_pre, yaw_only;
    ClipV *cs, *cw, *ct;
    short ia[3];
    float rx, ry, rz, wx, wy, wz, rw, f, area, px, pz, rry, rrz;
    float det;

    /* The frame's vertices, three bytes each, from the page that holds
       the frame. qglGemMap's own record makes this free when nobody else
       has taken the slot since the last model. */
    yaw_only = cpitch == 1.0f && spitch == 0.0f
            && croll == 1.0f && sroll == 0.0f;
    vb = mdl_frame_verts( ms, frame, PAGE_SLOT );
    for ( v = 0; v < nvert; v++ ) {
        rx = (float) vb[v*3]     * scale->x + origin->x;
        ry = (float) vb[v*3 + 1] * scale->y + origin->y;
        rz = (float) vb[v*3 + 2] * scale->z + origin->z;
        if ( yaw_only ) {
            wx = org->x + ( rx * cyaw - ry * syaw );
            wy = org->y + ( rx * syaw + ry * cyaw );
            wz = org->z + rz;
        } else {
            /* roll about model x, pitch about y, then yaw about world z:
               R_RotateForEntity's order. */
            rry = ry * croll - rz * sroll;
            rrz = ry * sroll + rz * croll;
            px = rx * cpitch + rrz * spitch;
            pz = rrz * cpitch - rx * spitch;
            wx = org->x + ( px * cyaw - rry * syaw );
            wy = org->y + ( px * syaw + rry * cyaw );
            wz = org->z + pz;
        }

        /* Renderer space is Y up: x unchanged, z becomes y -- the same
           swap d_faces.c makes reading a raw BSP vertex. */
        rx = wx; ry = wz; rz = wy;

        /* No divide here: a corner a hair in front of the near plane has
           a colossal 1/w. Keep w and clip against it first. */
        vw[v] = rx*m[3] + ry*m[7] + rz*m[11] + m[15];
        vx[v] = rx*m[0] + ry*m[4] + rz*m[ 8] + m[12];
        vy[v] = rx*m[1] + ry*m[5] + rz*m[ 9] + m[13];
        okv[v] = ( vw[v] >= z_near );
        inv[v] = okv[v] && vw[v] - vx[v] >= 0.0f && vw[v] + vx[v] >= 0.0f
                        && vw[v] - vy[v] >= 0.0f && vw[v] + vy[v] >= 0.0f;
        if ( inv[v] ) {
            /* The same expressions the clipped path evaluates, so the
               two agree to the bit on a corner that needs no clip. */
            rw = 1.0f / vw[v];
            sx[v] = xresh + vx[v] * rw * xresh;
            sy[v] = yresh - vy[v] * rw * yresh;
            srw[v] = rw;
        }
    }

    /* A model tests depth, never merely writes it: it comes after the
       world, which the BSP order put in front of it where it belongs.
       Off for us by qglSfZMode itself when dst has no depth buffer. */
    qglSfZMode( dst, zmode );

    for ( j = 0; j < ntri; j++ ) {
        short a, b, c;

        /* CM_SLOT alternates between this page and the colormap. The
           record survives that remap because it is copied before it. */
        tri = mdl_tris( ms, CM_SLOT );
        _fmemcpy( &t, tri + j, sizeof(t) );
        a = t.a; b = t.b; c = t.c;

        ia[0] = a; ia[1] = b; ia[2] = c;
        if ( !( okv[a] || okv[b] || okv[c] ) ) continue;

        if ( inv[a] && inv[b] && inv[c] ) {
            qv[0].x = sx[a]; qv[0].y = sy[a]; qv[0].z = srw[a];
            qv[1].x = sx[b]; qv[1].y = sy[b]; qv[1].z = srw[b];
            qv[2].x = sx[c]; qv[2].y = sy[c]; qv[2].z = srw[c];
            area = ( qv[1].x - qv[0].x ) * ( qv[2].y - qv[0].y )
                 - ( qv[2].x - qv[0].x ) * ( qv[1].y - qv[0].y );
            /*
             * Backface, or too small to cover a scanline: qglRsPoly
             * refuses anything whose denominator is under 2 (qgl$l1sqr,
             * twice the area of one pixel) and area IS that denominator
             * -- the triple it searches out of three vertices is these
             * three. e1m6's monsters hand over 287 triangles a frame and
             * 241 of them die in there, each having paid for a texture
             * map, a gradient and a clip first.
             */
            if ( area > -2.0f ) continue;
            qv[0].u = (float) t.u1 / MDL_UV_SCALE * srw[a];
            qv[0].v = (float) t.v1 / MDL_UV_SCALE * srw[a];
            qv[1].u = (float) t.u2 / MDL_UV_SCALE * srw[b];
            qv[1].v = (float) t.v2 / MDL_UV_SCALE * srw[b];
            qv[2].u = (float) t.u3 / MDL_UV_SCALE * srw[c];
            qv[2].v = (float) t.v3 / MDL_UV_SCALE * srw[c];
            /* The previous polygon mapped its EMS destination over PAGE_SLOT. */
            vb = mdl_frame_verts( ms, frame, PAGE_SLOT );
            lit = mdl_lit_map( world, mdl_lit_row( &t, vb, scale, origin,
                                                   cyaw, syaw, cpitch, spitch,
                                                   croll, sroll, yaw_only ) );
            if ( qglRsPoly( dst, (void far *) qv, 3,
                            lit ? QGL_M_ALIT : QGL_M_ATEX, skin ) > 0 ) drawn++;
            continue;
        }

        /*
         * BACKFACE BEFORE THE CLIP, not after. Clipping a triangle costs
         * 115 us -- measured, 31.5 clipped triangles a frame on e1m6 for
         * 3.64ms -- and half of any closed model's are facing away.
         *
         * The projected cross product is the 3x3 determinant of the clip
         * -space corners over w0*w1*w2, and the screen y flip negates
         * it, so with every w past the near plane sign(area) = -sign(det)
         * and no divide is needed. A corner BEHIND the near plane has a
         * w of the wrong sign and breaks that, so those go on to the
         * clipper and are tested after it as they always were.
         */
        bf_pre = 0;
        if ( okv[a] && okv[b] && okv[c] ) {
            det = vx[a] * ( vy[b] * vw[c] - vy[c] * vw[b] )
                - vy[a] * ( vx[b] * vw[c] - vx[c] * vw[b] )
                + vw[a] * ( vx[b] * vy[c] - vx[c] * vy[b] );
            if ( det <= 0.0f ) continue;
            bf_pre = 1;
        }

        cs = cba; cw = cbb; ns = 3;
        for ( k = 0; k < 3; k++ ) {
            cs[k].x = vx[ia[k]];
            cs[k].y = vy[ia[k]];
            cs[k].w = vw[ia[k]];
        }
        cs[0].u = (float) t.u1 / MDL_UV_SCALE;
        cs[1].u = (float) t.u2 / MDL_UV_SCALE;
        cs[2].u = (float) t.u3 / MDL_UV_SCALE;
        cs[0].v = (float) t.v1 / MDL_UV_SCALE;
        cs[1].v = (float) t.v2 / MDL_UV_SCALE;
        cs[2].v = (float) t.v3 / MDL_UV_SCALE;

        /* Sutherland-Hodgman against the five clip-space planes. The
           plane is chosen ONCE and then the whole ring measured, rather
           than a five-way switch inside the vertex loop. */
        for ( cp = 0; cp < 5; cp++ ) {
            switch ( cp ) {
            case 0:  for ( k = 0; k < ns; k++ ) cd[k] = cs[k].w - z_near;  break;
            case 1:  for ( k = 0; k < ns; k++ ) cd[k] = cs[k].w - cs[k].x; break;
            case 2:  for ( k = 0; k < ns; k++ ) cd[k] = cs[k].w + cs[k].x; break;
            case 3:  for ( k = 0; k < ns; k++ ) cd[k] = cs[k].w - cs[k].y; break;
            default: for ( k = 0; k < ns; k++ ) cd[k] = cs[k].w + cs[k].y; break;
            }
            nin = 0;
            for ( k = 0; k < ns; k++ ) if ( cd[k] >= 0.0f ) nin++;
            if ( nin == 0 ) { ns = 0; break; }
            if ( nin == ns ) continue;            /* wholly inside */

            nd = 0;
            for ( k = 0; k < ns; k++ ) {
                k2 = k + 1; if ( k2 == ns ) k2 = 0;
                if ( cd[k] >= 0.0f ) cw[nd++] = cs[k];
                if ( (cd[k] >= 0.0f) != (cd[k2] >= 0.0f) ) {
                    f = cd[k] / ( cd[k] - cd[k2] );
                    cw[nd].x = cs[k].x + f * ( cs[k2].x - cs[k].x );
                    cw[nd].y = cs[k].y + f * ( cs[k2].y - cs[k].y );
                    cw[nd].w = cs[k].w + f * ( cs[k2].w - cs[k].w );
                    cw[nd].u = cs[k].u + f * ( cs[k2].u - cs[k].u );
                    cw[nd].v = cs[k].v + f * ( cs[k2].v - cs[k].v );
                    nd++;
                }
            }
            ct = cs; cs = cw; cw = ct;
            ns = nd;
        }
        nout = ns;
        if ( nout < 3 ) continue;

        /* Projected u/z and v/z share the perspective edge walk. The
           affine filler recovers them once at each row endpoint. */
        for ( k = 0; k < nout; k++ ) {
            rw = 1.0f / cs[k].w;
            qv[k].x = xresh + cs[k].x * rw * xresh;
            qv[k].y = yresh - cs[k].y * rw * yresh;
            qv[k].z = rw;
            qv[k].u = cs[k].u * rw;
            qv[k].v = cs[k].v * rw;
        }

        /* Backface after the clip: clipping preserves winding, so the
           first three corners answer for all of them. */
        area = ( qv[1].x - qv[0].x ) * ( qv[2].y - qv[0].y )
             - ( qv[2].x - qv[0].x ) * ( qv[1].y - qv[0].y );
        if ( bf_pre && area >= 0.0f ) bf_bad++;
        if ( area < 0.0f ) {
            /* The previous polygon mapped its EMS destination over PAGE_SLOT. */
            vb = mdl_frame_verts( ms, frame, PAGE_SLOT );
            lit = mdl_lit_map( world, mdl_lit_row( &t, vb, scale, origin,
                                                   cyaw, syaw, cpitch, spitch,
                                                   croll, sroll, yaw_only ) );
            if ( qglRsPoly( dst, (void far *) qv, nout,
                            lit ? QGL_M_ALIT : QGL_M_ATEX, skin ) > 0 ) drawn++;
        }
    }
    return drawn;
}


/*
 * The model bounds against the frustum, then its eight corners and its
 * centre against the PVS, each a descent of the tree. The near corner
 * alone decides the frustum test, as r_cull_box does -- there are no
 * children to hand a clip mask to.
 */
static short d_mdl_visible( World *world, Renderer *rdr, DiskPlane far *frustum,
                             BspVec3 *org, float radius, float zlo, float zhi,
                             LeafCache far *lc )
{
    float lo[3], hi[3], px, py, pz, dp;
    short i, nodenr, vis = 0;

#if QR_PROF
    rdr->dv_calls++;
#endif
    lo[0] = org->x - radius; hi[0] = org->x + radius;
    lo[1] = org->y - radius; hi[1] = org->y + radius;
    lo[2] = org->z + zlo;    hi[2] = org->z + zhi;

    /* the frustum is renderer space, y up: the box's z goes in y */
    for ( i = 0; i < 6; i++ ) {
        px = frustum[i].norm.x > 0.0f ? lo[0] : hi[0];
        py = frustum[i].norm.y > 0.0f ? lo[2] : hi[2];
        pz = frustum[i].norm.z > 0.0f ? lo[1] : hi[1];
        dp = frustum[i].norm.x * px + frustum[i].norm.y * py + frustum[i].norm.z * pz;
        if ( dp + frustum[i].dist > 0.0f ) {
#if QR_PROF
            rdr->dv_fout++;
#endif
            return 0;
        }
    }

    /* Same box as last time: the leaves are the same, whatever the PVS
       now says about them. The whole of the saving is here. */
    if ( lc && lc->ok && lc->at.x == org->x && lc->at.y == org->y && lc->at.z == org->z
         && lc->r == radius && lc->zlo == zlo && lc->zhi == zhi ) {
        for ( i = 0; i < 9; i++ ) {
            nodenr = lc->lf[i];
            if ( nodenr > 0 && rdr->pvs_now[nodenr] ) {
#if QR_PROF
                rdr->dv_vis++;
#endif
                return -1;
            }
        }
        return 0;
    }

    for ( i = 0; i < 9; i++ ) {
        if ( i == 8 ) {
            px = org->x; py = org->y; pz = org->z + ( zlo + zhi ) * 0.5f;
        } else {
            px = ( i & 1 ) ? hi[0] : lo[0];
            py = ( i & 2 ) ? hi[1] : lo[1];
            pz = ( i & 4 ) ? hi[2] : lo[2];
        }
        /* r_point_leaf: the tree in BSP space, z up, no swap */
#if QR_PROF
        rdr->dv_desc++;
#endif
        nodenr = 0;
        while ( !( nodenr & 0x8000 ) ) {
            Plane far *pl = &world->planes[ world->nodes[nodenr].plane_id ];
            dp = px * pl->norm.x + py * pl->norm.y + pz * pl->norm.z - pl->dist;
            nodenr = dp >= 0.0f ? world->nodes[nodenr].child0 : world->nodes[nodenr].child1;
        }
        nodenr = (short) ~nodenr;
        if ( lc ) lc->lf[i] = nodenr;
        if ( nodenr > 0 && rdr->pvs_now[nodenr] ) {
            vis = -1;
            /* with nowhere to record them, the remaining eight are
               wasted work -- answer now, as this always did */
            if ( !lc ) {
#if QR_PROF
                rdr->dv_vis++;
#endif
                return -1;
            }
        }
    }
    if ( lc ) {
        lc->at = *org; lc->r = radius; lc->zlo = zlo; lc->zhi = zhi; lc->ok = 1;
    }
#if QR_PROF
    if ( vis ) rdr->dv_vis++;
#endif
    return vis;
}

/* Temp entities have already passed the server's PVS. Their arbitrary
   pitch and roll need a sphere, not d_mdl_visible's upright box. */
static short near d_mdl_sphere_visible( DiskPlane far *frustum,
                                         BspVec3 *org, float radius )
{
    float dp;
    short i;

    for ( i = 0; i < 6; i++ ) {
        dp = frustum[i].norm.x * org->x
           + frustum[i].norm.y * org->z
           + frustum[i].norm.z * org->y
           + frustum[i].dist;
        if ( dp > radius ) return 0;
    }
    return -1;
}

/* One slot per entity, made on the first frame that wants it. Pickups use
   item_max because DropBackpack grows item_count after this allocation. A
   refusal leaves the pointer null and every call recomputes, which is
   what this did before. */
static LeafCache far *lc_slot( Renderer *rdr, LeafCache far **arr, short n, short i )
{
    if ( n <= 0 || rdr->no_lcache ) return (LeafCache far *) 0;
    if ( !*arr ) {
        *arr = (LeafCache far *) qglMemAlloc( (long) n * sizeof(LeafCache) );
        if ( !*arr ) return (LeafCache far *) 0;
        _fmemset( *arr, 0, (unsigned) ( n * sizeof(LeafCache) ) );
    }
    return &(*arr)[i];
}

/*
 * The fallback pickup and exploding-box placeholder: half wide either
 * way, top high, spun by yaw, six flat quads.
 */
static short mdl_draw_box( BspVec3 *org, float half, float top,
                            float cyaw, float syaw, float *m,
                            float xresh, float yresh, float z_near,
                            QSurf dst, short side_col, short top_col )
{
    static short face[6][4] = {
        {0,1,2,3}, {7,6,5,4}, {0,4,5,1}, {1,5,6,2}, {2,6,7,3}, {3,7,4,0} };
    float bx[8], by[8], bw[8];
    float lx, ly, lz, rx, ry, rz, rw;
    short i, k;

    for ( i = 0; i < 8; i++ ) {
        lx = ( (i & 3) == 1 || (i & 3) == 2 ) ? half : -half;
        ly = ( (i & 3) >= 2 ) ? half : -half;
        lz = ( i >= 4 ) ? top : 0.0f;
        /* BSP is z up and the matrix is the renderer's, y up */
        rx = org->x + ( lx * cyaw - ly * syaw );
        rz = org->y + ( lx * syaw + ly * cyaw );
        ry = org->z + lz;
        bw[i] = rx*m[3] + ry*m[7] + rz*m[11] + m[15];
        if ( bw[i] < z_near ) return 0;
        bx[i] = rx*m[0] + ry*m[4] + rz*m[ 8] + m[12];
        by[i] = rx*m[1] + ry*m[5] + rz*m[ 9] + m[13];
    }
    for ( i = 0; i < 6; i++ ) {
        for ( k = 0; k < 4; k++ ) {
            rw = 1.0f / bw[ face[i][k] ];
            qv[k].x = xresh + bx[ face[i][k] ] * rw * xresh;
            qv[k].y = yresh - by[ face[i][k] ] * rw * yresh;
            qv[k].z = rw;
            qv[k].u = 0.0f; qv[k].v = 0.0f;
        }
        qglRsPoly( dst, (void far *) qv, 4, QGL_M_FLAT,
                    (long) ( i == 1 ? top_col : side_col ) );
    }
    return 6;
}

/*
 * A pickup as its b_*.bsp: five textured quads from org's corner -- the
 * bottom is on the floor and never shipped. A +N chain steps at 10 Hz
 * like the world's. A face with a corner behind the near plane is
 * dropped; the rest of the box still draws.
 */
static short mdl_draw_crate( World *world, BspVec3 *org, CrateModel far *c,
                              float *m, float xresh, float yresh, float z_near,
                              QSurf dst, short mip, float anim_time )
{
    float bx[8], by[8], bw[8];
    float rx, ry, rz, rw;
    QSurf src;
    long  step;
    short i, k, ci, drawn = 0;
    CrateFace far *cf;

    step = (long) ( anim_time * 10.0f );
    for ( i = 0; i < 8; i++ ) {
        rx = org->x + ( (i & 1) ? c->size.x : 0.0f );
        rz = org->y + ( (i & 2) ? c->size.y : 0.0f );
        ry = org->z + ( (i & 4) ? c->size.z : 0.0f );
        bw[i] = rx*m[3] + ry*m[7] + rz*m[11] + m[15];
        bx[i] = rx*m[0] + ry*m[4] + rz*m[ 8] + m[12];
        by[i] = rx*m[1] + ry*m[5] + rz*m[ 9] + m[13];
    }
    for ( i = 0; i < 5; i++ ) {
        cf = &c->f[i];
        for ( k = 0; k < 4; k++ ) if ( bw[ cf->v[k*3] ] < z_near ) break;
        if ( k < 4 ) continue;
        src = mod_tex_shaded( world,
                (short) ( cf->tex + ( cf->frames > 1 ? (short) ( step % cf->frames ) : 0 ) ), mip );
        if ( src == 0 ) continue;
        for ( k = 0; k < 4; k++ ) {
            ci = cf->v[k*3];
            rw = 1.0f / bw[ci];
            qv[k].x = xresh + bx[ci] * rw * xresh;
            qv[k].y = yresh - by[ci] * rw * yresh;
            qv[k].z = rw;
            qv[k].u = (float) cf->v[k*3+1] / 32.0f * rw;
            qv[k].v = (float) cf->v[k*3+2] / 32.0f * rw;
        }
        qglRsPoly( dst, (void far *) qv, 4, QGL_M_ATEX, src );
        drawn++;
    }
    return drawn;
}

short d_draw_items( World *world, Renderer *rdr, Player *player,
                     DiskPlane far *frustum, Mat4 *mtx_fin,
                     float xresh, float yresh, float z_near, QSurf dst )
{
    float *m = (float *) mtx_fin;
    float cyaw, syaw;
    short i, side, top, drawn = 0;
    BspVec3 bob;

    rdr->item_mdl_drawn = 0;
    if ( !world->item_count || rdr->no_items ) return 0;

    /* R_RotateForEntity: EF_ROTATE models turn at 100 degrees/sec. */
    cyaw = (float) cos( rdr->anim_time * 100.0 * 3.14159265 / 180.0 );
    syaw = (float) sin( rdr->anim_time * 100.0 * 3.14159265 / 180.0 );
    qglSfZMode( dst, QGL_Z_TEST );

    for ( i = 0; i < world->item_count; i++ ) {
        ItemEnt far *it = &world->item[i];
        AliasModel im;
        short have_mdl = 0;
        short have_crate = (short) ( it->crate >= 0 && it->crate < world->crate_count );

        if ( it->gone ) continue;
        bob = it->pos;
        if ( world->item_mdl_slot[i] >= 0 && world->item_mdl_slot[i] < world->item_mdl_count ) {
            _fmemcpy( &im, &world->item_mdl[world->item_mdl_slot[i]], sizeof(im) );
            have_mdl = im.state.loaded;
        }
        if ( have_crate ) {
            CrateModel far *cm = &world->crate[it->crate];
            float radius;
            bob.x += cm->size.x * 0.5f;
            bob.y += cm->size.y * 0.5f;
            bob.z += cm->size.z * 0.5f;
            radius = 0.5f * (float) sqrt( cm->size.x * cm->size.x +
                                          cm->size.y * cm->size.y );
            if ( !d_mdl_visible( world, rdr, frustum, &bob, radius,
                                  cm->size.z * -0.5f, cm->size.z * 0.5f,
                                  lc_slot( rdr, &rdr->item_lc, world->item_max, i ) ) ) continue;
        } else if ( have_mdl ) {
            if ( !d_mdl_visible( world, rdr, frustum, &bob,
                                  im.state.radius, im.state.zlo, im.state.zhi,
                                  lc_slot( rdr, &rdr->item_lc, world->item_max, i ) ) ) continue;
        } else if ( !d_mdl_visible( world, rdr, frustum, &bob,
                                     ENT_BOX_HALF * 1.5f, 0.0f, ENT_BOX_TOP + 8.0f,
                                     lc_slot( rdr, &rdr->item_lc, world->item_max, i ) ) ) continue;

        if ( have_crate ) {
            /* The map's own b_*.bsp spans origin to origin + size, as
               its asymmetric trigger does. The mip is by distance,
               using d_faces.c's own thresholds. */
            CrateModel far *cm = &world->crate[ it->crate ];
            BspVec3 corg = it->pos;
            float dx, dy, dz2, cdist;
            short cmip = 0;

            dx = corg.x - player->pos.x;
            dy = corg.y - player->pos.y;
            dz2 = corg.z - player->pos.z;
            cdist = (float) sqrt( dx*dx + dy*dy + dz2*dz2 );
            if ( rdr->use_mips ) {
                if      ( cdist >= 1400.0f ) cmip = 3;
                else if ( cdist >= 560.0f )  cmip = 2;
                else if ( cdist >= 280.0f )  cmip = 1;
            }
            drawn = (short) ( drawn + mdl_draw_crate( world, &corg, cm, m,
                                 xresh, yresh, z_near, dst, cmip, rdr->anim_time ) );
            continue;
        }

        if ( it->kind == ENT_ITEM_EXPLOBOX ) {
            /* the box stands still, b_explob's size */
            drawn = (short) ( drawn + mdl_draw_box( &bob, ENT_BOX_HALF, ENT_BOX_TOP,
                                 1.0f, 0.0f, m, xresh, yresh, z_near, dst,
                                 ENT_COL_YELLOW, ENT_COL_BROWN ) );
            continue;
        }

        if ( have_mdl ) {
            QSurf skin = world->item_skin[i] == 1 ? im.skin1 :
                         world->item_skin[i] == 2 ? im.skin2 : im.state.skin;
            short n = mdl_draw_tris( world, im.state.ntri, im.state.nvert, 0, &bob,
                                      cyaw, syaw, 1.0f, 0.0f, 1.0f, 0.0f,
                                      &im.state.scale, &im.state.origin, &im.state, skin,
                                      m, xresh, yresh, z_near, dst, QGL_Z_TEST );
            drawn = (short) ( drawn + n );
            rdr->item_mdl_drawn = (short) ( rdr->item_mdl_drawn + n );
            continue;
        }

        item_box_colours( it->kind, &side, &top );
        drawn = (short) ( drawn + mdl_draw_box( &bob, ENT_ITEM_HALF, ENT_ITEM_TOP,
                             cyaw, syaw, m, xresh, yresh, z_near, dst, side, top ) );
    }
    return drawn;
}


short d_draw_models( World *world, Renderer *rdr, DiskPlane far *frustum,
                      Mat4 *mtx_fin, float xresh, float yresh, float z_near,
                      QSurf dst )
{
    float *m = (float *) mtx_fin;
    short i, drawn = 0;

    if ( rdr->no_mdl ) return 0;

    for ( i = 0; i < world->mon_count; i++ ) {
        MdlEnt far *e = &world->mon[i];
        MdlState *ms;
        /* near: `world->mon` is far, and a far-to-near cast drops the
           segment silently -- the box would be read out of DS. */
        BspVec3 org;
        float a;
        short frame;

        if ( e->state == MDL_ST_GIBBED ) continue;
        ms = mdl_of( world, e->kind );
        if ( !ms ) continue;
        org = e->pos;
        if ( !d_mdl_visible( world, rdr, frustum, &org,
                              ms->radius, ms->zlo, ms->zhi,
                              lc_slot( rdr, &rdr->mdl_lc, world->mon_count, i ) ) ) continue;

        /* A leaper has no leap frames on the page and flies through the
           run cycle. */
        switch ( e->state ) {
        case MDL_ST_STAND: frame = mdl_set_frame( ms, MDL_SET_STAND, e->anim_frame ); break;
        case MDL_ST_RUN:
        case MDL_ST_LEAP:  frame = mdl_set_frame( ms, MDL_SET_RUN, e->anim_frame ); break;
        case MDL_ST_DEAD:  frame = mdl_set_frame( ms, MDL_SET_DEATH, e->anim_frame ); break;
        case MDL_ST_PAIN:  frame = mdl_set_frame( ms, MDL_SET_PAIN, e->anim_frame ); break;
        case MDL_ST_DEMO:  frame = e->anim_frame; break;
        default:           frame = mdl_set_frame( ms, MDL_SET_ATK, e->anim_frame ); break;
        }
        if ( frame >= ms->nframe ) frame = (short) ( ms->nframe - 1 );

        a = e->yaw * 3.14159265f / 180.0f;
        drawn = (short) ( drawn + mdl_draw_tris(
            world, ms->ntri, ms->nvert, frame, &org,
            (float) cos( a ), (float) sin( a ), 1.0f, 0.0f, 1.0f, 0.0f,
            &ms->scale, &ms->origin, ms, ms->skin,
            m, xresh, yresh, z_near, dst, QGL_Z_TEST ) );
    }
    return drawn;
}

short d_draw_gibs( World *world, Renderer *rdr, DiskPlane far *frustum,
                    Mat4 *mtx_fin, float xresh, float yresh,
                    float z_near, QSurf dst )
{
    float *m = (float *) mtx_fin;
    short i, drawn = 0;

    rdr->gib_mdl_drawn = 0;
    for ( i = 0; i < rdr->gib_count; i++ ) {
        GibEnt gib;
        GibEnt far *pool;
        EffectModel model;
        QSurf skin;
        float pitch, yaw, roll;
        short n;

        /* Model geometry and triangles consume the other EMS mappings,
           so re-map the state page before copying each small record. */
        pool = gib_map( rdr, PAGE_SLOT );
        if ( !pool ) break;
        _fmemcpy( &gib, &pool[i], sizeof(gib) );
        if ( !gib.model || gib.model >= world->fx_mdl_count ) continue;
        _fmemcpy( &model, &world->fx_mdl[gib.model], sizeof(model) );
        if ( !d_mdl_visible( world, rdr, frustum, &gib.pos,
                              model.state.radius, model.state.zlo, model.state.zhi, 0 ) ) continue;
        skin = mdl_effect_skin( world, gib.model );
        if ( !skin ) continue;
        pitch = gib.ang.x * 3.14159265f / 180.0f;
        yaw   = gib.ang.y * 3.14159265f / 180.0f;
        roll  = gib.ang.z * 3.14159265f / 180.0f;
        n = mdl_draw_tris( world, model.state.ntri, model.state.nvert, 0, &gib.pos,
                            (float) cos(yaw), (float) sin(yaw),
                            (float) cos(pitch), (float) sin(pitch),
                            (float) cos(roll), (float) sin(roll),
                            &model.state.scale, &model.state.origin, &model.state,
                            skin, m, xresh, yresh, z_near, dst, QGL_Z_TEST );
        drawn = (short) ( drawn + n );
    }
    rdr->gib_mdl_drawn = drawn;
    return drawn;
}

static short near boss_frame( TrigEnt far *t, MdlState *ms, float now )
{
    float elapsed;
    short set, frame;

    if ( t->state == ENT_TRIG_READY ||
         ( t->state == ENT_TRIG_DONE && t->delay_left <= 0.0f ) ) return -1;

    if ( t->state == ENT_TRIG_DONE ) {
        elapsed = ENT_BOSS_DYING - t->delay_left;
        if ( elapsed < 1.0f ) {
            set = MDL_BOSS_SHOCKC;
            frame = (short) ( elapsed * 10.0f );
        } else {
            set = MDL_BOSS_DEATH;
            frame = (short) ( ( elapsed - 1.0f ) * 10.0f );
            if ( frame >= ms->set_n[set] ) frame = (short) ( ms->set_n[set] - 1 );
        }
        return mdl_set_frame( ms, set, frame );
    }

    elapsed = now - t->wait_left;
    if ( elapsed < 0.0f ) elapsed = 0.0f;
    set = t->snd;
    if ( set == MDL_BOSS_RISE ) {
        float rise = (float) ms->set_n[set] * 0.1f;
        if ( elapsed < rise )
            return mdl_set_frame( ms, set, (short) ( elapsed * 10.0f ) );
        elapsed -= rise;
        set = MDL_BOSS_ATTACK;
    } else if ( set == MDL_BOSS_SHOCKA || set == MDL_BOSS_SHOCKB ) {
        if ( elapsed < 1.0f ) {
            frame = (short) ( elapsed * 10.0f );
            if ( frame >= ms->set_n[set] ) frame -= ms->set_n[set];
            return mdl_set_frame( ms, set, frame );
        }
        elapsed -= 1.0f;
        set = MDL_BOSS_ATTACK;
    } else {
        set = MDL_BOSS_ATTACK;
    }
    frame = (short) ( elapsed * 10.0f ) % ms->set_n[set];
    return mdl_set_frame( ms, set, frame );
}

short d_draw_boss( World *world, Renderer *rdr, DiskPlane far *frustum,
                    Mat4 *mtx_fin, float xresh, float yresh, float z_near,
                    QSurf dst )
{
    MdlState *ms = &world->boss;
    TrigEnt far *t;
    BspVec3 org;
    float a;
    short k, frame;

    if ( rdr->no_mdl || !ms->loaded ) return 0;
    for ( k = 0; k < world->trig_count; k++ ) {
        t = &world->trig[k];
        if ( t->kind != ENT_TRIG_BOSS ) continue;
        frame = boss_frame( t, ms, rdr->anim_time );
        if ( frame < 0 ) return 0;
        org = t->mins;
        if ( !d_mdl_visible( world, rdr, frustum, &org,
                              ms->radius, ms->zlo, ms->zhi, 0 ) ) return 0;
        a = t->speed * 3.14159265f / 180.0f;
        return mdl_draw_tris( world, ms->ntri, ms->nvert, frame, &org,
                              (float) cos( a ), (float) sin( a ), 1.0f, 0.0f, 1.0f, 0.0f,
                              &ms->scale, &ms->origin, ms, ms->skin,
                              (float *) mtx_fin, xresh, yresh, z_near, dst,
                              QGL_Z_TEST );
    }
    return 0;
}

/*
 * The view weapon: the model in hand at the eye, turned by the view's
 * own yaw and pitch and drawn last with depth OFF, which is what Quake
 * does -- it is not in the world and must not be hidden by it.
 *
 * cam->look_at is a POINT by the time this runs (v_update_camera makes
 * it one on the way out), so the direction is the difference: the
 * yaw's cos and sin are its x and z over their length, the pitch's are
 * that length and -y, positive looking down.
 */
short d_draw_view( World *world, Renderer *rdr, Camera *cam, Player *player,
                    Fight *fight, Mat4 *mtx_fin, float xresh, float yresh,
                    float z_near, QSurf dst )
{
    MdlState *ms;
    BspVec3 eye;
    float dx, dy, dz, len;
    short frame;

    if ( rdr->no_mdl || rdr->no_view ) return 0;
    if ( rdr->demo_view_frame == -2 ) return 0;
    if ( fight->state >= GS_EXIT ) return 0;     /* no gun in an intermission/finale */
    if ( rdr->anim_time < fight->invis_until ) return 0;   /* R_DrawViewModel: none while invisible */
    ms = &world->vmdl[ pl_view_of( fight->weapon ) ];
    if ( !ms->loaded ) return 0;

    eye = player->pos;
    eye.z += PL_EYE;

    dx = cam->look_at.x - cam->pos.x;
    dy = cam->look_at.y - cam->pos.y;
    dz = cam->look_at.z - cam->pos.z;
    len = (float) sqrt( dx * dx + dz * dz );
    if ( len < 0.001f ) len = 0.001f;

    /* frame 0 is the gun held; the fire set runs at 10 Hz from the
       shot and holds its last frame until the weapon is ready again.
       The nailgun cycles its eight while fire is held, as
       player_nail1/2 do. */
    frame = rdr->demo_view_frame;
    if ( frame < 0 ) frame = 0;
    if ( rdr->demo_view_frame < 0 && rdr->anim_time < fight->next_fire ) {
        frame = (short) ( 1 + (short) ( ( rdr->anim_time - fight->fire_at ) * 10.0f ) );
        if ( fight->weapon == PL_IT_NAILGUN || fight->weapon == PL_IT_SNG )
            frame = (short) ( 1 + ( (long) ( rdr->anim_time * 10.0f ) % 8 ) );
    }
    if ( frame >= ms->nframe ) frame = (short) ( ms->nframe - 1 );

    return mdl_draw_tris( world, ms->ntri, ms->nvert, frame, &eye,
                          dx / len, dz / len, len, -dy, 1.0f, 0.0f,
                          &ms->scale, &ms->origin, ms, ms->skin,
                          (float *) mtx_fin, xresh, yresh, z_near, dst, QGL_Z_OFF );
}

static short near projectile_model( const Spike *s )
{
    if ( s->gib ) return MDL_FX_ZOMGIB;
    if ( s->toss ) return MDL_FX_LAVABALL;
    if ( s->rocket ) return MDL_FX_MISSILE;
    if ( s->grenade ) return MDL_FX_GRENADE;
    if ( s->wiz ) return MDL_FX_WSPIKE;
    return s->dmg >= PL_SNG_DMG ? MDL_FX_SSPIKE : MDL_FX_SPIKE;
}

/* Original alias geometry for every projectile in flight. Straight
   missiles point down velocity; Quake's grenade and zombie chunk spin
   on all three axes. */
short d_draw_spikes( World *world, Renderer *rdr, Fight *fight,
                      Mat4 *mtx_fin, float xresh, float yresh,
                      float z_near, QSurf dst )
{
    float *m = (float *) mtx_fin;
    short i, drawn = 0;

    rdr->projectile_mdl_drawn = 0;

    for ( i = 0; i < PL_NAILS_MAX; i++ ) {
        Spike *s = &fight->nail[i];
        EffectModel model;
        BspVec3 at;
        float h, len, a, cyaw, syaw, cpitch, spitch, croll, sroll;
        QSurf skin;
        short id, frame, n;

        if ( !s->alive ) continue;
        id = projectile_model( s );
        if ( id < 0 || id >= world->fx_mdl_count ) continue;
        _fmemcpy( &model, &world->fx_mdl[id], sizeof(model) );
        if ( !model.state.loaded ) continue;
        skin = mdl_effect_skin( world, id );
        if ( !skin ) continue;
        at = s->pos;

        cyaw = cpitch = croll = 1.0f;
        syaw = spitch = sroll = 0.0f;
        if ( s->grenade ) {
            a = rdr->anim_time * ( s->gib ? 3000.0f : 300.0f ) * 3.14159265f / 180.0f;
            cyaw = (float) cos( a ); syaw = (float) sin( a );
            a = rdr->anim_time * ( s->gib ? 1000.0f : 300.0f ) * 3.14159265f / 180.0f;
            cpitch = (float) cos( a ); spitch = (float) sin( a );
            a = rdr->anim_time * ( s->gib ? 2000.0f : 300.0f ) * 3.14159265f / 180.0f;
            croll = (float) cos( a ); sroll = (float) sin( a );
        } else if ( !s->toss ) {
            h = (float) sqrt( s->vel.x*s->vel.x + s->vel.y*s->vel.y );
            len = (float) sqrt( h*h + s->vel.z*s->vel.z );
            if ( len > 0.001f ) {
                if ( h > 0.001f ) {
                    cyaw = s->vel.x / h;
                    syaw = s->vel.y / h;
                }
                cpitch = h / len;
                spitch = -s->vel.z / len;
            }
        }

        frame = model.state.nframe > 1
              ? (short) ( (long) ( rdr->anim_time * 10.0f ) % model.state.nframe ) : 0;
        n = mdl_draw_tris( world, model.state.ntri, model.state.nvert, frame, &at,
                            cyaw, syaw, cpitch, spitch, croll, sroll,
                            &model.state.scale, &model.state.origin,
                            &model.state, skin, m, xresh, yresh,
                            z_near, dst, QGL_Z_TEST );
        drawn = (short) ( drawn + n );
        rdr->projectile_mdl_drawn = (short) ( rdr->projectile_mdl_drawn + n );
    }
    return drawn;
}

short d_draw_beams( World *world, Renderer *rdr, DiskPlane far *frustum,
                     Mat4 *mtx_fin, float xresh, float yresh,
                     float z_near, QSurf dst )
{
    float *m = (float *) mtx_fin;
    short i, seg, live = 0, drawn = 0, segments = 0;

    rdr->beam_drawn = rdr->beam_segments = 0;
    if ( rdr->beam_live <= 0 ) return 0;

    for ( i = 0; i < BEAM_MAX; i++ ) {
        Beam far *pool;
        Beam b;
        EffectModel model;
        BspVec3 dist, at;
        float len, left, h, cyaw, syaw, cpitch, spitch, roll, zrad, radius;
        QSurf skin;

        /* Alias geometry and its skin remap both EMS slots. Re-acquire the
           state row before every small record. */
        pool = d_beam_map( rdr, PAGE_SLOT );
        if ( !pool ) break;
        _fmemcpy( &b, &pool[i], sizeof(b) );
        if ( !b.key ) continue;
        if ( b.end_time < rdr->anim_time ) {
            b.key = 0;
            _fmemcpy( &pool[i], &b, sizeof(b) );
            continue;
        }
        live++;
        if ( b.model < 0 || b.model >= world->fx_mdl_count ) continue;
        _fmemcpy( &model, &world->fx_mdl[b.model], sizeof(model) );
        if ( !model.state.loaded ) continue;
        skin = mdl_effect_skin( world, b.model );
        if ( !skin ) continue;

        dist.x = b.end.x - b.start.x;
        dist.y = b.end.y - b.start.y;
        dist.z = b.end.z - b.start.z;
        len = (float) sqrt( dist.x*dist.x + dist.y*dist.y + dist.z*dist.z );
        if ( len < 0.001f ) continue;
        dist.x /= len; dist.y /= len; dist.z /= len;
        h = (float) sqrt( dist.x*dist.x + dist.y*dist.y );
        cyaw = h > 0.001f ? dist.x / h : 1.0f;
        syaw = h > 0.001f ? dist.y / h : 0.0f;
        cpitch = h;
        spitch = -dist.z;
        at = b.start;
        left = len;
        zrad = (float) fabs( model.state.zlo );
        if ( (float) fabs( model.state.zhi ) > zrad )
            zrad = (float) fabs( model.state.zhi );
        radius = (float) sqrt( model.state.radius * model.state.radius
                             + zrad * zrad );
        seg = 0;

        /* CL_UpdateTEnts advances by 30 regardless of the model's length.
           The final segment is deliberately not trimmed. */
        while ( left > 0.0f ) {
            unsigned long seed = (unsigned long) ( rdr->anim_time * 1000.0f );
            short n;

            seed ^= (unsigned long) b.key * 25173UL + (unsigned long) seg * 13849UL;
            roll = (float) ( seed % 360UL ) * 0.017453293f;
            n = 0;
            if ( d_mdl_sphere_visible( frustum, &at, radius ) )
                n = mdl_draw_tris( world, model.state.ntri, model.state.nvert, 0, &at,
                                    cyaw, syaw, cpitch, spitch,
                                    (float) cos(roll), (float) sin(roll),
                                    &model.state.scale, &model.state.origin,
                                    &model.state, skin, m, xresh, yresh,
                                    z_near, dst, QGL_Z_TEST );
            drawn = (short) ( drawn + n );
            if ( b.model == MDL_FX_BOLT ) rdr->beam_sham_tris += n;
            if ( b.model == MDL_FX_BOLT3 ) rdr->beam_boss_tris += n;
            at.x += dist.x * 30.0f;
            at.y += dist.y * 30.0f;
            at.z += dist.z * 30.0f;
            left -= 30.0f;
            seg++;
            segments++;
        }
    }
    rdr->beam_live = live;
    rdr->beam_drawn = drawn;
    rdr->beam_segments = segments;
    return drawn;
}
