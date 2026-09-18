#!/usr/bin/env python3
"""mkmdl.py -- emit one Quake alias model as loadable geometry + a skin.

The counterpart to mkspr.py: instead of pre-rendering the model to sprites,
this ships the triangles so the renderer can transform them per frame. It
exists to make the comparison measurable -- same monster, same map, sprites
against real geometry.

    tools/mkmdl.py PAK0.PAK soldier out/

Emits (DOS 8.3 names, since the loader opens them by name):

    <name>.geo   header (incl. a per-model vertex scale/origin, fitted to
                 the frames, and up to eight frame sets' counts in the
                 order given, which is the order the runtime indexes),
                 the frames actually kept), triangles with per-corner UV
                 as fixed-point Integers, then one vertex array per
                 animation frame as raw BYTES -- the same trivertx_t
                 compression the .mdl file itself already uses.
    <name>.vtx   one vertex array per frame as raw BYTES, no header
    <name>.skn   the skin: sw*sh palette indices, top-down, no header
    <name>.itm   bundled alias model: .geo, .vtx, then every skin in order

UVs are normalised 0..1 -- uGL's own convention, and the patched library
scales by xRes rather than xRes-1 (see ugl-patch/README.md). The `onseam`
flag is resolved here, not on the target: a seam vertex used by a back-
facing triangle takes the +skinwidth/2 column, which is a property of the
triangle, so the u belongs to the CORNER and not to the vertex.
"""

from __future__ import annotations

import os
import struct
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import mdlview as mdl  # noqa: E402
from mdlsets import MDL_NSETS, parse_sets, select_indices, set_indices  # noqa: E402


