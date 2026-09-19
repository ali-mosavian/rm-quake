#!/usr/bin/env python3
"""
mkassets.py -- turn a Quake .bsp's texture lump into ready-to-blit 8-bit BMPs.

texLoadAll does this work at every launch, in BASIC: it reads the miptex data
a byte at a time, expands palette indices to RGB, bilinearly resamples each
mip to a fixed size, and then searches all 256 palette entries per output texel
to get back to an index. For dm3ish that is 150,960 single-byte file reads,
108,800 output texels and 27,852,800 palette-search iterations.

All of it is a pure function of the .bsp and the palette, so none of it needs
to happen on the target. This emits one atlas BMP per mip level, already in the
game palette, which uGL's own assembly BMP loader can pull in directly.
"""
import hashlib
import struct, sys, os, math
import re
import zlib
from collections.abc import Callable
from dataclasses import dataclass
from dataclasses import field

import mksnd
import mksprite
import lighthash
import texquant
from mdlsets import MDL_SETS

# every output lands here and becomes one assets.zip member
OUT: dict[str, bytes] = {}


def write_zip(path: str) -> None:
    # STORED, every member. The reader is src/qgl/zip.asm, which walks
    # local headers and refuses any method but 0 -- there is no inflate in
    # the renderer any more, and a member it could not read would surface
    # as a missing file rather than as wrong bytes. Costs ~330K on disk;
    # nothing in the build or the run is measured in disk.
    locs, cens, pos = [], [], 0
    for name, data in OUT.items():
        payload, method, xtra = data, 0, b""
        crc = zlib.crc32(data)
        nm = name.encode()
        locs.append(struct.pack("<4sHHHHHLLLHH", b"PK\x03\x04", 20, 0, method,
                                0, 0x21, crc, len(payload), len(data), len(nm),
                                len(xtra)) + nm + xtra + payload)
        cens.append(struct.pack("<4sHHHHHHLLLHHHHHLL", b"PK\x01\x02", 20, 20, 0,
                                method, 0, 0x21, crc, len(payload), len(data),
                                len(nm), 0, 0, 0, 0, 0, pos) + nm)
        pos += len(locs[-1])
    cd = b"".join(cens)
    open(path, 'wb').write(b"".join(locs) + cd + struct.pack(
        "<4sHHHHLLH", b"PK\x05\x06", 0, 0, len(OUT), len(OUT), len(cd), pos, 0))
    # round trip before trusting the writer
    import zipfile
    with zipfile.ZipFile(path) as z:
        for name, data in OUT.items():
            assert z.read(name) == data, name
            assert z.getinfo(name).compress_type == zipfile.ZIP_STORED, \
                f"{name} is not stored; qgl cannot inflate it"
    raw = sum(len(v) for v in OUT.values())
    print(f"  assets.zip: {len(OUT)} members, {raw:,} -> {os.path.getsize(path):,} bytes")

# ---------------------------------------------------------------------
# The .qmp container: one file per converted map.
#
# A map used to be FIVE files staged together -- the .bsp itself (read
# for its lump counts and its miptex names), assets.zip, texr.raw,
# texs.raw and pal.raw -- and nothing tied them to each other. e1m1's
# zip over dm3ish's atlases drew every texture as some other one and
# ran perfectly happily; the offset table indexes whatever atlas is
# there. One file cannot be half of another map.
#
# It is a flat directory, not a zip: the members are the same STORED
# bytes, but the reader (src/qgl/file.asm) seeks once to a table
# instead of walking seventeen local headers, and there is no method
# field to get wrong. Everything is little-endian.
#
#     0   "QMAP"              magic
#     4   version    long     QMAP_VER
#     8   ndir       short    directory entries
#    10   dirofs     long     where the directory starts
#    14   bsp_sum    long     the source .bsp's checksum: identity only
#    18   name       16 bytes the map's own name, NUL padded
#    34   pad to QMAP_HEAD
#    64   directory: ndir x { char name[16]; long ofs; long size; }
#   ...   member data, each aligned to 16
# q_pl.bi's PL_IT_* order for the weapons that have one: shotgun,
# super shotgun, nailgun, grenade launcher, super nailgun, rocket
# launcher, axe, Thunderbolt.
VIEW_MDLS = ('v_shot', 'v_shot2', 'v_nail', 'v_rock', 'v_nail2', 'v_rock2', 'v_axe', 'v_light')
VIEW_MODEL_SET = {'v_axe': 'frame'}

# Chthon's model, every set in mdltypes.h's MDL_BOSS_* order, for a map
# with a monster_boss.
BOSS_MDL = ('boss', 'rise,walk,death,attack,shocka,shockb,shockc')

ITEM_MODELS = {
    2: "armor", 3: "armor", 4: "g_shot", 6: "g_nail",
    7: "quaddama", 8: "suit", 12: "g_rock", 14: "g_nail2",
    15: "g_rock2", 16: "invulner", 18: "armor", 20: "invisibl", 21: "g_light",
}
KEY_MODELS = (
    ("w_s_key", "w_g_key"),
    ("m_s_key", "m_g_key"),
    ("b_s_key", "b_g_key"),
)
EFFECT_MODELS = (
    *((name, "*") for name in (
        "spike", "s_spike", "grenade", "missile", "lavaball", "w_spike", "zom_gib",
        "gib1", "gib2", "gib3", "h_player", "h_guard", "h_knight", "h_dog",
        "h_ogre", "h_demon", "h_zombie", "h_wizard", "h_shams", "bolt", "bolt3", "bolt2",
    )),
    ("flame", "flame:6"),
    ("flame2", "flame:6,flameb:11"),
)
EFFECT_PAGE = 16384
EFFECT_SKIN_W = 256
MDL_HEAD_FMT = "<4s5h6fh16B2h"
MDL_HEAD_SIZE = struct.calcsize(MDL_HEAD_FMT)


def build_sbar(pak: str, tmp: str) -> dict[str, bytes]:
    """The packed single-player status and menu atlases from gfx.wad."""
    import subprocess

    if not pak or not os.path.exists(pak):
        # The runtime is fatal on a missing sbar.raw, by name -- say so
        # here too, where the fix is.
        print("  sbar: no PAK, so no status bar in the container")
        return {}
    os.makedirs(tmp, exist_ok=True)
    r = subprocess.run([sys.executable, os.path.join(os.path.dirname(__file__), "mkgfx.py"), pak, tmp],
                       capture_output=True, text=True)
    if r.returncode != 0:
        raise SystemExit(f"mkgfx: {r.stderr.strip() or r.stdout.strip()}")
    return {name: open(os.path.join(tmp, name), "rb").read()
            for name in ("sbar.raw", "menu.raw")}


def build_sounds(pak: str, tmp: str) -> dict[str, bytes]:
    """The bsc4/32n stream, its decode table and the id table, as container members."""
    import subprocess

    if not pak or not os.path.exists(pak):
        print("  sound: no PAK, so no sounds in the container")
        return {}
    os.makedirs(tmp, exist_ok=True)
    r = subprocess.run([sys.executable, os.path.join(os.path.dirname(__file__), "mksnd.py"), pak, tmp],
                       capture_output=True, text=True)
    if r.returncode != 0:
        raise SystemExit(f"mksnd: {r.stderr.strip() or r.stdout.strip()}")
    print(r.stdout, end="")
    return {n: open(os.path.join(tmp, n), "rb").read() for n in ("snd.bsc", "snddec.raw", "sndtab.raw")}


def build_models(pak: str, kinds: set[int], boss: bool, tmp: str) -> dict[str, bytes]:
    """<name>.geo/.vtx/.skn for each kind, and Chthon's for a map with him, as container members."""
    import subprocess

    out: dict[str, bytes] = {}
    if not pak or not os.path.exists(pak):
        return out
    os.makedirs(tmp, exist_ok=True)
    for name, sets in [MDL_SETS[k] for k in sorted(kinds) if k in MDL_SETS] + ([BOSS_MDL] if boss else []):
        r = subprocess.run([sys.executable, os.path.join(os.path.dirname(__file__), 'mkmdl.py'),
                            pak, name, tmp, sets], capture_output=True, text=True)
        if r.returncode != 0:
            raise SystemExit(f"mkmdl {name}: {r.stderr.strip() or r.stdout.strip()}")
        # mkmdl truncates the .geo's name to 8 characters and not the
        # other two; the member keeps the model's own name either way.
        for ext, stem in (('geo', name[:8]), ('vtx', name), ('skn', name)):
            out[f'{name}.{ext}'] = open(os.path.join(tmp, f'{stem}.{ext}'), 'rb').read()
    print(f"  models: {len(out) // 3} of the map's own monster kinds")

    # The view weapons. Not the map's -- a weapon can be picked up on
    # any of them -- so all seven ship and the runtime loads the one in
    # hand. mkmdl's 'shot' set is v_*.mdl's whole frame list.
    for name in VIEW_MDLS:
        r = subprocess.run([sys.executable, os.path.join(os.path.dirname(__file__), 'mkmdl.py'),
                            pak, name, tmp, VIEW_MODEL_SET.get(name, 'shot')], capture_output=True, text=True)
        if r.returncode != 0:
            raise SystemExit(f"mkmdl {name}: {r.stderr.strip() or r.stdout.strip()}")
        for ext, stem in (('geo', name[:8]), ('vtx', name), ('skn', name)):
            out[f'{name}.{ext}'] = open(os.path.join(tmp, f'{stem}.{ext}'), 'rb').read()
    print(f"  view models: {len(VIEW_MDLS)}")
    return out


def build_bundled_model(pak: str, name: str, tmp: str, spec: str = "*") -> bytes:
    import subprocess

    os.makedirs(tmp, exist_ok=True)
    r = subprocess.run([sys.executable, os.path.join(os.path.dirname(__file__), "mkmdl.py"),
                        pak, name, tmp, spec], capture_output=True, text=True)
    if r.returncode != 0:
        raise SystemExit(f"mkmdl {name}: {r.stderr.strip() or r.stdout.strip()}")
    return open(os.path.join(tmp, f"{name[:8]}.itm"), "rb").read()


def build_item_models(pak: str, names: set[str], tmp: str) -> dict[str, bytes]:
    if not pak or not os.path.exists(pak):
        return {}
    out = {f"{name}.itm": build_bundled_model(pak, name, tmp) for name in sorted(names)}
    print(f"  pickup models: {len(out)}")
    return out


