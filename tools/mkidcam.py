"""mkidcam.py PAK demos/demo.dem out.qdc -- extract an id demo camera track."""

from dataclasses import dataclass, field
from pathlib import Path
import struct
import sys

from pakget import member


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
    baselines: dict[int, tuple[float, float, float]] = field(default_factory=dict)
    origins: dict[int, tuple[float, float, float]] = field(default_factory=dict)


def read_coord(reader: Reader) -> float:
    return reader.short() * 0.125


def skip_baseline(reader: Reader) -> tuple[float, float, float]:
    reader.take(4)
    origin = []
    for _ in range(3):
        origin.append(read_coord(reader))
        reader.byte()
    return tuple(origin)


def parse_serverinfo(reader: Reader, state: DemoState) -> None:
    protocol = reader.long()
    if protocol != 15:
        raise ValueError(f"protocol {protocol}, want 15")
    reader.take(2)
    reader.string()
    models = []
    while name := reader.string():
        models.append(name)
    while reader.string():
        pass
    if not models or not models[0].startswith("maps/") or not models[0].endswith(".bsp"):
        raise ValueError("serverinfo has no world model")
    state.map_name = Path(models[0]).stem
    state.view_height = 22
    state.baselines.clear()
    state.origins.clear()


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
    reader.long()
    if bits & SU_WEAPONFRAME:
        reader.byte()
    if bits & SU_ARMOR:
        reader.byte()
    if bits & SU_WEAPON:
        reader.byte()
    reader.take(8)


def parse_update(reader: Reader, state: DemoState, cmd: int) -> None:
    bits = cmd & 0x7F
    if bits & U_MOREBITS:
        bits |= reader.byte() << 8
    entity = reader.short() if bits & U_LONGENTITY else reader.byte()
    for flag in (U_MODEL, U_FRAME, U_COLORMAP, U_SKIN, U_EFFECTS):
        if bits & flag:
            reader.byte()
    baseline = state.baselines.get(entity, (0.0, 0.0, 0.0))
    origin = []
    origin_bits = (U_ORIGIN1, U_ORIGIN2, U_ORIGIN3)
    angle_bits = (U_ANGLE1, U_ANGLE2, U_ANGLE3)
    for axis in range(3):
        origin.append(read_coord(reader) if bits & origin_bits[axis] else baseline[axis])
        if bits & angle_bits[axis]:
            reader.byte()
    state.origins[entity] = tuple(origin)


def skip_temp_entity(reader: Reader) -> None:
    kind = reader.byte()
    if kind in (5, 6, 9, 13):
        reader.take(14)
    elif kind == 12:
        reader.take(8)
    elif 0 <= kind <= 11:
        reader.take(6)
    else:
        raise ValueError(f"unknown temp entity {kind}")


def parse_packet(data: bytes, state: DemoState) -> bool:
    reader = Reader(data)
    timed = False
    while reader.pos < len(data):
        cmd = reader.byte()
        if cmd & 0x80:
            parse_update(reader, state, cmd)
            continue
        match cmd:
            case 0:
                raise ValueError("svc_bad")
            case 1 | 2 | 27 | 28 | 30 | 33:
                pass
            case 3:
                reader.take(5)
            case 4:
                reader.long()
            case 5:
                state.view_entity = reader.short()
            case 6:
                mask = reader.byte()
                reader.take((1 if mask & 1 else 0) + (1 if mask & 2 else 0) + 9)
            case 7:
                state.server_time = reader.float()
                timed = True
            case 8 | 9 | 26 | 31 | 34:
                reader.string()
            case 10:
                reader.take(3)
            case 11:
                parse_serverinfo(reader, state)
            case 12 | 13:
                reader.byte()
                reader.string()
            case 14:
                reader.take(3)
            case 15:
                parse_clientdata(reader, state)
            case 16:
                reader.short()
            case 17:
                reader.take(2)
            case 18:
                reader.take(11)
            case 19:
                reader.take(8)
            case 20:
                skip_baseline(reader)
            case 21:
                raise ValueError("obsolete svc_spawnbinary")
            case 22:
                entity = reader.short()
                state.baselines[entity] = skip_baseline(reader)
            case 23:
                skip_temp_entity(reader)
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


def camera_track(data: bytes) -> tuple[str, list[tuple[float, ...]]]:
    state = DemoState()
    records = []
    start_time = None
    for angles, message in packets(data):
        if not parse_packet(message, state) or state.server_time is None:
            continue
        origin = state.origins.get(state.view_entity)
        if origin is None:
            continue
        if start_time is None:
            start_time = state.server_time
        time = state.server_time - start_time
        if records and time <= records[-1][0]:
            continue
        records.append(
            (
                time,
                origin[0] + 1.0 / 32.0,
                origin[1] + 1.0 / 32.0,
                origin[2] + state.view_height + 1.0 / 32.0,
                angles[0],
                angles[1],
            )
        )
    if not state.map_name or len(records) < 2:
        raise ValueError("demo produced no camera track")
    return state.map_name, records


def write_track(path: Path, map_name: str, records: list[tuple[float, ...]]) -> None:
    encoded_map = map_name.encode("ascii")
    if len(encoded_map) > 8:
        raise ValueError(f"map name {map_name!r} is too long")
    data = bytearray(b"QDC1" + encoded_map.ljust(9, b"\0"))
    for record in records:
        data.extend(struct.pack("<6f", *record))
    path.write_bytes(data)


def main() -> None:
    if len(sys.argv) != 4:
        raise SystemExit(__doc__)
    pak = Path(sys.argv[1]).read_bytes()
    demo = member(pak, sys.argv[2])
    map_name, records = camera_track(demo)
    write_track(Path(sys.argv[3]), map_name, records)


if __name__ == "__main__":
    main()
