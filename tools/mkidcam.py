"""mkidcam.py PAK demos/demo.dem out.qdc -- extract id demo client snapshots."""

from dataclasses import dataclass, field
from pathlib import Path
import re
import struct
import sys

from pakget import member
from mksnd import SOUNDS


U_MOREBITS = 1 << 0
U_ORIGIN1 = 1 << 1
U_ORIGIN2 = 1 << 2
U_ORIGIN3 = 1 << 3
U_ANGLE2 = 1 << 4
U_FRAME = 1 << 6
U_ANGLE1 = 1 << 8
U_ANGLE3 = 1 << 9
U_MODEL = 1 << 10
U_COLORMAP = 1 << 11
U_SKIN = 1 << 12
U_EFFECTS = 1 << 13
U_LONGENTITY = 1 << 14

SU_VIEWHEIGHT = 1 << 0
SU_IDEALPITCH = 1 << 1
SU_PUNCH1 = 1 << 2
SU_VELOCITY1 = 1 << 5
SU_WEAPONFRAME = 1 << 12
SU_ARMOR = 1 << 13
SU_WEAPON = 1 << 14

ENT_BRUSH = 1
ENT_MONSTER = 2
ENT_ITEM = 3
ENT_LIGHT = 4
ENT_SOUND = 5
ENT_DAMAGE = 6
ENT_MUZZLE = 7
ENT_STOP_SOUND = 8
ENT_PROJECTILE = 9

EF_MUZZLEFLASH = 2
LOCAL_SOUNDS = {name.lower(): index for index, name in enumerate(SOUNDS)}
PROJECTILES = {
    "spike": 0, "s_spike": 1, "grenade": 2, "missile": 3,
    "lavaball": 4, "w_spike": 5,
    "h_zombie": 6, "zom_gib": 6, "gib1": 6, "gib2": 6, "gib3": 6,
}

MONSTERS = {
    "soldier": (0, "stand,run,death,pain"),
    "knight": (1, "stand,runb,death,pain,attackb"),
    "dog": (2, "stand:1,run,death,pain:1"),
    "ogre": (3, "stand:1,run,death,pain:3,shoot"),
    "demon": (4, "stand:1,run,death,pain:3,attacka"),
    "zombie": (5, "stand:1,run:8,death,paina:8,atta"),
    "wizard": (6, "hover,fly,death,pain,magatt"),
    "shambler": (7, "stand:1,run,death,pain,magic"),
}

ITEMS = {
    "health": 0,
    "b_bh10": 0,
    "b_bh25": 0,
    "b_bh100": 0,
    "b_shell0": 1,
    "b_shell1": 1,
    "armor": 2,
    "armor1": 3,
    "g_shot2": 4,
    "g_shot": 1,
    "b_nail0": 5,
    "b_nail1": 5,
    "g_nail": 6,
    "quaddama": 7,
    "suit": 8,
    "b_explob": 9,
    "w_s_key": 10,
    "m_s_key": 10,
    "w_g_key": 11,
    "m_g_key": 11,
    "g_rock": 12,
    "b_rock0": 13,
    "b_rock1": 13,
    "g_nail2": 14,
    "g_rock2": 15,
    "invulner": 16,
    "end1": 17,
    "armor2": 18,
    "b_batt0": 19,
    "b_batt1": 19,
    "invisibl": 20,
    "backpack": 1,
}


@dataclass(slots=True)
class NetEntity:
    model: int = 0
    frame: int = 0
    skin: int = 0
    effects: int = 0
    origin: tuple[float, float, float] = (0.0, 0.0, 0.0)
    angles: tuple[float, float, float] = (0.0, 0.0, 0.0)


@dataclass(slots=True)
class Reader:
    data: bytes
    pos: int = 0

    def take(self, size: int) -> bytes:
        end = self.pos + size
        if end > len(self.data):
            raise ValueError(f"short message at byte {self.pos}")
        value = self.data[self.pos : end]
        self.pos = end
        return value

    def byte(self) -> int:
        return self.take(1)[0]

    def char(self) -> int:
        return struct.unpack("<b", self.take(1))[0]

    def short(self) -> int:
        return struct.unpack("<h", self.take(2))[0]

    def long(self) -> int:
        return struct.unpack("<i", self.take(4))[0]

    def float(self) -> float:
        return struct.unpack("<f", self.take(4))[0]

    def string(self) -> str:
        end = self.data.find(b"\0", self.pos)
        if end < 0:
            raise ValueError(f"unterminated string at byte {self.pos}")
        value = self.data[self.pos : end].decode("latin1")
        self.pos = end + 1
        return value