def resample_skin(skin) -> tuple[int, int, np.ndarray]:
    # POWER OF TWO, both axes. The textured fillers mask u and v with
    # (size-1), so a 300x194 skin has no mask that works and every
    # textured polygon rasterises nothing -- silently, with no error from
    # the library. The world textures already obey this: mkassets.py
    # resamples every one into 64/32/16/8 cells for exactly this reason.
    #
    # It must also fit one scanline (<= 8192) and stay <= 16,384 bytes
    # (one EMS page), which is qglRsPoly's own limit on a texture: the
    # texel base is a patched immediate and no filler remaps mid-polygon.
    # Within that, pick the POT pair closest to the skin's own aspect
    # ratio, largest area first among equally-close ones -- an even
    # POT/POT search, not a guess, since which pair wins depends on the
    # source image's shape.
    MAX_BYTES = 16384
    target_ratio = skin.width / skin.height
    best = None
    pots = [2**k for k in range(0, 14) if 2**k <= 8192]  # 1..8192
    for w in pots:
        for h in pots:
            if w * h > MAX_BYTES or w > 8192:
                continue
            dist = abs((w / h) - target_ratio)
            score = (dist, -(w * h))  # closest ratio, then largest area
            if best is None or score < best[0]:
                best = (score, w, h)
    sw, sh = best[1], best[2]
    yi = (np.arange(sh) * skin.height // sh).clip(0, skin.height - 1)
    xi = (np.arange(sw) * skin.width // sw).clip(0, skin.width - 1)
    return sw, sh, skin.pixels[yi][:, xi]  # nearest neighbour: stays in palette indices


def kept(m: mdl.Model, name: str, n: int) -> list:
    return [m.frames[i] for i in set_indices([f.name for f in m.frames], name, n)]


def select_frames(m: mdl.Model, sets: list[tuple[str, int]]) -> list:
    spec = ",".join(name + (f":{n}" if n else "") for name, n in sets)
    indices, _counts = select_indices([f.name for f in m.frames], spec)
    if not indices:
        raise ValueError("model selection contains no frames")
    return [m.frames[i] for i in indices]


def quantise(frames: list) -> tuple[np.ndarray, np.ndarray, bytes]:
    # Vertices as BYTES, one per axis -- trivertx_t's own compression --
    # against one scale and origin fitted to every kept frame.
    allv = np.concatenate([f.verts for f in frames], axis=0)  # [n*frames, 3]
    vmin = allv.min(axis=0)
    vmax = allv.max(axis=0)
    vscale = np.where(vmax > vmin, (vmax - vmin) / 255.0, 1.0)
    vtx = b"".join(np.clip(np.rint((f.verts - vmin) / vscale), 0, 255).astype(np.uint8).tobytes() for f in frames)
    return vmin, vscale, vtx


def vertex_bytes(pak: str, name: str, spec: str) -> bytes:
    """The .vtx stream mkmdl writes for name and spec, straight from the PAK."""
    blob, entries = mdl.read_pak(pak)
    m = mdl.parse_mdl(mdl.pak_read(blob, entries, f"progs/{name}.mdl"), name)
    return quantise(select_frames(m, parse_sets(spec)))[2]


def main() -> int:
    if len(sys.argv) not in (4, 5):
        print(__doc__)
        return 1
    pak, name, outdir = sys.argv[1:4]
    sets = parse_sets(sys.argv[4] if len(sys.argv) == 5 else None)
    os.makedirs(outdir, exist_ok=True)

    blob, entries = mdl.read_pak(pak)
    m = mdl.parse_mdl(mdl.pak_read(blob, entries, f"progs/{name}.mdl"), name)
    uv = mdl.build_uv(m)                     # [tri][corner][u, v], normalised
    skin = m.skins[0]

    frames = select_frames(m, sets)

    vmin, vscale, vtx = quantise(frames)

    UV_SCALE = 32767   # fixed-point Integer UV, 0..32767 = 0.0..1.0

    skins = [resample_skin(s) for s in m.skins]
    sw, sh, res = skins[0]
    if any((w, h) != (sw, sh) for w, h, _ in skins):
        raise SystemExit(f"{name}: skins resample to different sizes")

    # sw/sh, NOT skin.width/skin.height: the loader creates a Surface of
    # exactly this size and the source image's own dimensions would be a
    # fact it has no use for. They were written here before anything read
    # them, which is how they stayed wrong for free.
    counts = [len(kept(m, s, n)) for s, n in sets]
    if (len(counts) > MDL_NSETS or max(counts) > 31 or len(frames) > 255
            or sum(counts) != len(frames)):
        raise SystemExit(f"{name}: sets {counts} for {len(frames)} frames: the header holds {MDL_NSETS}"
                         " counts of up to 31 that add up to the frames")

    # QMD4 expands the semantic animation slots from eight to sixteen.
    out = bytearray(struct.pack("<4sHHHHH", b"QMD4", len(m.tris), len(m.st),
                                len(frames), sw, sh))
    out += struct.pack("<6f", *vscale.tolist(), *vmin.tolist())
    out += struct.pack("<H16B", len(counts), *(counts + [0] * MDL_NSETS)[:MDL_NSETS])
    # Frames one 16K EMS page holds, and the pages they take: the loader
    # maps page frame // page_frames, so no frame straddles two.
    page_frames = 16384 // (len(m.st) * 3)
    vpages = -(-len(frames) // page_frames)
    out += struct.pack("<2H", vpages, page_frames)
    for t, (_ff, a, b, c) in enumerate(m.tris):
        out += struct.pack("<3h", a, b, c)
        for k in range(3):
            uf = float(np.clip(uv[t, k, 0], 0.0, 1.0))
            vf = float(np.clip(uv[t, k, 1], 0.0, 1.0))
            out += struct.pack("<2h", round(uf * UV_SCALE), round(vf * UV_SCALE))

    geo = os.path.join(outdir, f"{name[:8]}.geo")
    open(geo, "wb").write(bytes(out))

    # Flat, no header -- cnt*3 bytes, frame-major -- read whole with a
    # single uarReadH straight into a memAlloc'd conventional-memory
    # block (model.bas's own pvs.bin does the identical thing), then
    # PEEK'd. Conventional memory, not EMS: the earlier EMS-bitmap design
    # worked, but this is simpler and this data is small enough (8,160
    # bytes for the soldier's stand+run set) to just live in DOS memory
    # like the PVS lump already does.
    vtxpath = os.path.join(outdir, f"{name[:8]}.vtx")
    open(vtxpath, "wb").write(vtx)

    # Raw palette indices, top-down, no header: the .geo already carries
    # sw and sh, and a second copy in a BMP header is a second place for
    # them to be wrong. qgl has no BMP reader and does not want one --
    # the world atlas ships as texr.raw/texs.raw for the same reason.
    skn = os.path.join(outdir, f"{name[:8]}.skn")
    open(skn, "wb").write(res.tobytes())

    for i, (_w, _h, pixels) in enumerate(skins[1:], 1):
        open(os.path.join(outdir, f"{name[:8]}{i}.skn"), "wb").write(pixels.tobytes())

    # Small alias models use one stream: header/triangles, vertices, then
    # every skin. The regular monster loader keeps the split files.
    open(os.path.join(outdir, f"{name[:8]}.itm"), "wb").write(
        bytes(out) + vtx + b"".join(pixels.tobytes() for _w, _h, pixels in skins)
    )

    print(f"{name}: {len(m.tris)} tris, {len(m.st)} verts, {len(frames)} frames")
    print(f"  {os.path.basename(geo):<14} {len(out):,} B  (header + tris {len(m.tris)*18:,})")
    print(f"  {os.path.basename(vtxpath):<14} {len(vtx):,} B  "
          f"(flat, memAlloc'd conventional memory -- see mdl_load in d_mdl.bas)")
    print(f"  {os.path.basename(skn):<14} {skin.width}x{skin.height} -> {sw}x{sh} "
          f"= {sw*sh:,} B (power of two, both axes)")
    for i, f in enumerate(frames[:4]):
        print(f"    frame {i}: {f.name}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