def build_effect_models(pak: str, tmp: str) -> dict[str, bytes]:
    if not pak or not os.path.exists(pak):
        return {}
    models = [(name, build_bundled_model(pak, name, tmp, spec)) for name, spec in EFFECT_MODELS]
    geom = bytearray()
    skins = bytearray()
    records: list[tuple[str, bytes, int, int, int]] = []

    def page_place(payload: bytes) -> int:
        used = len(geom) % EFFECT_PAGE
        if used + len(payload) > EFFECT_PAGE:
            geom.extend(bytes(EFFECT_PAGE - used))
        offset = len(geom)
        geom.extend(payload)
        return offset

    def skin_place(payload: bytes) -> int:
        used = len(skins) % EFFECT_PAGE
        if used + len(payload) > EFFECT_PAGE:
            skins.extend(bytes(EFFECT_PAGE - used))
        offset = len(skins)
        skins.extend(payload)
        return offset

    for name, model in models:
        if len(model) < MDL_HEAD_SIZE:
            raise SystemExit(f"{name}: bundled model has no complete header")
        head = model[:MDL_HEAD_SIZE]
        magic, ntri, nvert, nframe, sw, sh, *_ = struct.unpack(MDL_HEAD_FMT, head)
        tri_n = ntri * 18
        vtx_n = nvert * nframe * 3
        skin_n = sw * sh
        if magic != b"QMD4" or MDL_HEAD_SIZE + tri_n + vtx_n + skin_n != len(model):
            raise SystemExit(f"{name}: bundled model layout is not one header, geometry and skin")
        if tri_n > EFFECT_PAGE or vtx_n > EFFECT_PAGE:
            raise SystemExit(f"{name}: effect geometry crosses an EMS page")
        tri = model[MDL_HEAD_SIZE:MDL_HEAD_SIZE + tri_n]
        vtx = model[MDL_HEAD_SIZE + tri_n:MDL_HEAD_SIZE + tri_n + vtx_n]
        vtx_ofs = page_place(vtx)
        tri_ofs = page_place(tri)
        skin_ofs = skin_place(model[-skin_n:])
        records.append((name[:8], head, vtx_ofs, tri_ofs, skin_ofs))

    if len(geom) > 65535:
        raise SystemExit(f"effect geometry is {len(geom)} bytes; its offsets are 16-bit")
    skin_h = -(-len(skins) // EFFECT_SKIN_W)
    skins.extend(bytes(skin_h * EFFECT_SKIN_W - len(skins)))
    data = bytearray(struct.pack("<4sHHHI", b"QFX3", len(records), EFFECT_SKIN_W,
                                 skin_h, len(geom)))
    for name, head, vtx_ofs, tri_ofs, skin_ofs in records:
        data += struct.pack("<8s", name.encode()) + head
        data += struct.pack("<HHI", vtx_ofs, tri_ofs, skin_ofs)
    data += geom + skins
    print(f"  effect models: {len(models)}, {len(geom)} B geometry, {len(skins)} B skin atlas")
    return {"effects.bin": bytes(data)}


def item_models(ents: list[bytes], monster_kinds: set[int]) -> set[str]:
    names: set[str] = set()
    prefix = struct.calcsize("<3ff3fff")
    item_size = struct.calcsize("<hhhhh3f")
    for data in ents:
        counts = struct.unpack_from("<13h", data, prefix)
        ntele, nplat, nhide, nitem, nmon, worldtype = (
            counts[1], counts[2], counts[3], counts[4], counts[7], counts[11]
        )
        ngate = struct.unpack_from("<h", data, struct.calcsize(ENTS_HEAD) - 2)[0]
        ofs = (struct.calcsize(ENTS_HEAD) + nmon * struct.calcsize(ENTS_MON)
               + ntele * struct.calcsize("<h3ffhh") + nplat * struct.calcsize("<hffhh")
               + nhide * 2 + ngate * struct.calcsize("<hhh"))
        for i in range(nitem):
            kind, amount, _target, crate, _order = struct.unpack_from("<hhhhh", data, ofs + i * item_size)
            if crate >= 0 or kind == 9:
                continue
            if kind in (10, 11):
                names.add(KEY_MODELS[worldtype][kind - 10])
            elif kind == 17:
                bit = next((b for b in range(4) if amount & (1 << b)), 0)
                names.add(f"end{bit + 1}")
            elif kind in ITEM_MODELS:
                names.add(ITEM_MODELS[kind])
    if monster_kinds & {0, 3}:
        names.add("backpack")
    return names


SKILLS     = 4   # easy, normal, hard, nightmare: an ents<N>.bin each
ENTS_HEAD  = '<4f3fff13hfhhh3ffh'  # ent.h's EntsHead; the monsters follow it
ENTS_MON   = '<hh3ffhh'          # ent.h's EntsMon

QMAP_VER   = 9
QMAP_HEAD  = 64
QMAP_NAME  = 16
QMAP_ENT   = 24
QMAP_ALIGN = 16


def write_qmap(path: str, name: str, bsp: bytes, members: dict[str, bytes]) -> None:
    # src/host/pak.h's QMAP_MAX: the runtime reads the directory into
    # a fixed array and is fatal past it, which is a dead run rather
    # than a bad build unless this says so here.
    if len(members) > 80:
        raise SystemExit(f"{len(members)} members: cport's QMAP_MAX is 80")
    for m in members:
        if len(m.encode()) >= QMAP_NAME:
            raise SystemExit(f"member name {m!r} does not fit {QMAP_NAME - 1} characters")

    dirofs = QMAP_HEAD
    ofs    = dirofs + QMAP_ENT * len(members)
    ofs   += -ofs % QMAP_ALIGN
    dirs, blob = bytearray(), bytearray()
    for mname, data in members.items():
        dirs += struct.pack(f"<{QMAP_NAME}sll", mname.encode(), ofs, len(data))
        pad   = -len(data) % QMAP_ALIGN
        blob += data + bytes(pad)
        ofs  += len(data) + pad

    head = struct.pack(f"<4slHlL{QMAP_NAME}s", b"QMAP", QMAP_VER, len(members),
                       dirofs, zlib.crc32(bsp) & 0xffffffff, name.encode())
    head += bytes(QMAP_HEAD - len(head))
    body  = head + bytes(dirs)
    body += bytes(-len(body) % QMAP_ALIGN)
    open(path, "wb").write(bytes(body) + bytes(blob))

    # Round trip before trusting the writer: read every member back the
    # way the driver will, by the directory alone.
    d = open(path, "rb").read()
    magic, ver, n, do, _, nm = struct.unpack_from(f"<4slHlL{QMAP_NAME}s", d, 0)
    assert magic == b"QMAP" and ver == QMAP_VER and n == len(members)
    assert nm.split(b"\0")[0].decode() == name
    for k in range(n):
        mn, mo, ms = struct.unpack_from(f"<{QMAP_NAME}sll", d, do + k * QMAP_ENT)
        mn = mn.split(b"\0")[0].decode()
        assert d[mo:mo + ms] == members[mn], mn
    print(f"  {os.path.basename(path)}: {len(members)} members, "
          f"{sum(len(v) for v in members.values()):,} -> {len(d):,} bytes")


# The geometry store's row width, and the corner count a face record may
# carry. GEOM_MAXVTX must match q_map.bi's, and d_faces.c sizes its own
# vertex arrays as GEOM_MAXVTX + 8 -- the clipper's headroom. GEOM_W must
# match GEOM_W in q_map.bi: it is
# the unit uglMapEx maps, and a record that straddled it would be read
# half from the wrong EMS page.
GEOM_W = 8192
GEOM_MAXVTX = 33

MIPS   = 4
EMS_PAGE = 16384                # one window: the most a cell may ever be
MIN_CELL = 64                   # texels; under this a level shares the one above
OFS_BITS = 23                   # of the table entry; the top nine carry the dims


def _p2_down(v):
    return v if v and (v & (v - 1)) == 0 else 1 << (v.bit_length() - 1)


def tex_cell_levels(w, h):
    """Atlas cell (width, height) per mip level, at the texture's own aspect.

    Quake sizes are multiples of 16, not powers of two, and the renderer
    addresses a cell by masking -- so each side is rounded DOWN to a power
    of two, and the pair down again until it fits one EMS window. 79 of
    e1m1's 81 textures come through untouched; 256x128 and 128x192 lose one
    halving each.

    A level whose cell would be under MIN_CELL texels repeats the one above
    instead: 2x2 is a scanline table for nothing, and the saving is 440
    bytes of e1m1's 508K, so this is about degenerate views and not memory.
    """
    W, H = _p2_down(w), _p2_down(h)
    while W * H > EMS_PAGE:
        if W >= H: W //= 2
        else:      H //= 2
    out = []
    for k in range(MIPS):
        cw, ch = max(W >> k, 1), max(H >> k, 1)
        out.append(out[-1] if out and cw * ch < MIN_CELL else (cw, ch))
    return out

def read_lumps(d):
    return [struct.unpack_from('<ii', d, 4 + 8*i) for i in range(15)]

def pack_read(path, want):
    """Pull one file out of a Quake PACK archive (base.dat is one)."""
    d = open(path, 'rb').read()
    magic, dirofs, dirlen = struct.unpack_from('<4sii', d, 0)
    if magic != b'PACK':
        raise SystemExit(f"{path} is not a PACK archive")
    for i in range(dirlen // 64):
        e    = dirofs + 64*i
        name = d[e:e+56].split(b'\0')[0].decode('latin-1')
        off, ln = struct.unpack_from('<ii', d, e+56)
        if name == want:
            return d[off:off+ln]
    raise SystemExit(f"{want} not found in {path}")

def load_palette(raw):
    if len(raw) < 768:
        raise SystemExit(f"palette is {len(raw)} bytes, expected >= 768")
    return [tuple(raw[i*3:i*3+3]) for i in range(256)]


def srgb_linear(s: float) -> float:
    s = s / 255
    return s / 12.92 if s <= 0.04045 else ((s + 0.055) / 1.055) ** 2.4


def oklab(rgb: tuple[float, float, float]) -> tuple[float, float, float]:
    r, g, b = (srgb_linear(c) for c in rgb)
    l = math.cbrt(0.4122214708 * r + 0.5363325363 * g + 0.0514459929 * b)
    m = math.cbrt(0.2119034982 * r + 0.6806995451 * g + 0.1073969566 * b)
    s = math.cbrt(0.0883024619 * r + 0.2817188376 * g + 0.6299787005 * b)
    return (0.2104542553 * l + 0.7936177850 * m - 0.0040720468 * s,
            1.9779984951 * l - 2.4285922050 * m + 0.4505937099 * s,
            0.0259040371 * l + 0.7827717662 * m - 0.8086757660 * s)


def oklch_colormap(palette: bytes) -> bytes:
    """64 rows of 256: row r shades index i by 2 - 2r/63 in OKLab lightness,
    unclipped, chroma scaled with it and hue kept, matched to i's own
    ramp of 16 or black by dL^2 + dC^2 + 4 dH^2. Fullbrights stay. The
    shade is of the colour the DAC shows, 6 bits a channel."""
    base = [tuple((palette[i * 3 + c] >> 2) * 255 / 63 for c in range(3)) for i in range(256)]
    ok = [oklab(c) for c in base]
    table = bytearray(64 * 256)
    for r in range(64):
        scale = 2 - 2 * r / 63
        for i in range(256):
            if i >= 224:
                table[r * 256 + i] = i
                continue
            L = oklab(tuple(c * scale for c in base[i]))[0]
            ratio = L / max(ok[i][0], 1e-9)
            ta, tb = ok[i][1] * ratio, ok[i][2] * ratio
            tc = math.hypot(ta, tb)
            best, low = 0, math.inf
            for j in [0] + [j for j in range(i // 16 * 16, i // 16 * 16 + 16) if j != 0]:
                dl = L - ok[j][0]
                dc = tc - math.hypot(ok[j][1], ok[j][2])
                dh2 = max((ta - ok[j][1]) ** 2 + (tb - ok[j][2]) ** 2 - dc * dc, 0)
                if (d := dl * dl + dc * dc + 4 * dh2) < low:
                    best, low = j, d
            table[r * 256 + i] = best
    return bytes(table)


def sky_layers(cells: list, shade0: bytes, pal: list[tuple[int, int, int]]) -> dict[str, bytes]:
    """R_InitSky, offline: a 256x128 sky texture's right half is the back
    layer and its left half the front, index 0 clear. 128 rows of each,
    shaded by row 0 -- which takes index 48 to 0 as well, so an opaque one
    becomes the nearest other colour."""
    dark = min(range(1, 256), key=lambda i: sum((a - b) ** 2 for a, b in zip(pal[i], pal[0])))
    for cl in cells:
        if cl is None:
            continue
        buf, base = cl
        w, h, mo = struct.unpack_from('<iii', buf, base + 16)
        if not buf[base:base + 3].lower() == b'sky' or (w, h) != (256, 128):
            continue
        src = buf[base + mo:base + mo + w * h]
        back = bytes(shade0[src[y * 256 + 128 + x]] for y in range(128) for x in range(128))
        front = bytes((shade0[p] or dark) if p else 0 for p in (src[y * 256 + x] for y in range(128) for x in range(128)))
        return {'sky.raw': back + front}
    return {}


def write_bmp8(path, w, h, pixels, pal):
    OUT[os.path.basename(path)] = bmp8_bytes(w, h, pixels, pal)

def bmp8_bytes(w, h, pixels, pal):
    """8-bit uncompressed BMP, bottom-up, rows padded to 4 bytes."""
    stride = (w + 3) & ~3
    pad    = stride - w
    px     = bytearray()
    for y in range(h-1, -1, -1):                 # BMP rows run bottom to top
        px += pixels[y*w:(y+1)*w] + bytes(pad)
    palb = bytearray()
    for (r, g, b) in pal:
        palb += bytes((b, g, r, 0))              # BMP palette is BGRA
    off  = 14 + 40 + 1024
    hdr  = b'BM' + struct.pack('<IHHI', off + len(px), 0, 0, off)
    info = struct.pack('<IiiHHIIiiII', 40, w, h, 1, 8, 0, len(px), 2835, 2835, 256, 256)
    return hdr + info + palb + px

UA_WIN = 16384          # uglArr's page size (UA_WIN in src/ugl/uglarr.asm)

# Element sizes for the paged lumps, so the padding lands where uGL puts it.
PAGED_ELEM = {
    'nodes.pag': 16,    # Node: planeid child0 child1 lfaceid lfacenum + bound[6]
    'clip.pag':   6,    # ClipNode: planenum front back
    'leaves.pag':16,    # Leaf: cont vislist bound[6] lfaceid lfacenum
    'faces.pag': 8,     # Face: three bounded indices with packed cache head, geom_row, side
}


# A node or leaf box in six bytes, (v + 4096) / 32 with the min rounded
# down and the max up, so a box only ever grows: r_cull_box_c culls less
# than it might, never more. bspfile.bi's PackedBounds, r_walk.c's unpack.
BOUND_Q = 32
BOUND_BASE = -4096


def bound_bytes(bound: bytes) -> bytes:
    mn = struct.unpack_from('<3h', bound, 0)
    mx = struct.unpack_from('<3h', bound, 6)
    lo = [(v - BOUND_BASE) // BOUND_Q for v in mn]
    hi = [-((BOUND_BASE - v) // BOUND_Q) for v in mx]
    if not all(0 <= q <= 255 for q in lo + hi):
        raise SystemExit(f'a box past the packed range: {mn} {mx}')
    return bytes(lo + hi)


# Arrays the renderer backs with CONVENTIONAL memory are laid out FLAT --
# no page padding. uglArr maps such a store once, whole, and the renderer
# then indexes arr(i) natively, so element i must be at exactly i*elem.
# Page padding would break that at every page boundary.
#
# EMS-backed arrays still need padding: a 16k frame really is a window.
FLAT = {'nodes.pag', 'clip.pag', 'leaves.pag', 'faces.pag'}


def write_flat(path, payload):
    OUT[os.path.basename(path)] = bytes(payload)
    return len(payload)
    return len(payload)


def write_paged(path, payload, elem, win=UA_WIN):
    """Lay a lump out the way uglArr's store is laid out.

    Page p starts at byte p*win and holds (win // elem) elements, with the
    page's tail padded -- INCLUDING the last page. uglArrLoad can then read
    whole pages straight into the mapped EMS window, with no partial page to
    special-case and no element arithmetic: the two layouts are identical by
    construction.

    Page padding, not element padding, is deliberate. At 22 bytes it wastes
    16 bytes per page rather than 10 bytes per element -- 64 bytes against
    27,500 on e1m1's node tree.

    Unlike BLOAD there is no 64K cap here, which is the other reason the
    paged lumps are not .bld: e1m1's node tree does not fit in one.
    """
    perpg = win // elem
    if perpg < 1:
        raise SystemExit(f"{path}: element {elem} is larger than a {win}-byte page")
    n = len(payload) // elem
    out = bytearray()
    for start in range(0, n, perpg):
        chunk = payload[start * elem:min(start + perpg, n) * elem]
        out += chunk + b'\x00' * (win - len(chunk))
    OUT[os.path.basename(path)] = bytes(out)
    return len(out)
    return len(out)


def write_bload(path, payload):
    # headerless now: the loader takes the size from uarSize
    OUT[os.path.basename(path)] = bytes(payload)
    return len(payload)
    return len(payload)


LM_CHUNK = 32000        # keep a chunk's byte offsets inside a signed 16-bit
                        # BASIC integer, and every chunk under BLOAD's 64K cap
# The luxel atlas. 8192 divides an EMS page exactly -- two scanlines per
# page, neither straddling one -- which mgl's EMS dcs do NOT guarantee for
# an arbitrary width. It is also uglbmp.asm's BMP_MAX_BPS, the widest
# scanline that loader will take.
EMS_PGSIZE  = 16384
LM_ATLAS_W  = 8192
LM_BMP_MAXBPS = 8192        # uglbmp.asm's BMP_MAX_BPS
assert LM_ATLAS_W <= LM_BMP_MAXBPS, "atlas scanline wider than uGL will load"

def pot2(v):
    """v rounded up to a power of two."""
    p = 1
    while p < v:
        p <<= 1
    return p

def face_lightmap_geometry(d, lumps):
    """Per-face texturemins/extents, exactly as Quake's CalcSurfaceExtents.

    The lightmap grid is one luxel per 16 texels, anchored at the face's
    texture-space minimum rounded down to a multiple of 16. Both numbers are
    needed at runtime -- extents sizes the cached surface, texturemins is what
    turns an interpolated texture coordinate back into a lightmap coordinate --
    and both are a pure function of the .bsp, so they are computed here rather
    than at every launch.
    """
    def lump(i):
        o, n = lumps[i]
        return d[o:o+n]

    raw = lump(3)
    verts = [struct.unpack_from('<3f', raw, k) for k in range(0, len(raw), 12)]
    raw = lump(12)
    edges = [struct.unpack_from('<2H', raw, k) for k in range(0, len(raw), 4)]
    raw = lump(13)
    surfedges = [struct.unpack_from('<i', raw, k)[0] for k in range(0, len(raw), 4)]
    raw = lump(6)
    texinfo = [struct.unpack_from('<8f2i', raw, k) for k in range(0, len(raw), 40)]

    faces = lump(7)
    geo = []
    for k in range(0, len(faces), 20):
        _pl, _sd, firstedge, numedges, ti = struct.unpack_from('<hhihh', faces, k)
        tv = texinfo[ti]
        mins = [1e30, 1e30]
        maxs = [-1e30, -1e30]
        for e in range(numedges):
            se = surfedges[firstedge + e]
            v = verts[edges[abs(se)][0 if se >= 0 else 1]]
            for j in range(2):
                val = (v[0]*tv[j*4+0] + v[1]*tv[j*4+1] +
                       v[2]*tv[j*4+2] + tv[j*4+3])
                mins[j] = min(mins[j], val)
                maxs[j] = max(maxs[j], val)
        tmin, ext = [], []
        for j in range(2):
            bmin = math.floor(mins[j] / 16)
            bmax = math.ceil(maxs[j] / 16)
            tmin.append(int(bmin * 16))
            ext.append(int((bmax - bmin) * 16))
        geo.append((tmin, ext))
    return geo


def convert_lightmaps(d, lumps, out):
    """Repack the LIGHTING lump face-by-face, plus a per-face index.

    Two reasons not to ship the lump verbatim. It is 73.5K on dm3ish and
    BLOAD stops at 64K, so it has to be split regardless; and splitting on
    face boundaries means a face's luxels never straddle two arrays, which
    would otherwise have to be special-cased in the builder. Faces come out
    in face order, so the table is indexed by face id with no indirection.

    Record is 8 integers plus two hash bytes:
        ofs_hi, ofs_lo, tmin_s, tmin_t, lm_w, lm_h, styles01, styles23,
        hash_multiplier, hash_shift
    ofs_hi = -1 marks a face with no lightmap. The two styles words pack four
    bytes each and are written unsigned -- an unlit face is styles 255,255,
    i.e. 0xFFFF, which BASIC reads back as -1; only the bytes matter.
    """
    lo, ln = lumps[8]
    lighting = d[lo:lo+ln]
    faces = d[lumps[7][0]:lumps[7][0]+lumps[7][1]]
    geo = face_lightmap_geometry(d, lumps)

    # Every lit face's planes, one a style, in face order and stacked lm_h
    # rows apart in its slot: R_BuildLightMap sums them, each scaled by its
    # style, and a flickering light lives only in a face's second plane.
    runs = []
    for k in range(0, len(faces), 20):
        styles = faces[k+12:k+16]
        lightofs = struct.unpack_from('<i', faces, k+16)[0]
        (tmin, ext) = geo[k // 20]
        lm_w = (ext[0] >> 4) + 1
        lm_h = (ext[1] >> 4) + 1
        nstyles = sum(1 for b in styles if b != 255)
        if lightofs < 0 or nstyles == 0:
            continue
        size = lm_w * lm_h
        if lightofs + size * nstyles > len(lighting):
            raise SystemExit(f"face {k//20}: lightofs {lightofs}+{size}x{nstyles} "
                             f"runs past the {len(lighting)}-byte LIGHTING lump")
        if pot2(lm_w) * pot2(lm_h * nstyles) > LM_ATLAS_W:
            raise SystemExit(f"face {k//20}: {lm_w}x{lm_h}x{nstyles} luxels round up to "
                             f"a slot past one {LM_ATLAS_W}-byte scanline")
        runs.append((k // 20, size, lightofs, lm_w, lm_h * nstyles))

    # Each face gets a slot of pot(lm_w) x pot(lm_h) bytes. Two reasons.
    #
    # Alignment: a run whose size is 2^k, placed at an offset that is a
    # multiple of 2^k, cannot cross an 8192-byte scanline -- 8192 being a
    # power of two itself. Allocating slots in DESCENDING size order from
    # offset 0 keeps every one of them naturally aligned, so the invariant
    # costs no padding at all beyond the power-of-two rounding.
    #
    # And the rows stay addressable with a single stride, so the builder
    # walks the rect with an add rather than a multiply per row.
    #
    # Only the slot is padded, not the grid: the builder is still told the
    # face's REAL lm_w/lm_h and reads lm_w bytes from each of lm_h rows, so
    # the pad bytes are never read and sf$lgrid stays its old size. Padding
    # the grid itself would have grown that DGROUP buffer four-fold.
    slots = sorted(runs, key=lambda r: -(pot2(r[3]) * pot2(r[4])))
    atlas = bytearray()
    placed = {}
    for (fid, size, lightofs, lm_w, lm_h) in slots:
        pw, ph = pot2(lm_w), pot2(lm_h)
        base = len(atlas)
        assert base % (pw * ph) == 0, "slot allocation lost its alignment"
        atlas += bytes(pw * ph)
        for row in range(lm_h):
            src = lightofs + row * lm_w
            atlas[base + row*pw : base + row*pw + lm_w] = \
                lighting[src:src+lm_w]
        placed[fid] = (base % LM_ATLAS_W, base // LM_ATLAS_W, pw)

    if len(atlas) % LM_ATLAS_W:
        atlas += bytes(LM_ATLAS_W - (len(atlas) % LM_ATLAS_W))
    atlas_h = max(1, len(atlas) // LM_ATLAS_W)

    surface_keys = lighthash.bsp_surface_keys(d)
    animation_counts = lighthash.face_animation_counts(d)
    hash_choices = {
        variant: lighthash.best_hash(keys, 512) for variant, keys in surface_keys.items()
    }
    table = bytearray()
    lit = unlit = 0
    for k in range(0, len(faces), 20):
        styles = faces[k+12:k+16]
        (tmin, ext) = geo[k // 20]
        lm_w = (ext[0] >> 4) + 1
        lm_h = (ext[1] >> 4) + 1
        fid = k // 20
        if fid not in placed:
            unlit += 1
            ax, ay = 0, -1
        else:
            lit += 1
            ax, ay, _pw = placed[fid]
        # Same 16-byte record as before, but the two offset fields now carry
        # the face's place in the atlas: the scanline, then x within it. The
        # scanline takes the signed field so -1 still means "unlit", which is
        # the sign test sb_build already makes.
        style_tuple = tuple(style for style in styles if style != 255)
        choice = hash_choices.get(
            (style_tuple, animation_counts[fid]), lighthash.HashChoice(0, 0, 0)
        )
        if choice.collisions:
            raise SystemExit(
                f"face {fid}: styles {style_tuple} and {animation_counts[fid]} animation frames "
                "have no exact nine-bit surface hash"
            )
        table += struct.pack('<hH4h2HBB', ay, ax, tmin[0], tmin[1], lm_w, lm_h,
                             styles[0] | (styles[1] << 8),
                             styles[2] | (styles[3] << 8),
                             choice.multiplier, choice.shift)

    # Raw rows of LM_ATLAS_W, top down, into a qgl EMS store of one row
    # per record -- the same shape fgeom.bin has. Padded above to a whole
    # row, which the loader demands.
    out['lm.bin'] = bytes(atlas)
    blob = atlas

    # The table is not written out on its own any more: convert_lumps folds
    # it into each face's geometry record, so one mapping and one copy per
    # drawn face fetch the corners and the lightmap placement together.
    return lit, unlit, 1, len(blob), 1, bytes(table)


ENT_PAIR = re.compile(r'"([^"]*)"\s*"([^"]*)"')
TRIG_ONCE, TRIG_MULTI, TRIG_COUNTER, TRIG_BUTTON, TRIG_EXIT, TRIG_SHOOT, TRIG_SECRET = 0, 1, 2, 3, 4, 5, 6   # ENT_TRIG_*
TRIG_SHOOT_BUTTON = 14
TRIG_SHOOTER = 7
TRIG_RELAY, TRIG_BOSS, TRIG_BOLT = 8, 9, 10   # a use passed on; Chthon, unseen; event_lightning
TRIG_FIREBALL = 11   # misc_fireball: a lava ball up from its origin every 3 to 8 seconds
TRIG_MJUMP = 12   # trigger_monsterjump
TRIG_SKILL = 13   # trigger_setskill
KEY_NAMES = ('key', 'runekey', 'keycard')   # items.qc's netname by worldtype



@dataclass(slots=True, frozen=True)
class CrateSrc:
    size: tuple[float, float, float]
    faces: list[tuple[str, list[tuple[int, int, int]]]]   # texture, 4 x (corner bits, u*32, v*32)
    textures: dict[str, tuple[bytes, int]]                # name -> (buffer, miptex offset)


def pack_members(path: str, prefix: str) -> dict[str, bytes]:
    d = open(path, 'rb').read()
    magic, dirofs, dirlen = struct.unpack_from('<4sii', d, 0)
    if magic != b'PACK':
        raise SystemExit(f"{path} is not a PACK archive")
    out: dict[str, bytes] = {}
    for i in range(dirlen // 64):
        e = dirofs + 64 * i
        name = d[e:e + 56].split(b'\0')[0].decode('latin-1')
        off, ln = struct.unpack_from('<ii', d, e + 56)
        if name.startswith(prefix):
            out[name[len(prefix):]] = d[off:off + ln]
    return out


def crate_src(b: bytes) -> CrateSrc:
    # a b_*.bsp is one box from the origin: six faces, the bottom on the
    # floor and never shipped. u,v are the face's own, shifted up by whole
    # textures so the least is in [0,1) -- the filler wraps
    h = struct.unpack_from('<i30i', b, 0)
    lump = lambda k: (h[1 + 2 * k], h[2 + 2 * k])  # noqa: E731
    vo, vl = lump(3)
    verts = [struct.unpack_from('<3f', b, vo + j * 12) for j in range(vl // 12)]
    eo, el = lump(12)
    edges = [struct.unpack_from('<2H', b, eo + j * 4) for j in range(el // 4)]
    so, sl = lump(13)
    sedges = [struct.unpack_from('<i', b, so + j * 4)[0] for j in range(sl // 4)]
    to, tl = lump(6)
    tinf = [struct.unpack_from('<8fii', b, to + j * 40) for j in range(tl // 40)]
    xo, _ = lump(2)
    names: list[tuple[str, int, int]] = []
    textures: dict[str, tuple[bytes, int]] = {}
    for t in range(struct.unpack_from('<i', b, xo)[0]):
        o = struct.unpack_from('<i', b, xo + 4 + t * 4)[0]
        tn, w, hh = struct.unpack_from('<16sii', b, xo + o)
        name = tn.split(b'\0')[0].decode('latin-1')
        names.append((name, w, hh))
        textures[name] = (b, xo + o)
    size = tuple(max(v[i] for v in verts) for i in range(3))
    faces: list[tuple[str, list[tuple[int, int, int]]]] = []
    fo, fl = lump(7)
    for j in range(fl // 20):
        _, _, fe, ne, ti = struct.unpack_from('<hhihh', b, fo + j * 20)
        vs = []
        for k in range(ne):
            e = sedges[fe + k]
            vs.append(verts[edges[abs(e)][0 if e >= 0 else 1]])
        if all(v[2] == 0.0 for v in vs):
            continue
        t = tinf[ti]
        name, tw, th = names[t[8]]
        uv = [((t[0] * x + t[1] * y + t[2] * z + t[3]) / tw, (t[4] * x + t[5] * y + t[6] * z + t[7]) / th)
              for x, y, z in vs]
        du = -math.floor(min(u for u, _ in uv))
        dv = -math.floor(min(v for _, v in uv))
        faces.append((name, [((x > 0) | (y > 0) << 1 | (z > 0) << 2, round((u + du) * 32), round((v + dv) * 32))
                             for (x, y, z), (u, v) in zip(vs, uv)]))
        if len(vs) != 4:
            raise SystemExit(f"crate face {j} has {len(vs)} vertices")
    if len(faces) != 5:
        raise SystemExit(f"crate has {len(faces)} faces off the floor, not 5")
    return CrateSrc(size, faces, textures)


def load_crates(pak: str) -> dict[str, CrateSrc]:
    if not pak or not os.path.exists(pak):
        return {}
    return {n[:-4]: crate_src(b) for n, b in pack_members(pak, 'maps/').items() if n.startswith('b_')}


def crate_name(classname: str, amount: int) -> str | None:
    # items.qc's setmodel by spawnflags, which item_amount already read
    match classname, amount:
        case 'item_health', 15: return 'b_bh10'
        case 'item_health', 100: return 'b_bh100'
        case 'item_health', _: return 'b_bh25'
        case 'item_shells', 40: return 'b_shell1'
        case 'item_shells', _: return 'b_shell0'
        case 'item_spikes', 50: return 'b_nail1'
        case 'item_spikes', _: return 'b_nail0'
        case 'item_rockets', 10: return 'b_rock1'
        case 'item_rockets', _: return 'b_rock0'
        case 'item_cells', 12: return 'b_batt1'
        case 'item_cells', _: return 'b_batt0'
        case 'misc_explobox', _: return 'b_explob'
        case _: return None


@dataclass(slots=True)
class CrateSet:
    src: dict[str, CrateSrc]
    tex_base: int                                            # the map's own texture count
    used: list[str] = field(default_factory=list)            # models in crate-index order
    tex: list[tuple[str, bytes, int]] = field(default_factory=list)   # atlas cells after the map's

    def crate(self, name: str | None) -> int:
        if name is None or name not in self.src:
            return -1
        if name not in self.used:
            self.used.append(name)
        return self.used.index(name)

    def tex_id(self, model: str, name: str) -> tuple[int, int]:
        # a +0name face brings every +Nname frame, consecutive: the
        # renderer steps them at 10 Hz as it does the world's chains
        textures = self.src[model].textures
        frames = sorted(n for n in textures if n[0] == '+' and n[2:] == name[2:]) if name[0] == '+' else [name]
        ids = []
        for n in frames:
            if n not in [t[0] for t in self.tex]:
                self.tex.append((n, *textures[n]))
            ids.append(self.tex_base + [t[0] for t in self.tex].index(n))
        if ids != list(range(ids[0], ids[0] + len(ids))):
            raise SystemExit(f"{name}'s frames are not consecutive in the atlas: {ids}")
        return ids[0], len(ids)

    def cells(self) -> list[tuple[bytes, int]]:
        return [(b, o) for _, b, o in self.tex]


MON_KIND = {'monster_army': 0, 'monster_knight': 1, 'monster_dog': 2, 'monster_ogre': 3, 'monster_demon1': 4,
            'monster_zombie': 5, 'monster_wizard': 6, 'monster_shambler': 7}   # MDL_KIND_*; no model for the rest
ITEM_KIND = {'item_health': 0, 'item_shells': 1, 'item_armor1': 2, 'item_armor2': 3,
             'weapon_supershotgun': 4, 'item_spikes': 5, 'weapon_nailgun': 6,
             'item_artifact_super_damage': 7, 'item_artifact_envirosuit': 8, 'misc_explobox': 9,
             'item_key1': 10, 'item_key2': 11, 'weapon_grenadelauncher': 12, 'item_rockets': 13,
             'weapon_supernailgun': 14, 'weapon_rocketlauncher': 15,
             'item_artifact_invulnerability': 16, 'item_sigil': 17, 'item_armorInv': 18, 'item_cells': 19,
             'item_artifact_invisibility': 20, 'weapon_lightning': 21}
# misc.qc's ambientsound calls: the wav and its volume, ATTN_STATIC
AMB_KIND = {'ambient_comp_hum': ('ambience/comp1', 1.0), 'ambient_drone': ('ambience/drone6', 0.5),
            'ambient_drip': ('ambience/drip1', 0.5), 'ambient_swamp1': ('ambience/swamp1', 0.5),
            'ambient_swamp2': ('ambience/swamp2', 0.5),
            # FireAmbient, and light_fluoro's and light_fluorospark's own
            'light_torch_small_walltorch': ('ambience/fire1', 0.5),
            'light_flame_large_yellow': ('ambience/fire1', 0.5),
            'light_flame_small_yellow': ('ambience/fire1', 0.5),
            'light_flame_small_white': ('ambience/fire1', 0.5),
            'light_fluoro': ('ambience/fl_hum1', 0.5), 'light_fluorospark': ('ambience/buzz1', 0.5)}
FLAME_KIND = {
    'light_torch_small_walltorch': (0, 0),
    'light_flame_large_yellow': (1, 1),
    'light_flame_small_yellow': (1, 0),
    'light_flame_small_white': (1, 0),
}


@dataclass(slots=True)
class Ents:
    # What parse_entities gathers, in the map's entity order. The spawn
    # functions append to it, which is why they are not pure: the order is
    # the file's, and ids, crate numbers and the bolt doors' indices follow it.
    nmodels: int
    boxes: list[tuple[float, ...]]
    skill: int
    crates: CrateSet
    spawn: tuple[float, float, float] = (0.0, 0.0, 0.0)
    title: str = ''
    worldtype: int = 0
    angle: float = 0.0
    spawn2: tuple[float, float, float] | None = None   # info_player_start2
    angle2: float = 0.0
    inter: tuple[tuple[float, float, float], float, float] | None = None   # origin, pitch, yaw
    dests: dict[str, tuple[tuple[float, float, float], float]] = field(default_factory=dict)
    trigs: list[tuple[str, int, int, int]] = field(default_factory=list)   # target, model, targetname id, PLAYER_ONLY
    hides: list[int] = field(default_factory=list)
    gates: list[tuple[int, int, int]] = field(default_factory=list)   # model, 1 the boss gate, rune bits
    bolt_doors: list[int] = field(default_factory=list)   # the electrode doors, target "lightning"
    plats: list[tuple] = field(default_factory=list)
    doors: list[tuple] = field(default_factory=list)
    items: list[tuple] = field(default_factory=list)
    uses: list[tuple] = field(default_factory=list)
    names: dict[str, int] = field(default_factory=dict)
    mons: list[tuple] = field(default_factory=list)
    ambs: list[tuple[int, int, tuple[float, float, float]]] = field(default_factory=list)
    flames: list[tuple[tuple[float, float, float], int, int]] = field(default_factory=list)
    air_bubbles: list[tuple[float, float, float]] = field(default_factory=list)
    # targetname, origin, wait, target
    corners: list[tuple[str, tuple[float, float, float], float, str]] = field(default_factory=list)
    trains: list[tuple] = field(default_factory=list)   # model, speed, damage, targetname id, first corner, sounds
    lights: list[tuple[int, int, int]] = field(default_factory=list)   # targetname id, style, starts on
    combat_order: dict[int, int] = field(default_factory=dict)         # brush model -> source edict


def ent_vec(v: str) -> tuple[float, float, float]:
    x, y, z = (float(t) for t in v.split())
    return (x, y, z)


def ent_item_amount(classname: str, flags: int) -> int:
    # items.qc: H_ROTTEN 15, H_MEGA 100, else 25; WEAPON_BIG2 40 shells,
    # else 20; armor_touch's 100 green, 150 yellow, 200 red; weapon_touch's
    # 5 shells with the super shotgun, 30 nails with either nailgun and
    # 5 rockets with either launcher; item_rockets 5, WEAPON_BIG2 10;
    # item_spikes 25, WEAPON_BIG2 50; a powerup's 30 seconds; the
    # exploding box's 20 health
    match classname, flags & 1, flags & 2:
        case 'item_sigil', _, _: return flags & 15   # sigil_touch's serverflags bits
        case ('item_artifact_super_damage' | 'item_artifact_envirosuit' | 'item_artifact_invulnerability'
              | 'item_artifact_invisibility'), _, _:
            return 30
        case 'misc_explobox', _, _: return 20
        case 'weapon_supershotgun', _, _: return 5
        case 'weapon_nailgun', _, _: return 30
        case 'weapon_grenadelauncher' | 'weapon_rocketlauncher', _, _: return 5
        case 'weapon_supernailgun', _, _: return 30
        case 'weapon_lightning', _, _: return 15
        case 'item_rockets', 1, _: return 10
        case 'item_rockets', _, _: return 5
        case 'item_spikes', 1, _: return 50
        case 'item_spikes', _, _: return 25
        case 'item_cells', 1, _: return 12
        case 'item_cells', _, _: return 6
        case 'item_armor1', _, _: return 100
        case 'item_armor2', _, _: return 150
        case 'item_armorInv', _, _: return 200
        case 'item_health', 1, _: return 15
        case 'item_health', _, 2: return 100
        case 'item_health', _, _: return 25
        case _, 1, _: return 40
        case _: return 20


def ent_name_id(e: Ents, s: str) -> int:
    # targetnames become ids; 0 is none
    return e.names.setdefault(s, len(e.names) + 1) if s else 0


CENTERPRINT_MAX = 1023


def message_bytes(text: str) -> bytes:
    value = text.replace('\\n', '\n').encode('latin1')
    if len(value) > CENTERPRINT_MAX:
        raise ValueError(f"centerprint exceeds {CENTERPRINT_MAX} bytes")
    return value


def ent_msg(kv: dict[str, str]) -> bytes:
    return message_bytes(kv.get('message', ''))


def message_pool(messages: list[bytes]) -> tuple[dict[bytes, int], bytes]:
    offsets: dict[bytes, int] = {}
    pool = bytearray(b'\0')
    for message in messages:
        if not message or message in offsets:
            continue
        offsets[message] = len(pool)
        pool += message + b'\0'
    if len(pool) > 32767:
        raise ValueError("entity message pool exceeds 16-bit offsets")
    return offsets, bytes(pool)


def ent_movedir(angle: float) -> tuple[float, float, float]:
    # SetMovedir: angle -1 up, -2 down, anything else a heading
    match angle:
        case -1.0: return (0.0, 0.0, 1.0)
        case -2.0: return (0.0, 0.0, -1.0)
        case _: return (math.cos(math.radians(angle)), math.sin(math.radians(angle)), 0.0)


def ent_travel(angle: float, box: tuple[float, ...], lip: float) -> tuple[float, float, float]:
    # the brush moves its size along its movedir, less the lip
    mdir = ent_movedir(angle)
    size = [box[k + 3] - box[k] for k in range(3)]
    dist = max(sum(abs(mdir[k]) * size[k] for k in range(3)) - lip, 0.0)
    return (mdir[0] * dist, mdir[1] * dist, mdir[2] * dist)


def ent_model(e: Ents, v: str) -> int:
    m = int(v[1:]) if v.startswith('*') and v[1:].isdigit() else 0
    return m if 0 < m < e.nmodels else 0


def ent_door(e: Ents, m: int, kv: dict[str, str], box: tuple[float, ...]) -> tuple:
    # func_door: speed 100, wait 3, lip 8 unless the map says
    speed = float(kv.get('speed', '0')) or 100.0
    hold = float(kv.get('wait', '0')) or 3.0
    lip = float(kv.get('lip', '0')) or 8.0
    flags = int(kv.get('spawnflags', '0'))
    travel = ent_travel(float(kv.get('angle', '0')), box, lip)
    # DOOR_SILVER_KEY 16, DOOR_GOLD_KEY 8: door_touch's "You need the
    # silver key" stands in for a message the map does not give
    key = 1 if flags & 16 else 2 if flags & 8 else 0
    msg = ent_msg(kv)
    if key and not kv.get('message'):
        msg = message_bytes(f"You need the {'silver' if key == 1 else 'gold'} {KEY_NAMES[e.worldtype]}")
    return (m, travel, (0.0, 0.0, 0.0), speed, hold, 1 if flags & 1 else 0, 1 if flags & 4 else 0,
            ent_name_id(e, kv.get('targetname', '')), 0, 0, int(kv.get('sounds', '0')), key, msg)


def ent_secret(e: Ents, m: int, kv: dict[str, str], box: tuple[float, ...]) -> tuple:
    # func_door_secret, fd_secret_use: back t_width along v_right (or
    # down), then t_length along v_forward; speed 50, wait 5, open_once
    # stays; shot open unless named, or no_shoot, or always_shoot
    flags = int(kv.get('spawnflags', '0'))
    yaw = math.radians(float(kv.get('angle', '0')))
    fwd = (math.cos(yaw), math.sin(yaw), 0.0)
    right = (math.sin(yaw), -math.cos(yaw), 0.0)
    size = [box[k + 3] - box[k] for k in range(3)]
    width = float(kv.get('t_width', '0')) or (size[2] if flags & 4 else abs(sum(right[k] * size[k] for k in range(3))))
    length = float(kv.get('t_length', '0')) or abs(sum(fwd[k] * size[k] for k in range(3)))
    temp = 1.0 - (flags & 2)
    mid = (0.0, 0.0, -width) if flags & 4 else tuple(right[k] * width * temp for k in range(3))
    travel = tuple(mid[k] + fwd[k] * length for k in range(3))
    speed = float(kv.get('speed', '0')) or 50.0
    hold = -1.0 if flags & 1 else (float(kv.get('wait', '0')) or 5.0)
    name = ent_name_id(e, kv.get('targetname', ''))
    shoot = 0 if flags & 8 else (1 if not name or flags & 16 else 0)
    return (m, travel, mid, speed, hold, 0, kv.get('target', ''), name, 1, shoot,
            int(kv.get('sounds', '0')) or 3, 0, ent_msg(kv))


def ent_trig(e: Ents, m: int, kv: dict[str, str]) -> tuple:
    # trigger_once is a multiple with wait -1; a multiple re-arms after
    # wait, 0.2 unless the map says; a counter fires at count, 2
    match kv['classname']:
        case 'trigger_secret': kind, wait, count = TRIG_SECRET, -1.0, 0
        case 'trigger_once': kind, wait, count = TRIG_ONCE, -1.0, 0
        case 'trigger_multiple': kind, wait, count = TRIG_MULTI, float(kv.get('wait', '0')) or 0.2, 0
        case _: kind, wait, count = TRIG_COUNTER, -1.0, int(kv.get('count', '0')) or 2
    if int(kv.get('health', '0')) > 0:
        kind = TRIG_SHOOT   # multi_killed: shot, not touched; wait as above
    msg = ent_msg(kv)
    snd = int(kv.get('sounds', '0'))
    if kind == TRIG_SECRET:
        snd = snd or 1
        if not kv.get('message'):
            msg = b'You found a secret area!'
    # delay: SUB_UseTargets' DelayThink, seconds before the target fires
    return (m, kind, ent_name_id(e, kv.get('target', '')), ent_name_id(e, kv.get('targetname', '')),
            ent_name_id(e, kv.get('killtarget', '')), count, wait, 0.0, (0.0, 0.0, 0.0), snd, msg,
            (0.0, 0.0, 0.0), float(kv.get('delay', '0')))


def ent_button(e: Ents, m: int, kv: dict[str, str], box: tuple[float, ...]) -> tuple:
    # func_button: speed 40, wait 1, lip 4; wait -1 stays pressed
    speed = float(kv.get('speed', '0')) or 40.0
    wait = float(kv.get('wait', '0')) or 1.0
    lip = float(kv.get('lip', '0')) or 4.0
    travel = ent_travel(float(kv.get('angle', '0')), box, lip)
    kind = TRIG_SHOOT_BUTTON if int(kv.get('health', '0')) > 0 else TRIG_BUTTON
    return (m, kind, ent_name_id(e, kv.get('target', '')), ent_name_id(e, kv.get('targetname', '')),
            ent_name_id(e, kv.get('killtarget', '')), 0, wait, speed, travel, int(kv.get('sounds', '0')), ent_msg(kv))


# The spawn functions: each returns False where it declines the entity -- no
# brush, a second intermission, a light under style 32, an onlyregistered with no
# message -- and ent_spawn then
# tries what a declined classname can still be.

def sp_worldspawn(e: Ents, kv: dict[str, str]) -> bool:
    e.title = kv.get('message', '')
    e.worldtype = int(kv.get('worldtype', '0') or 0)
    return True


def sp_player_start(e: Ents, kv: dict[str, str]) -> bool:
    e.spawn = ent_vec(kv.get('origin', '0 0 0'))
    e.angle = float(kv.get('angle', '0'))
    return True


def sp_player_start2(e: Ents, kv: dict[str, str]) -> bool:
    e.spawn2 = ent_vec(kv.get('origin', '0 0 0'))
    e.angle2 = float(kv.get('angle', '0'))
    return True


def sp_intermission(e: Ents, kv: dict[str, str]) -> bool:
    # FindIntermission picks one at random; the first is as good
    if e.inter is not None:
        return False
    mangle = ent_vec(kv.get('mangle', '0 0 0'))
    e.inter = (ent_vec(kv.get('origin', '0 0 0')), mangle[0], mangle[1])
    return True


def sp_teleport_dest(e: Ents, kv: dict[str, str]) -> bool:
    e.dests[kv.get('targetname', '')] = (ent_vec(kv.get('origin', '0 0 0')), float(kv.get('angle', '0')))
    return True


def sp_teleport(e: Ents, kv: dict[str, str]) -> bool:
    if not (m := ent_model(e, kv.get('model', ''))):
        return False
    e.trigs.append((kv.get('target', ''), m, ent_name_id(e, kv.get('targetname', '')),
                    int(kv.get('spawnflags', '0')) & 1))
    e.hides.append(m)
    if not int(kv.get('spawnflags', '0')) & 2:
        box = e.boxes[m]
        center = tuple((box[i] + box[i + 3]) * 0.5 for i in range(3))
        e.ambs.append((mksnd.SOUNDS.index('ambience/hum1'), 127, center))
    return True


def sp_trigger(e: Ents, kv: dict[str, str]) -> bool:
    # trigger_once, trigger_multiple, trigger_secret
    if not (m := ent_model(e, kv.get('model', ''))):
        return False
    e.hides.append(m)
    e.uses.append(ent_trig(e, m, kv))
    return True


def sp_counter(e: Ents, kv: dict[str, str]) -> bool:
    if m := ent_model(e, kv.get('model', '')):
        e.hides.append(m)
    e.uses.append(ent_trig(e, m, kv))
    return True


def sp_spikeshooter(e: Ents, kv: dict[str, str]) -> bool:
    # spikeshooter_use: a spike along movedir at 500 when used;
    # SUPERSPIKE (1) bites 18 for 9. wait carries the damage
    e.uses.append((0, TRIG_SHOOTER, 0, ent_name_id(e, kv.get('targetname', '')), 0,
                   18 if int(kv.get('spawnflags', '0')) & 1 else 9, 0.0, 500.0,
                   ent_movedir(float(kv.get('angle', '0'))), 0, b'', ent_vec(kv.get('origin', '0 0 0'))))
    return True


def sp_changelevel(e: Ents, kv: dict[str, str]) -> bool:
    # the level ends here; its message is the map's title
    if not (m := ent_model(e, kv.get('model', ''))):
        return False
    e.hides.append(m)
    # count carries the trigger's own map, a message id once packed: one next_map a
    # file kept the last read, and e1m4's main exit led to e1m8
    # snd carries NO_INTERMISSION; exits have no sound field of their own.
    # Keep the names as strings until pack_entities: allocating their ids
    # here would renumber every later target in old saves.
    e.uses.append((m, TRIG_EXIT, kv.get('target', ''), 0,
                   kv.get('killtarget', ''), kv.get('map', '')[:8].encode('latin1'),
                   -1.0, 0.0, (0.0, 0.0, 0.0), int(kv.get('spawnflags', '0')) & 1,
                   message_bytes(e.title), (0.0, 0.0, 0.0), float(kv.get('delay', '0'))))
    return True


def sp_button(e: Ents, kv: dict[str, str]) -> bool:
    if not (m := ent_model(e, kv.get('model', ''))):
        return False
    e.uses.append(ent_button(e, m, kv, e.boxes[m]))
    return True


def sp_fireball(e: Ents, kv: dict[str, str]) -> bool:
    # speed as the map gives it: id's default is `self.speed == 1000`,
    # a compare, so an unset one leaves 0 and the ball rises 0..200
    e.uses.append((0, TRIG_FIREBALL, 0, 0, 0, 0, 0.0, float(kv.get('speed', '0')),
                   (0.0, 0.0, 0.0), 0, b'', ent_vec(kv.get('origin', '0 0 0'))))
    return True


def sp_monsterjump(e: Ents, kv: dict[str, str]) -> bool:
    # a walking monster in it takes movedir * speed, and height up from the ground;
    # travel carries that velocity
    if not (m := ent_model(e, kv.get('model', ''))):
        return False
    e.hides.append(m)
    speed, height = float(kv.get('speed', '0')) or 200.0, float(kv.get('height', '0')) or 200.0
    dx, dy, _ = ent_movedir(float(kv.get('angle', '0')))
    e.uses.append((m, TRIG_MJUMP, 0, 0, 0, 0, 0.0, 0.0, (dx * speed, dy * speed, height), 0, b''))
    return True


def sp_setskill(e: Ents, kv: dict[str, str]) -> bool:
    # trigger_skill_touch: the skill cvar on every touch, which the next map loads by
    if not (m := ent_model(e, kv.get('model', ''))):
        return False
    e.hides.append(m)
    skill = min(max(int(float(kv.get('message', '0') or '0')), 0), SKILLS - 1)
    e.uses.append((m, TRIG_SKILL, 0, 0, 0, skill, 0.0, 0.0, (0.0, 0.0, 0.0), 0, b''))
    return True


def sp_relay(e: Ents, kv: dict[str, str]) -> bool:
    # SUB_UseTargets passed on, at once: delay is not ported
    e.uses.append((0, TRIG_RELAY, ent_name_id(e, kv.get('target', '')), ent_name_id(e, kv.get('targetname', '')),
                   ent_name_id(e, kv.get('killtarget', '')), 0, 0.0, 0.0, (0.0, 0.0, 0.0), 0, ent_msg(kv)))
    return True


def sp_boss(e: Ents, kv: dict[str, str]) -> bool:
    # Chthon: the rune wakes him, his health boss_awake's 1 on easy else 3,
    # a bolt a point, dead his target fires; his model ships as BOSS_MDL
    yaw = float(kv.get('angle', '0'))
    e.uses.append((0, TRIG_BOSS, ent_name_id(e, kv.get('target', '')), ent_name_id(e, kv.get('targetname', '')),
                   0, 1 if e.skill == 0 else 3, yaw, yaw, (0.0, 0.0, 0.0), 0, b'',
                   ent_vec(kv.get('origin', '0 0 0'))))
    return True


def sp_lightning(e: Ents, kv: dict[str, str]) -> bool:
    # lightning_use: a point off Chthon with both electrode doors up; travel
    # carries the doors' indices, patched once every door is read
    e.uses.append((0, TRIG_BOLT, 0, ent_name_id(e, kv.get('targetname', '')),
                   0, 0, 0.0, 0.0, (0.0, 0.0, 0.0), 0, b'', ent_vec(kv.get('origin', '0 0 0'))))
    return True


def sp_onlyregistered(e: Ents, kv: dict[str, str]) -> bool:
    # OnlyRegisteredTouch on the shareware: the message and misc/talk
    # every two seconds, its target never fired -- a multiple with no target
    if not ((m := ent_model(e, kv.get('model', ''))) and kv.get('message')):
        return False
    e.hides.append(m)
    e.uses.append(ent_trig(e, m, {'classname': 'trigger_multiple', 'message': kv['message'], 'wait': '2'}))
    return True


def sp_episodegate(e: Ents, kv: dict[str, str]) -> bool:
    # misc.qc: there for its rune held, so the runtime decides
    if not (m := ent_model(e, kv.get('model', ''))):
        return False
    e.gates.append((m, 0, int(kv.get('spawnflags', '0')) & 15))
    return True


def sp_bossgate(e: Ents, kv: dict[str, str]) -> bool:
    # there until all four are held
    if not (m := ent_model(e, kv.get('model', ''))):
        return False
    e.gates.append((m, 1, 15))
    return True


def sp_door(e: Ents, kv: dict[str, str]) -> bool:
    if not (m := ent_model(e, kv.get('model', ''))):
        return False
    if kv.get('target') == 'lightning':
        e.bolt_doors.append(len(e.doors))
    e.doors.append(ent_door(e, m, kv, e.boxes[m]))
    return True


def sp_door_secret(e: Ents, kv: dict[str, str]) -> bool:
    if not (m := ent_model(e, kv.get('model', ''))):
        return False
    e.doors.append(ent_secret(e, m, kv, e.boxes[m]))
    return True


def sp_light(e: Ents, kv: dict[str, str]) -> bool:
    # light_use toggles its style between "m" and "a", START_OFF (1)
    # starting it "a"; a plain light with no targetname is removed
    # before it sets anything
    if int(kv.get('style', '0') or 0) < 32:
        return False
    if kv.get('targetname') or kv['classname'] == 'light_fluoro':
        e.lights.append((ent_name_id(e, kv.get('targetname', '')), int(kv['style']),
                         0 if int(kv.get('spawnflags', '0')) & 1 else 1))
    if kv['classname'] == 'light_fluoro':
        e.ambs.append((mksnd.SOUNDS.index('ambience/fl_hum1'), 127, ent_vec(kv.get('origin', '0 0 0'))))
    return True


def sp_corner(e: Ents, kv: dict[str, str]) -> bool:
    e.corners.append((kv.get('targetname', ''), ent_vec(kv.get('origin', '0 0 0')), float(kv.get('wait', '0')),
                      kv.get('target', '')))
    return True


def sp_train(e: Ents, kv: dict[str, str]) -> bool:
    # func_train: speed 100; a targetname waits for its trigger
    if not (m := ent_model(e, kv.get('model', ''))):
        return False
    e.trains.append((m, float(kv.get('speed', '0')) or 100.0, int(float(kv.get('dmg', '0')) or 2.0),
                     ent_name_id(e, kv.get('targetname', '')),
                     kv.get('target', ''), int(kv.get('sounds', '0') or 0)))
    return True


def sp_plat(e: Ents, kv: dict[str, str]) -> bool:
    # sounds 0 is 2, medplat
    if not (m := ent_model(e, kv.get('model', ''))):
        return False
    e.plats.append((m, float(kv.get('speed', '0')), float(kv.get('height', '0')),
                    int(kv.get('sounds', '0') or 0) or 2,
                    int(kv.get('spawnflags', '0') or 0) & 1))
    return True


def sp_air_bubbles(e: Ents, kv: dict[str, str]) -> bool:
    e.air_bubbles.append(ent_vec(kv.get('origin', '0 0 0')))
    return True


def sp_flame(e: Ents, kv: dict[str, str]) -> bool:
    model, anim_set = FLAME_KIND[kv['classname']]
    origin = ent_vec(kv.get('origin', '0 0 0'))
    e.flames.append((origin, model, anim_set))
    wav, volume = AMB_KIND[kv['classname']]
    e.ambs.append((mksnd.SOUNDS.index(wav), int(255 * volume), origin))
    return True


SPAWN: dict[str, Callable[[Ents, dict[str, str]], bool]] = {
    'worldspawn': sp_worldspawn,
    'info_player_start': sp_player_start,
    'info_player_start2': sp_player_start2,
    'info_intermission': sp_intermission,
    'info_teleport_destination': sp_teleport_dest,
    'trigger_teleport': sp_teleport,
    'trigger_once': sp_trigger,
    'trigger_multiple': sp_trigger,
    'trigger_secret': sp_trigger,
    'trigger_counter': sp_counter,
    'trap_spikeshooter': sp_spikeshooter,
    'trigger_changelevel': sp_changelevel,
    'func_button': sp_button,
    'misc_fireball': sp_fireball,
    'trigger_monsterjump': sp_monsterjump,
    'trigger_setskill': sp_setskill,
    'trigger_relay': sp_relay,
    'monster_boss': sp_boss,
    'event_lightning': sp_lightning,
    'trigger_onlyregistered': sp_onlyregistered,
    'func_episodegate': sp_episodegate,
    'func_bossgate': sp_bossgate,
    'func_door': sp_door,
    'func_door_secret': sp_door_secret,
    'light': sp_light,
    'light_fluoro': sp_light,
    'path_corner': sp_corner,
    'func_train': sp_train,
    'func_plat': sp_plat,
    'air_bubbles': sp_air_bubbles,
    **{name: sp_flame for name in FLAME_KIND},
}


def ent_spawn(e: Ents, kv: dict[str, str], order: int) -> None:
    # ED_FindFunction: the classname's spawn function, and for one it has
    # not or one that declined, the kinds a classname can still name
    c = kv.get('classname')
    if c is None:
        return
    if (fn := SPAWN.get(c)) and fn(e, kv):
        return
    m = ent_model(e, kv.get('model', ''))
    match c:
        case str() if c.startswith('trigger_') and m:
            # any trigger's brush is a volume: e1m1 drew its changelevel
            # as a column of the "trigger" texture
            e.hides.append(m)
        case str() if c in MON_KIND:
            # SPAWN_CRUCIFIED: a zombie nailed up, never walkmonster_start
            e.mons.append((MON_KIND[c], order, ent_vec(kv.get('origin', '0 0 0')), float(kv.get('angle', '0')),
                           kv.get('target', ''), int(kv.get('spawnflags', '0')) & 1 if c == 'monster_zombie' else 0))
        case str() if c in ITEM_KIND:
            amount = ent_item_amount(c, int(kv.get('spawnflags', '0')))
            e.items.append((ITEM_KIND[c], amount, ent_name_id(e, kv.get('target', '')),
                            e.crates.crate(crate_name(c, amount)), order,
                            ent_vec(kv.get('origin', '0 0 0'))))
        case str() if c in AMB_KIND:
            wav, fvol = AMB_KIND[c]
            e.ambs.append((mksnd.SOUNDS.index(wav), int(255 * fvol), ent_vec(kv.get('origin', '0 0 0'))))


def spawn_entities(text: str, nmodels: int, boxes: list[tuple[float, ...]], skill: int,
                   crates: CrateSet) -> Ents:
    # Resolved here, not on the target: BASIC strings cap at 32,767 bytes
    # and e1m3's entities lump is 45,762 -- mod_find_spawn died at error 5
    # before anything else could. The renderer wants four facts out of the
    # text, so those are what ships: spawn, matched teleporter pairs,
    # func_plats, func_doors, what fires them, the monsters, and which
    # submodels a trigger hides. Layout must match the Ents* types in q_ent.bi.
    e = Ents(nmodels, boxes, skill, crates)
    # NOT_EASY 256, NOT_MEDIUM 512, NOT_HARD 1024, on any entity -- e1m1's
    # ambush hints exist only below hard; nightmare is hard's set
    skip = 256 << min(skill, 2)
    for order, block in enumerate(text.split('{')[1:], 1):
        kv = dict(ENT_PAIR.findall(block.split('}')[0]))
        if m := ent_model(e, kv.get('model', '')):
            e.combat_order[m] = order
        if int(kv.get('spawnflags', '0')) & skip:
            # an entity the skill removes never links its brush: e1m3's NOT_EASY
            # trigger_once *67 drew as a solid wall of the "trigger" texture
            if ent_model(e, kv.get('model', '')):
                e.hides.append(ent_model(e, kv['model']))
            continue
        ent_spawn(e, kv, order)
    return e


def pack_entities(e: Ents, nmodels: int, gravity: float, crates: CrateSet) -> bytes:
    # every skill spawned before any is packed: the crate records are the union's,
    # so one atlas serves all four
    spawn, angle, inter, worldtype = e.spawn, e.angle, e.inter, e.worldtype
    trigs, dests, hides, bolt_doors, plats, doors = e.trigs, e.dests, e.hides, e.bolt_doors, e.plats, e.doors
    items, uses, mons, ambs, corners, trains, lights = e.items, e.uses, e.mons, e.ambs, e.corners, e.trains, e.lights
    uses = [(*u[:2], ent_name_id(e, u[2]), u[3], ent_name_id(e, u[4]), *u[5:])
            if u[1] == TRIG_EXIT else u for u in uses]
    # Exit target ids precede fields added after savegames existed. Keep that
    # stable, then resolve a secret door's outgoing target.
    doors = [(*d[:6], ent_name_id(e, d[6]), *d[7:]) if d[8] else d for d in doors]

    # a teleporter with no destination still hides its brush
    teles = [(m, *dests[t], name, only) for t, m, name, only in trigs if t in dests]

    if inter is None:
        inter = (spawn, 0.0, angle)   # no info_intermission: the start, as Quake does
    spawn2, angle2 = (e.spawn2, e.angle2) if e.spawn2 else (spawn, angle)
    # corners by index, each pointing at the next; a train at one with no target stays
    corner_at = {name: i for i, (name, _, _, _) in reversed(list(enumerate(corners))) if name}
    trains = [(m, speed, dmg, targeted, corner_at[first], snd)
              for m, speed, dmg, targeted, first, snd in trains
              if first in corner_at]
    if any(u[1] == TRIG_BOLT for u in uses):
        assert len(bolt_doors) == 2, bolt_doors
        uses = [(*u[:8], (float(bolt_doors[0]), float(bolt_doors[1]), 0.0), *u[9:]) if u[1] == TRIG_BOLT else u
                for u in uses]
    # Message fields are 16-bit offsets into one NUL string pool. Offset zero
    # is the empty string, so doors, centerprints and changelevel map names
    # share storage without a fixed 1 KB slot per possible centerprint.
    messages: list[bytes] = []

    def msg_id(b: bytes) -> int:
        return msg_offsets.get(b, 0)

    for d in doors:
        messages.append(d[-1])
    for u in uses:
        messages.append(u[10])
        if u[1] == TRIG_EXIT:
            messages.append(u[5])
    title = message_bytes(e.title)
    messages.append(title)
    msg_offsets, msg_pool = message_pool(messages)
    buf = bytearray(struct.pack(ENTS_HEAD, *spawn, angle, *inter[0], inter[1], inter[2], nmodels,
                                len(teles), len(plats), len(hides), len(items), len(doors), len(uses), len(mons),
                                len(ambs), len(trains), len(corners), worldtype, len(crates.used), gravity,
                                len(msg_pool), msg_id(title), len(lights), *spawn2, angle2, len(e.gates)))
    for kind, order, org, yaw, target, cruc in mons:
        buf += struct.pack(ENTS_MON, kind, order, *org, yaw, corner_at.get(target, -1), cruc)
    for m, org, yaw, name, only in teles:
        buf += struct.pack('<h3ffhh', m, *org, yaw, name, only)
    for m, speed, height, snd, low_trigger in plats:
        buf += struct.pack('<hffhh', m, speed, height, snd, low_trigger)
    for m in hides:
        buf += struct.pack('<h', m)
    for m, boss, bits in e.gates:
        buf += struct.pack('<hhh', m, boss, bits)
    for kind, amount, target, crate, order, org in items:
        buf += struct.pack('<hhhhh3f', kind, amount, target, crate, order, *org)
    for m, travel, mid, speed, hold, start_open, action, targeted, secret, shoot, snd, key, msg in doors:
        buf += struct.pack('<h3f3fffhhhhhhhhh', m, *travel, *mid, speed, hold, start_open, action, targeted,
                           secret, shoot, snd, key, msg_id(msg), e.combat_order.get(m, 0))
    for m, kind, target, name, kill, count, wait, speed, travel, snd, msg, *rest in uses:
        org = rest[0] if rest else (0.0, 0.0, 0.0)   # a shooter's
        delay = rest[1] if len(rest) > 1 else 0.0
        count = msg_id(count) if kind == TRIG_EXIT else count
        buf += struct.pack('<6hff3f3fhhfh', m, kind, target, name, kill, count, wait, speed, *travel, *org, snd,
                           msg_id(msg), delay, e.combat_order.get(m, 0))
    for snd, vol, org in ambs:
        buf += struct.pack('<hh3f', snd, vol, *org)
    for m, speed, dmg, targeted, first, snd in trains:
        buf += struct.pack('<hfhhhh', m, speed, dmg, targeted, first, snd)
    for _, org, wait, target in corners:
        buf += struct.pack('<3ffh', *org, wait, corner_at.get(target, -1))
    # q_ent.bi's CrateModel: the size, then five faces of atlas id,
    # frame count and four (corner bits, u*32, v*32)
    for name in crates.used:
        src = crates.src[name]
        buf += struct.pack('<3f', *src.size)
        for tn, corners4 in src.faces:
            tex, frames = crates.tex_id(name, tn)
            buf += struct.pack('<hh12b', tex, frames, *(v for corner in corners4 for v in corner))
    for name, style, on in lights:
        buf += struct.pack('<3h', name, style, on)
    buf += msg_pool
    return bytes(buf)


def convert_lumps(d, lumps, outdir, gravity, crates):
    """Convert each lump from its on-disk layout to the renderer's own.

    model.bas did this per element, in BASIC, copying field by field -- and
    for three lumps the two layouts genuinely differ, which is why the loop
    existed rather than a bulk read. Doing it here means the target can BLOAD
    each array in one go."""
    out = {}

    def lump(i):
        o, n = lumps[i]
        return d[o:o+n]

    # Lightmaps first: their per-face table goes into the geometry records
    # below, so it has to exist before they are built.
    lit, unlit, nchunks, lmbytes, ntab, lmtab = convert_lightmaps(d, lumps, out)
    print(f"  lightmaps    {lit:,} lit faces, {unlit:,} unlit, "
          f"{lmbytes:,} bytes in {nchunks} chunk(s), table in {ntab}")

    # The per-face geometry store, fgeom.bin -- which replaces BOTH the
    # vertex array and the surfedge list.
    #
    # The renderer's inner loop wants one thing from the mesh: this face's
    # corner positions, in order. An indexed mesh cannot answer that
    # without both tables resident, which is why they cost 43K of
    # conventional memory on dm3ish and 151K on e1m1. Written out flat,
    # per face, the answer streams from EMS with a working set of ONE
    # face -- so the arrays leave low memory entirely and nothing has to
    # be cached, evicted or prefetched.
    #
    # The price is duplication: a vertex shared by four faces is stored
    # four times, 3.6x overall on e1m1. In EMS, against 4MB, that is not
    # a price.
    #
    # Layout. Rows of GEOM_W bytes, because a row is what uglMapEx maps
    # and a record must not straddle one -- the same constraint, and the
    # same solution, as the luxel atlas. A record is
    #
    #     word nvtx
    #     18 bytes of lightmap placement and exact hash parameters
    #     nvtx * { int16 x, y, z }                        Q13.3 as before
    #
    # and a face carries (row, offset) instead of (ledgeid, ledgenum). The
    # lightmap header rides along because both are wanted at the same
    # moment, by the same face, and shipping them apart cost a second
    # 36K-to-88K block that had to be resident all frame.
    raw = lump(3)
    verts = []
    for k in range(0, len(raw), 12):
        vidx = k // 12
        xyz = []
        for axis, coord in zip('xyz', struct.unpack_from('<fff', raw, k)):
            scaled = round(coord * 8)
            # BASIC's build is overflow-unchecked, so a coordinate that
            # did not fit would silently wrap and corrupt geometry with
            # no error. Fail here instead.
            if not (-32768 <= scaled <= 32767):
                raise SystemExit(
                    f"fgeom.bin: vertex {vidx} {axis}={coord} scales to "
                    f"{scaled}, outside signed 16-bit Q13.3 range")
            xyz.append(scaled)
        verts.append(xyz)

    raw = lump(12)
    edges = [struct.unpack_from('<2H', raw, k) for k in range(0, len(raw), 4)]
    raw = lump(13)
    surfedges = [struct.unpack_from('<i', raw, k)[0]
                 for k in range(0, len(raw), 4)]

    rows = [bytearray()]
    fgeom = []                                  # (row, offset) per face
    raw = lump(7)
    for k in range(0, len(raw), 20):
        firstedge, = struct.unpack_from('<i', raw, k + 4)
        nvtx, = struct.unpack_from('<h', raw, k + 8)
        # 33 corners is the standing contract, and d_faces.c's MAXV is
        # derived from it; a face past it runs off every per-face array.
        if not (0 < nvtx <= GEOM_MAXVTX):
            raise SystemExit(
                f"fgeom.bin: face {k // 20} has {nvtx} corners, and "
                f"GEOM_MAXVTX is {GEOM_MAXVTX}")
        fi = k // 20
        rec = bytearray(struct.pack('<h', nvtx))
        rec += lmtab[fi * 18:(fi + 1) * 18]
        for e in range(nvtx):
            se = surfedges[firstedge + e]
            rec += struct.pack('<3h', *verts[edges[abs(se)][0 if se >= 0 else 1]])
        if len(rows[-1]) + len(rec) > GEOM_W:
            rows[-1].extend(b'\0' * (GEOM_W - len(rows[-1])))
            rows.append(bytearray())
        fgeom.append((len(rows) - 1, len(rows[-1])))
        rows[-1] += rec
    rows[-1].extend(b'\0' * (GEOM_W - len(rows[-1])))
    out['fgeom.bin'] = b''.join(bytes(r) for r in rows)

    ent_text = lump(0).split(b'\0')[0].decode('latin-1')
    nmodels = lumps[14][1] // 64
    if nmodels > 128:
        raise SystemExit(f"faces.pag: {nmodels} models do not fit Face.side")
    boxes = [struct.unpack_from('<6f', lump(14), m * 64) for m in range(nmodels)]
    # one a skill, chosen at load (trigger_setskill): NOT_EASY 256, NOT_MEDIUM 512,
    # NOT_HARD 1024; nightmare is hard's set
    spawned = [spawn_entities(ent_text, nmodels, boxes, skill, crates) for skill in range(SKILLS)]
    for skill, e in enumerate(spawned):
        out[f'ents{skill}.bin'] = pack_entities(e, nmodels, gravity, crates)
    # Skill filtering happens before spawn functions, so keep one count and
    # origin run per skill even though stock e1m4 happens to use all skills.
    out['air.bin'] = (struct.pack('<4h', *(len(e.air_bubbles) for e in spawned)) +
                      b''.join(struct.pack('<3f', *org)
                               for e in spawned for org in e.air_bubbles) +
                      struct.pack('<4H', *(len(e.flames) for e in spawned)) +
                      b''.join(struct.pack('<3fBB', *org, model, anim_set)
                               for e in spawned for org, model, anim_set in e.flames))

    # marksurfaces: identical either side. models: 64 -> 32, bspfile.bi's
    # Submodel -- the box, hulls 0 and 1, the face run
    out['lface.bld'] = lump(11)
    out['models.bld'] = b''.join(struct.pack('<6f4h', *struct.unpack_from('<6f', lump(14), m * 64),
                                             *struct.unpack_from('<2i', lump(14), m * 64 + 36),
                                             *struct.unpack_from('<2i', lump(14), m * 64 + 56))
                                 for m in range(nmodels))
    # raw, not BLOAD: read by fileReadH into a memAlloc block, which puts
    # it in upper memory rather than the far heap. It is walked by a PEEK
    # loop over a byte offset and nothing indexes it as an array.
    out['pvs.bin'] = lump(4)

    # texinfo: texinfo(40) -> texinfo2(34). flags is dropped (nothing reads
    # it -- see bspfile.bi's texinfo2 comment) and miptex narrows to an
    # integer (it indexes this map's own texture list, max seen is 72).
    raw = lump(6)
    buf = bytearray()
    for k in range(0, len(raw), 40):
        tidx = k // 40
        vals = struct.unpack_from('<8fii', raw, k)
        miptex = vals[8]
        if not (-32768 <= miptex <= 32767):
            raise SystemExit(
                f"texinf.bld: texinfo {tidx} miptex={miptex}, outside "
                f"signed 16-bit range")
        buf += struct.pack('<8fh', *vals[:8], miptex)
    out['texinf.bld'] = bytes(buf)
    # clipnodes: the collision hulls. planenum narrowed long->integer (see
    # bspfile.bi's clipnode/cliptmp comment): 8 bytes on disk, 6 in memory.
    raw = lump(9)
    out['clip.pag'] = b''.join(
        struct.pack('<hhh', struct.unpack_from('<i', raw, k)[0], *struct.unpack_from('<hh', raw, k+4))
        for k in range(0, len(raw), 8))

    # faces: face(20) -> face2(8), dropping the two flag words and the
    # LIGHTING-lump offset (unused: d_surf.bas's lm_info replaced it).
    #
    # firstedge/numedges are replaced by the face's address in fgeom.bin:
    # the row uglMapEx maps, and the byte offset of its record inside it.
    # The spare high bits of the three bounded indices carry the cache's
    # eleven-bit null head. They become mutable only after loading.
    raw = lump(7)
    buf = bytearray()
    for k in range(0, len(raw), 20):
        planeid, side, _le, _ln, texinfoid, _f1, _f2, _lightmap = \
            struct.unpack_from('<hhihhhhi', raw, k)
        grow, gofs = fgeom[k // 20]
        if not (0 <= planeid < 8192):
            raise SystemExit(f"faces.pag: plane {planeid} does not fit 13 bits")
        if not (0 <= gofs < 16384):
            raise SystemExit(f"faces.pag: geometry offset {gofs} does not fit 14 bits")
        if not (0 <= texinfoid < 1024):
            raise SystemExit(f"faces.pag: texinfo {texinfoid} does not fit 10 bits")
        if not (0 <= grow <= 255):
            raise SystemExit(f"faces.pag: geometry row {grow} does not fit Face.geom_row")
        if k // 20 >= 8192:
            raise SystemExit("faces.pag: face count does not fit the surface-cache owner field")
        null_head = 0x07FF
        packed_plane = planeid | (null_head & 0x0007) << 13
        packed_geom = gofs | (null_head & 0x0018) << 11
        packed_texinfo = texinfoid | (null_head & 0x07E0) << 5
        buf += struct.pack('<HHHBB', packed_plane, packed_geom, packed_texinfo, grow, side)
    out['faces.pag'] = bytes(buf)

    # leaves: leaf(28) -> Leaf(16), dropping the trailing 4 bytes,
    # narrowing cont to an integer (always one of six small CONTENTS_*
    # negatives -- see bspfile.bi's Leaf comment) and the box to bytes
    raw = lump(10)
    buf = bytearray()
    for k in range(0, len(raw), 28):
        cont, vislist = struct.unpack_from('<ii', raw, k)
        bound = bound_bytes(raw[k+8:k+20])
        lfaceid, lfacenum = struct.unpack_from('<hh', raw, k+20)
        # S_UpdateAmbientSounds' water and sky levels, a nibble each
        amb = (min(15, (raw[k + 24] + 8) // 17) << 4) | min(15, (raw[k + 25] + 8) // 17)
        buf += struct.pack('<bB', cont, amb) + struct.pack('<i', vislist) + bound + struct.pack('<hh', lfaceid, lfacenum)
    out['leaves.pag'] = bytes(buf)

    # planes: plane(20) -> Plane(16), the type dropped -- nothing reads it
    raw = lump(1)
    buf = bytearray()
    for k in range(0, len(raw), 20):
        buf += raw[k:k + 16]
    out['planes.bld'] = bytes(buf)

    # nodes: node(24) -> Node(16). planeid narrows, the box packs, and it
    # moves to the end -- the two structures are not in the same order.
    raw = lump(5)
    buf = bytearray()
    for k in range(0, len(raw), 24):
        planeid, child0, child1 = struct.unpack_from('<ihh', raw, k)
        bound = bound_bytes(raw[k+8:k+20])
        lfaceid, lfacenum = struct.unpack_from('<hh', raw, k+20)
        buf += struct.pack('<hhhhh', planeid, child0, child1, lfaceid, lfacenum) + bound
    out['nodes.pag'] = bytes(buf)

    total = 0
    for name, payload in out.items():
        path = os.path.join(outdir, name)
        if name.endswith('.pag'):
            if name in FLAT:
                total += write_flat(path, payload)
            else:
                # page-padded: read a page at a time into an EMS window
                total += write_paged(path, payload, PAGED_ELEM[name])
        elif name.endswith('.bin'):
            # raw: read through qglFileRead into a block or straight into
            # a mapped EMS window, so none of them is bound by BLOAD's
            # 64K cap or by BASIC's far heap
            OUT[name] = bytes(payload)
            total += len(payload)
        else:
            total += write_bload(path, payload)
        print(f"  {name:12} {len(payload):7,} bytes")
    print(f"  lumps total  {total:7,} bytes")


def main():
    if len(sys.argv) < 4:
        raise SystemExit("usage: mkassets.py <map.bsp> <base.dat> <outdir> [pak0.pak: the pickups' b_*.bsp]")
    bsp, packpath, outdir = sys.argv[1], sys.argv[2], sys.argv[3]
    pak = sys.argv[4] if len(sys.argv) > 4 else ''
    os.makedirs(outdir, exist_ok=True)
    d   = open(bsp, 'rb').read()
    pal = load_palette(pack_read(packpath, 'color/palette.lmp'))

    # texLoadAll read every miptex byte as colmap[byte], not as a palette index
    # directly -- row 0 of the colormap, its brightest level. In this data set
    # that row is nowhere near the identity (221 of 256 entries differ), so
    # skipping it shifts almost every texel to the wrong colour. Apply it here
    # exactly where the original applied it: on the way in.
    cmap = pack_read(packpath, 'color/colormap.lmp')
    if len(cmap) < 256:
        raise SystemExit(f"colormap is {len(cmap)} bytes, expected >= 256")
    shade0 = cmap[:256]
    # The surface builder's table is 64 shades of 256, as colormap.lmp is,
    # but shaded in OKLCH along each ramp rather than id's. Row 0 above
    # stays id's: it is the unlit atlas, not a shade.
    OUT['colmap.bin'] = oklch_colormap(pack_read(packpath, 'color/palette.lmp'))
    # qgl links a zip driver and no PACK driver, so the font travels in
    # assets.zip; the BASIC build reads it straight out of base.dat.
    OUT['font.fnt'] = pack_read(packpath, 'font/4x6.fnt')
    print(f"  colmap.bin    {64*256:7,} bytes  (64 shades x 256)")

    lumps   = read_lumps(d)
    toff, _ = lumps[2]
    ntex    = struct.unpack_from('<i', d, toff)[0]
    offs    = [struct.unpack_from('<i', d, toff + 4 + 4*k)[0] for k in range(ntex)]
    # the pickups' b_*.bsp boxes: their textures are cells after the map's
    crates  = CrateSet(load_crates(pak), ntex)
    print("converting lumps ...", flush=True)
    # world.qc: sv_gravity 100 on e1m8, 800 everywhere else
    gravity = 100.0 if os.path.basename(bsp).lower() == 'e1m8.bsp' else 800.0
    convert_lumps(d, lumps, outdir, gravity, crates)
    cells = [(d, toff + o) if o >= 0 else None for o in offs] + crates.cells()
    ntex  = len(cells)
    if ntex * MIPS > 1024:
        raise SystemExit(f"{ntex} textures with the pickups': q_map.bi's ofs(1023) holds 256")
    print(f"  crates: {len(crates.used)} models, {len(crates.tex)} textures after the map's {len(offs)}")

    # What the runtime used to read out of the .bsp itself, so it no
    # longer has to open it: the lump COUNTS, and the miptex headers,
    # whose NAMES are what say which texture is a liquid and which is
    # one frame of an animation. bsphdr.h's DiskMipTex, 40 bytes, one
    # per texture the map owns -- the crates' cells come after those in
    # the atlas and have no header here. A texture the map lists with a
    # -1 offset has none on disk either; it ships as zeros, which is a
    # blank name, which is neither a liquid nor a chain.
    OUT['miptex.bin'] = b''.join(
        d[toff + o : toff + o + 40] if o >= 0 else bytes(40) for o in offs)
    # mod.h's QmapCounts, in its field order. The divisors are
    # bsphdr.h's DISK*_SIZE, and they are the same fact twice by
    # necessity -- one side writes the count, the other only reads it.
    OUT['counts.bin'] = struct.pack(
        '<11lh',
        lumps[7][1] // 20,      # faces      DISKFACE_SIZE
        lumps[3][1] // 12,      # verts      DISKVERTEX_SIZE
        lumps[12][1] // 4,      # edges      DISKEDGE_SIZE
        lumps[13][1] // 4,      # ledges     DISKLEDGE_SIZE
        lumps[10][1] // 28,     # leaves     DISKLEAF_SIZE
        lumps[1][1] // 20,      # planes     DISKPLANE_SIZE
        lumps[5][1] // 24,      # nodes      DISKNODE_SIZE
        lumps[6][1] // 40,      # tex_infos  DISKTEXINFO_SIZE
        lumps[9][1] // 8,       # clips      DISKCLIPNODE_SIZE
        len(offs),              # textures, the map's own
        lumps[11][1],           # face_lump_bytes
        lumps[14][1] // 64 )    # models     DISKSUBMODEL_SIZE

    # ------------------------------------------------------------------
    # Two atlases and a lookup table, not 648 dcs.
    #
    # Each texture used to become its own EMS dc, four mips times two
    # variants: 160 dcs on dm3ish at 264 bytes of CONVENTIONAL memory each
    # for the struct and scanline table, measured at 42,176. e1m1 would
    # make 648 -- the ~171K that stops it loading. One store per variant
    # instead, with the renderer making one VIEW per cell HEIGHT and
    # re-shaping it per face.
    #
    # Cell-major, every cell a self-aligned power-of-two run, packed
    # largest first -- so an offset is always a multiple of its own size
    # and no cell can straddle the 16K window a view maps (uglview.asm).
    # With the sizes descending that alignment is free: each area divides
    # the one before it.
    levels = []
    for cl in cells:
        if cl is None:
            levels.append([(8, 8)] * MIPS)
            continue
        buf, base = cl
        w, h = struct.unpack_from('<ii', buf, base + 16)
        levels.append(tex_cell_levels(w, h))

    def shared(k, lvl):
        return lvl > 0 and levels[k][lvl] == levels[k][lvl - 1]

    blocks = [(cw * ch, k, lvl)
              for k in range(ntex)
              for lvl, (cw, ch) in enumerate(levels[k]) if not shared(k, lvl)]
    blocks.sort(key=lambda b: -b[0])

    place, pos = [[0] * MIPS for _ in range(ntex)], 0
    for area, k, lvl in blocks:
        assert pos % area == 0, "cell not self-aligned"
        place[k][lvl] = pos
        pos += area
    for k in range(ntex):
        for lvl in range(1, MIPS):
            if shared(k, lvl):
                place[k][lvl] = place[k][lvl - 1]
    assert pos < (1 << OFS_BITS), f"atlas {pos} bytes, past what the table can say"

    raw_at, shd_at = bytearray(pos), bytearray(pos)
    for k, cl in enumerate(cells):
        for lvl in range(MIPS):
            if shared(k, lvl):
                continue
            cw, ch = levels[k][lvl]
            o = place[k][lvl]
            if cl is None:
                continue                       # already zeros
            buf, base = cl
            w, h = struct.unpack_from('<ii', buf, base + 16)
            mo   = struct.unpack_from('<i',  buf, base + 24 + 4*lvl)[0]
            mw, mh = w >> lvl, h >> lvl
            src  = buf[base+mo : base+mo + mw*mh]
            if len(src) < mw*mh:
                print(f"  ! texture {k} mip {lvl} truncated, left blank")
                continue
            # raw: indices, for the surface builder, which shades through the
            # full colormap itself. shaded: row 0 applied, for the unlit path.
            lit = bytes(shade0[b] for b in src)
            # A cell at the texture's own size is a COPY: a round trip through
            # RGB and the palette would move indices the atlas is meant to
            # carry exactly. Only a halved or rounded-down cell is resampled.
            if (mw, mh) == (cw, ch):
                raw_at[o:o+cw*ch] = src
                shd_at[o:o+cw*ch] = lit
            else:
                raw_at[o:o+cw*ch] = texquant.resample_indices(src, mw, mh, cw, ch, pal)
                shd_at[o:o+cw*ch] = texquant.resample_indices(lit, mw, mh, cw, ch, pal)

    OUT.update(sky_layers(cells[:len(offs)], shade0, pal))

    exact = sum(1 for k, cl in enumerate(cells)
                if cl is not None and
                levels[k][0] == tuple(struct.unpack_from('<ii', cl[0], cl[1] + 16)))
    print(f"  atlas: {pos:,} bytes a variant, {exact}/{ntex} textures at native size")

    for at in (raw_at, shd_at):
        if len(at) % LM_ATLAS_W:
            at += bytes(LM_ATLAS_W - (len(at) % LM_ATLAS_W))
    rows = len(raw_at) // LM_ATLAS_W
    # Loose and raw, beside the exe, for the same reason lm.bin is raw --
    # and loose rather than zipped because qgl maps them into EMS a page
    # at a time, which a deflated member cannot serve.
    for name, at in (("TEXR.RAW", raw_at), ("TEXS.RAW", shd_at)):
        with open(os.path.join(outdir, name), "wb") as fh:
            fh.write(bytes(at))
        print(f"  {name}: {LM_ATLAS_W}x{rows} = {len(at):,} bytes")

    # The same bytes again, flat and beside the exe rather than in the zip,
    # because qgl reads them: qglSfLoad is a raw blob straight into an EMS
    # surface's store and file.asm is plain INT 21h, which cannot see inside
    # assets.zip. Same delivery FONT.FNT and SOLDIER.GEO already use -- the
    # zip is for what mgl loads.
    #
    # Truncated to exactly rows*LM_ATLAS_W: qglSfLoad reads y_res rows of
    # x_res and fails the load if any row runs short, so the file has to be
    # the surface's own size and not a byte more.
    qmap_texr = qmap_texs = b""
    for name, at in (("texr.raw", raw_at), ("texs.raw", shd_at)):
        payload = bytes(at)[: rows * LM_ATLAS_W]
        open(os.path.join(outdir, name), "wb").write(payload)
        if name == "texr.raw": qmap_texr = payload
        else:                  qmap_texs = payload
        print(f"  {name}: {LM_ATLAS_W}x{rows} = {len(payload):,} bytes (flat, unzipped)")

    # The palette too, flat: screen.bas picks the HUD colours out of it and
    # writes it into screenshots, and reads it with a plain OPEN.
    pal_raw = pack_read(packpath, 'color/palette.lmp')[:768]
    open(os.path.join(outdir, "pal.raw"), "wb").write(pal_raw)
    qmap_pal = pal_raw
    print(f"  pal.raw: {len(pal_raw)} bytes (flat, unzipped)")

    # [id*4 + lvl] -> the cell's byte offset in bits 0..22, then log2 of
    # its width in 23..26 and of its height in 27..30. Bit 31 stays clear
    # so BASIC's signed long arithmetic reads it like any other positive
    # number -- `p \ 65536` truncating toward zero has cost this project a
    # day before now.
    #
    # In the entry rather than beside it because the dims are wanted at
    # exactly the sites that already read the offset, and a second table
    # is 1K of near data on the C side and 2K on BASIC's, which is the
    # memory neither has. The offset is still EMITTED, not re-derived.
    tbl = bytearray()
    for k in range(ntex):
        for lvl in range(MIPS):
            cw, ch = levels[k][lvl]
            tbl += struct.pack('<l', place[k][lvl]
                               | (cw.bit_length() - 1) << OFS_BITS
                               | (ch.bit_length() - 1) << (OFS_BITS + 4))
    write_bload(os.path.join(outdir, "texofs.bld"), bytes(tbl))
    print(f"  texofs.bld: {ntex} x {MIPS} cells, offset and dims")
    written = MIPS * 2

    print(f"done: {written} atlases for {ntex} textures across {MIPS} mip levels")

    # Portals, rebuilt from the tree -- tools/mkportals.py says why -- held to
    # its PVS-subset check here too, so a map whose portals do not cover the
    # PVS fails the build rather than culling what it should have drawn.
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    import mkportals
    pb = mkportals.read_bsp(bsp)
    portals = mkportals.build_portals(pb)
    checked, bad = mkportals.pvs_subset_check(pb, portals)
    if bad:
        # e1m4: 152 of 1230. Such a map ships an index of zeros -- a ref count
        # of 0 -- and r_load_portals leaves pt_ok 0, so the walk reads the PVS
        # alone. Its table would have passed the runtime's 4,681 anyway.
        print(f"  portals do not cover the PVS: {bad} of {checked} leaves; shipped without, the PVS alone draws")
        OUT['portalidx.bld'] = bytes(2 * (len(pb.leaves) + 1))
    else:
        OUT['portalidx.bld'], OUT['portalref.bld'] = mkportals.portal_lumps(pb, portals)
        print(f"  portals: {len(OUT['portalref.bld'])//14} refs over {len(pb.leaves)} leaves")

    write_zip(os.path.join(outdir, 'assets.zip'))

    # And the same bytes as ONE file. The zip and the three loose ones
    # stay for the BASIC build, which still stages five things; cport
    # reads this and nothing else.
    stem = os.path.splitext(os.path.basename(bsp))[0]
    # Only the kinds the map actually spawns, read back out of the
    # ents.bin just written rather than kept on the side: the file is
    # what the runtime loads, so it is what decides which models ship.
    head = struct.calcsize('<3ff3fff')
    ent_blobs = [OUT[f'ents{skill}.bin'] for skill in range(SKILLS)]
    kinds = {struct.unpack_from('<h', ents, struct.calcsize(ENTS_HEAD) + i * struct.calcsize(ENTS_MON))[0]
             for ents in ent_blobs
             for i in range(struct.unpack_from('<13h', ents, head)[7])}
    # Chthon at any skill, as the kinds are: e1m7's monster_boss has no spawnflags
    boss = b'"monster_boss"' in d[lumps[0][0]:lumps[0][0] + lumps[0][1]]
    models = build_models(pak, kinds, boss, os.path.join(outdir, '.mdl'))
    pickup_models = build_item_models(pak, item_models(ent_blobs, kinds), os.path.join(outdir, '.itemmdl'))
    effect_models = build_effect_models(pak, os.path.join(outdir, ".effectmdl"))
    sprites = {"sprites.bin": mksprite.build_sprite_bank(open(pak, "rb").read())} if pak else {}
    sbar = build_sbar(pak, os.path.join(outdir, '.gfx'))
    sounds = build_sounds(pak, os.path.join(outdir, '.snd'))
    write_qmap(os.path.join(outdir, stem + '.qmp'), stem, d,
               dict(OUT, **models, **pickup_models, **effect_models, **sprites, **sbar, **sounds,
                    **{'texr.raw': qmap_texr, 'texs.raw': qmap_texs, 'pal.raw': qmap_pal}))


if __name__ == "__main__":
    main()