@dataclass(slots=True)
class DemoState:
    map_name: str = ""
    view_entity: int = 0
    view_height: int = 22
    server_time: float | None = None
    models: list[str] = field(default_factory=list)
    sounds: list[str] = field(default_factory=list)
    baselines: dict[int, NetEntity] = field(default_factory=dict)
    entities: dict[int, NetEntity] = field(default_factory=dict)
    seen: set[int] = field(default_factory=set)
    items: int = 0
    health: int = 100
    armor: int = 0
    weapon: int = 1
    weapon_frame: int = 0
    weapon_model: int = 0
    shells: int = 25
    nails: int = 0
    rockets: int = 0
    cells: int = 0
    lightstyles: dict[int, str] = field(default_factory=dict)
    events: list[tuple[float, ...]] = field(default_factory=list)


def read_coord(reader: Reader) -> float:
    return reader.short() * 0.125


def read_baseline(reader: Reader) -> NetEntity:
    model = reader.byte()
    frame = reader.byte()
    reader.byte()
    skin = reader.byte()
    origin = []
    angles = []
    for _ in range(3):
        origin.append(read_coord(reader))
        angles.append(reader.byte() * 360.0 / 256.0)
    return NetEntity(model, frame, skin, 0, tuple(origin), tuple(angles))


def parse_serverinfo(reader: Reader, state: DemoState) -> None:
    protocol = reader.long()
    if protocol != 15:
        raise ValueError(f"protocol {protocol}, want 15")
    reader.take(2)
    reader.string()
    models = [""]
    while name := reader.string():
        models.append(name)
    sounds = [""]
    while name := reader.string():
        sounds.append(name)
    if len(models) < 2 or not models[1].startswith("maps/") or not models[1].endswith(".bsp"):
        raise ValueError("serverinfo has no world model")
    state.map_name = Path(models[1]).stem
    state.models = models
    state.sounds = sounds
    state.view_height = 22
    state.baselines.clear()
    state.entities.clear()
    state.lightstyles.clear()
    state.events.clear()


def parse_clientdata(reader: Reader, state: DemoState) -> None:
    bits = reader.short() & 0xFFFF
    state.view_height = reader.char() if bits & SU_VIEWHEIGHT else 22
    if bits & SU_IDEALPITCH:
        reader.char()
    for axis in range(3):
        if bits & (SU_PUNCH1 << axis):
            reader.char()
        if bits & (SU_VELOCITY1 << axis):
            reader.char()
    state.items = reader.long()
    if bits & SU_WEAPONFRAME:
        state.weapon_frame = reader.byte()
    else:
        state.weapon_frame = 0
    if bits & SU_ARMOR:
        state.armor = reader.byte()
    else:
        state.armor = 0
    if bits & SU_WEAPON:
        state.weapon_model = reader.byte()
    else:
        state.weapon_model = 0
    state.health = reader.short()
    reader.byte()
    state.shells = reader.byte()
    state.nails = reader.byte()
    state.rockets = reader.byte()
    state.cells = reader.byte()
    state.weapon = reader.byte()


def parse_update(reader: Reader, state: DemoState, cmd: int) -> None:
    bits = cmd & 0x7F
    if bits & U_MOREBITS:
        bits |= reader.byte() << 8
    entity = reader.short() if bits & U_LONGENTITY else reader.byte()
    baseline = state.baselines.get(entity, NetEntity())
    model = reader.byte() if bits & U_MODEL else baseline.model
    frame = reader.byte() if bits & U_FRAME else baseline.frame
    if bits & U_COLORMAP:
        reader.byte()
    skin = reader.byte() if bits & U_SKIN else baseline.skin
    effects = reader.byte() if bits & U_EFFECTS else baseline.effects
    origin = []
    angles = []
    origin_bits = (U_ORIGIN1, U_ORIGIN2, U_ORIGIN3)
    angle_bits = (U_ANGLE1, U_ANGLE2, U_ANGLE3)
    for axis in range(3):
        origin.append(read_coord(reader) if bits & origin_bits[axis] else baseline.origin[axis])
        angles.append(reader.byte() * 360.0 / 256.0 if bits & angle_bits[axis] else baseline.angles[axis])
    state.entities[entity] = NetEntity(model, frame, skin, effects, tuple(origin), tuple(angles))
    state.seen.add(entity)
    if effects & EF_MUZZLEFLASH:
        number = 0 if entity == state.view_entity else entity
        state.events.append((number, ENT_MUZZLE, 0, 0, 0, *origin, angles[1]))


def local_sound(name: str) -> int | None:
    stem = name.lower().removesuffix(".wav")
    return LOCAL_SOUNDS.get(stem)


def parse_sound(reader: Reader, state: DemoState) -> None:
    mask = reader.byte()
    volume = reader.byte() if mask & 1 else 255
    attenuation = reader.byte() if mask & 2 else 64
    packed = reader.short() & 0xFFFF
    sound = reader.byte()
    origin = tuple(read_coord(reader) for _ in range(3))
    if sound >= len(state.sounds):
        return
    entity = packed >> 3
    channel = packed & 7
    number = 0 if entity == state.view_entity else entity
    if state.sounds[sound].lower().removesuffix(".wav") == "misc/null":
        state.events.append((number, ENT_STOP_SOUND, 0, channel, 0,
                             0.0, 0.0, 0.0, 0.0))
        return
    if (sound_id := local_sound(state.sounds[sound])) is None:
        return
    attn = max(0, min(3, (attenuation + 32) // 64))
    state.events.append((number, ENT_SOUND, sound_id, channel | attn << 3,
                         volume, *origin, 0.0))


def parse_damage(reader: Reader, state: DemoState) -> None:
    armor = reader.byte()
    blood = reader.byte()
    origin = tuple(read_coord(reader) for _ in range(3))
    state.events.append((armor, ENT_DAMAGE, blood, 0, 0, *origin, 0.0))


def parse_temp_entity(reader: Reader, state: DemoState) -> None:
    kind = reader.byte()
    if kind in (5, 6, 9, 13):
        reader.take(14)
        return
    origin = tuple(read_coord(reader) for _ in range(3))
    if kind == 12:
        reader.take(2)
    sound_id = {0: 141, 1: 141, 3: 3, 4: 3, 7: 160, 8: 29, 12: 3}.get(kind)
    if sound_id is not None:
        state.events.append((-1, ENT_SOUND, sound_id, 1 << 3, 255, *origin, 0.0))


def parse_packet(data: bytes, state: DemoState) -> bool:
    reader = Reader(data)
    timed = False
    state.seen.clear()
    while reader.pos < len(data):
        cmd = reader.byte()
        if cmd & 0x80:
            parse_update(reader, state, cmd)
            continue
        match cmd:
            case 0:
                raise ValueError(f"svc_bad at byte {reader.pos - 1}")
            case 1 | 2 | 27 | 28 | 30 | 33:
                pass
            case 3:
                reader.take(5)
            case 4:
                reader.long()
            case 5:
                state.view_entity = reader.short()
            case 6:
                parse_sound(reader, state)
            case 7:
                state.server_time = reader.float()
                timed = True
            case 8 | 9 | 26 | 31 | 34:
                reader.string()
            case 10:
                reader.take(3)
            case 11:
                parse_serverinfo(reader, state)
            case 12:
                style = reader.byte()
                state.lightstyles[style] = reader.string()
            case 13:
                reader.byte()
                reader.string()
            case 14:
                reader.take(3)
            case 15:
                parse_clientdata(reader, state)
            case 16:
                packed = reader.short() & 0xFFFF
                entity = packed >> 3
                state.events.append((0 if entity == state.view_entity else entity,
                                     ENT_STOP_SOUND, 0, packed & 7, 0,
                                     0.0, 0.0, 0.0, 0.0))
            case 17:
                reader.take(2)
            case 18:
                reader.take(11)
            case 19:
                parse_damage(reader, state)
            case 20:
                read_baseline(reader)
            case 21:
                raise ValueError("obsolete svc_spawnbinary")
            case 22:
                entity = reader.short()
                state.baselines[entity] = read_baseline(reader)
            case 23:
                parse_temp_entity(reader, state)
            case 24 | 25:
                reader.byte()
            case 29:
                reader.take(9)
            case 32:
                reader.take(2)
            case _:
                raise ValueError(f"unknown server command {cmd} at byte {reader.pos - 1}")
    return timed


def packets(data: bytes) -> list[tuple[tuple[float, float, float], bytes]]:
    newline = data.find(b"\n")
    if newline < 0:
        raise ValueError("demo has no CD track line")
    reader = Reader(data, newline + 1)
    result = []
    while reader.pos < len(data):
        length = reader.long()
        if length < 0 or length > 65536:
            raise ValueError(f"bad demo packet length {length}")
        angles = struct.unpack("<3f", reader.take(12))
        result.append((angles, reader.take(length)))
    return result


def mdl_frame_names(data: bytes) -> list[str]:
    _ident, _version = struct.unpack_from("<4si", data)
    nskins, skin_w, skin_h, nverts, ntris, nframes = struct.unpack_from("<6i", data, 48)
    pos = 84
    for _ in range(nskins):
        group = struct.unpack_from("<i", data, pos)[0]
        pos += 4
        count = 1
        if group:
            count = struct.unpack_from("<i", data, pos)[0]
            pos += 4 + count * 4
        pos += count * skin_w * skin_h
    pos += nverts * 12 + ntris * 16
    names = []
    for _ in range(nframes):
        grouped = struct.unpack_from("<i", data, pos)[0]
        pos += 4
        count = 1
        if grouped:
            count = struct.unpack_from("<i", data, pos)[0]
            pos += 4 + 16 + count * 4
        for _ in range(count):
            pos += 8
            names.append(data[pos : pos + 16].split(b"\0", 1)[0].decode("latin1"))
            pos += 16 + nverts * 4
    return names


def compact_frame_map(pak: bytes, name: str, spec: str) -> dict[int, int]:
    names = mdl_frame_names(member(pak, f"progs/{name}.mdl"))
    chosen = []
    groups = []
    for part in spec.split(","):
        prefix, _, limit = part.partition(":")
        matches = [i for i, frame in enumerate(names) if re.sub(r"\d+$", "", frame) == prefix]
        kept = matches[: int(limit)] if limit else matches
        groups.append(kept)
        chosen.extend(kept)
    reverse = {original: compact for compact, original in enumerate(chosen)}
    for original, frame in enumerate(names):
        if original in reverse:
            continue
        base = re.sub(r"\d+$", "", frame)
        if "death" in base:
            group = groups[2]
        elif "pain" in base:
            group = groups[3]
        elif base.startswith(("run", "walk", "fly")):
            group = groups[1]
        elif base.startswith(("stand", "idle", "hover")):
            group = groups[0]
        else:
            group = groups[4] if len(groups) > 4 and groups[4] else groups[0]
        same = [i for i, candidate in enumerate(names) if re.sub(r"\d+$", "", candidate) == base]
        ordinal = same.index(original)
        fallback = group[min(ordinal, len(group) - 1)]
        reverse[original] = reverse[fallback]
    return reverse


def local_items(bits: int) -> int:
    result = bits & 7
    if bits & 8:
        result |= 64
    if bits & 16:
        result |= 32
    if bits & 32:
        result |= 128
    if bits & 131072:
        result |= 8
    if bits & 262144:
        result |= 16
    return result


def local_weapon(bit: int) -> int:
    return {1: 1, 2: 2, 4: 4, 8: 64, 16: 32, 32: 128}.get(bit, 1)


def armor_kind(bits: int) -> int:
    if bits & 32768:
        return 3
    if bits & 16384:
        return 2
    if bits & 8192:
        return 1
    return 0


def power_bits(bits: int) -> int:
    return ((1 if bits & 524288 else 0) | (2 if bits & 1048576 else 0) |
            (4 if bits & 2097152 else 0) | (8 if bits & 4194304 else 0))


def item_kind(stem: str, skin: int) -> int:
    return (2, 3, 18)[min(skin, 2)] if stem == "armor" else ITEMS[stem]


def entity_records(state: DemoState, frame_maps: dict[str, dict[int, int]]) -> list[tuple[float, ...]]:
    records = []
    for number in sorted(state.seen):
        entity = state.entities[number]
        if entity.model <= 0 or entity.model >= len(state.models):
            continue
        model = state.models[entity.model]
        stem = Path(model).stem.lower()
        if model.startswith("*"):
            records.append((number, ENT_BRUSH, int(model[1:]), 0, 0, *entity.origin, entity.angles[1]))
        elif stem in MONSTERS:
            kind, _spec = MONSTERS[stem]
            records.append((number, ENT_MONSTER, kind, 6,
                            frame_maps[stem].get(entity.frame, 0), *entity.origin, entity.angles[1]))
        elif stem in ITEMS:
            records.append((number, ENT_ITEM, item_kind(stem, entity.skin),
                            1 if stem == "backpack" else 0, 0, *entity.origin, entity.angles[1]))
        elif stem in PROJECTILES:
            records.append((number, ENT_PROJECTILE, PROJECTILES[stem], 0,
                            entity.frame, *entity.origin, entity.angles[1]))
    for style, pattern in sorted(state.lightstyles.items()):
        if pattern:
            value = (ord(pattern[int((state.server_time or 0.0) * 10) % len(pattern)]) - ord("a")) * 10
            records.append((style, ENT_LIGHT, style, 0, value, 0.0, 0.0, 0.0, 0.0))
    records.extend(state.events)
    state.events.clear()
    return records


def camera_track(pak: bytes, data: bytes) -> tuple[str, list[tuple[tuple[float, ...], list[tuple[float, ...]]]]]:
    state = DemoState()
    records = []
    start_time = None
    frame_maps = {name: compact_frame_map(pak, name, spec) for name, (_kind, spec) in MONSTERS.items()}
    for angles, message in packets(data):
        if not parse_packet(message, state) or state.server_time is None:
            continue
        view = state.entities.get(state.view_entity)
        if view is None:
            continue
        if start_time is None:
            start_time = state.server_time
        time = state.server_time - start_time
        if records and time <= records[-1][0][0]:
            continue
        entities = entity_records(state, frame_maps)
        records.append((
            (
                time,
                view.origin[0] + 1.0 / 32.0,
                view.origin[1] + 1.0 / 32.0,
                view.origin[2] + state.view_height + 1.0 / 32.0,
                angles[0],
                angles[1],
                local_items(state.items),
                state.health,
                state.armor,
                local_weapon(state.weapon),
                state.weapon_frame,
                state.shells,
                state.nails,
                state.rockets,
                state.cells,
                armor_kind(state.items),
                power_bits(state.items),
                state.weapon_model,
                len(entities),
            ),
            entities,
        ))
    if not state.map_name or len(records) < 2:
        raise ValueError("demo produced no camera track")
    return state.map_name, records


def write_track(
    path: Path,
    map_name: str,
    records: list[tuple[tuple[float, ...], list[tuple[float, ...]]]],
) -> None:
    encoded_map = map_name.encode("ascii")
    if len(encoded_map) > 8:
        raise ValueError(f"map name {map_name!r} is too long")
    data = bytearray(b"QDS1" + encoded_map.ljust(9, b"\0"))
    for frame, entities in records:
        data.extend(struct.pack("<6fl12h", *frame))
        for entity in entities:
            data.extend(struct.pack("<5h4f", *entity))
    path.write_bytes(data)


def main() -> None:
    if len(sys.argv) != 4:
        raise SystemExit(__doc__)
    pak = Path(sys.argv[1]).read_bytes()
    demo = member(pak, sys.argv[2])
    map_name, records = camera_track(pak, demo)
    write_track(Path(sys.argv[3]), map_name, records)


if __name__ == "__main__":
    main()
