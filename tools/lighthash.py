"""Search and validate the surface cache's per-face light/animation hashes."""

import math
import re
import struct
import sys
from dataclasses import dataclass


WORLD_PATTERNS = (
    "m",
    "mmnmmommommnonmmonqnmmo",
    "abcdefghijklmnopqrstuvwxyzyxwvutsrqponmlkjihgfedcba",
    "mmmmmaaaaammmmmaaaaaabcdefgabcdefg",
    "mamamamamama",
    "jklmnopqrstuvwxyzyxwvutsrqponmlkj",
    "nmonqnmomnmomomno",
    "mmmaaaabcdefgmmmmaaaammmaamm",
    "mmmaaammmaaammmabcdefaaaammmmabcdefmmmaaaa",
    "aaaaaaaazzzzzzzz",
    "mmamammmmammamamaaamammma",
    "abcdefghijklmnopqrrqponmlkjihgfedcba",
)
ENT_PAIR = re.compile(r'"([^"]*)"\s*"([^"]*)"')


@dataclass(slots=True, frozen=True)
class HashChoice:
    collisions: int
    multiplier: int
    shift: int


def _lump(data: bytes, index: int) -> bytes:
    offset, size = struct.unpack_from("<ll", data, 4 + index * 8)
    return data[offset : offset + size]


def _switch_styles(data: bytes) -> set[int]:
    text = _lump(data, 0).split(b"\0")[0].decode("latin-1")
    result: set[int] = set()
    for block in text.split("{")[1:]:
        values = dict(ENT_PAIR.findall(block.split("}")[0]))
        if "style" in values and (values.get("targetname") or values.get("classname") == "light_fluoro"):
            result.add(int(values["style"]))
    return result


def _key(digits: tuple[int, ...]) -> int:
    key = 0
    place = 1
    for digit in digits:
        key += digit * place
        place *= 27
    return key


def reachable_keys(styles: tuple[int, ...], switch_styles: set[int]) -> frozenset[int]:
    animated = sorted({style for style in styles if 0 < style < len(WORLD_PATTERNS)})
    switched = sorted(style for style in set(styles) & switch_styles if 32 <= style <= 63)
    switch_index = {style: index for index, style in enumerate(switched)}
    period = math.lcm(*(len(WORLD_PATTERNS[style]) for style in animated)) if animated else 1
    keys: set[int] = set()

    for tick in range(period):
        for mask in range(1 << len(switched)):
            digits = []
            for style in styles:
                if style == 0 or style > 63:
                    digit = 12
                elif style in animated:
                    digit = ord(WORLD_PATTERNS[style][tick % len(WORLD_PATTERNS[style])]) - ord("a")
                elif style in switch_index:
                    digit = 12 if mask & (1 << switch_index[style]) else 0
                elif style == 63:
                    digit = 0
                else:
                    digit = 26
                digits.append(digit)
            keys.add(_key(tuple(digits)))

    # A render before the first simulation tick sees switchable styles unset.
    keys.add(
        _key(
            tuple(
                12
                if style == 0 or style > 63
                else ord(WORLD_PATTERNS[style][0]) - ord("a")
                if style in animated
                else 0
                if style == 63
                else 26
                for style in styles
            )
        )
    )
    # -nostyles forces every style to the neutral 'm'.
    keys.add(_key(tuple(12 for _ in styles)))
    return frozenset(keys)


def reachable_surface_keys(
    styles: tuple[int, ...], switch_styles: set[int], animation_frames: int
) -> frozenset[int]:
    return frozenset(
        light_key * animation_frames + frame
        for light_key in reachable_keys(styles, switch_styles)
        for frame in range(animation_frames)
    )


def face_animation_counts(data: bytes) -> tuple[int, ...]:
    miptex = _lump(data, 2)
    texture_count = struct.unpack_from("<l", miptex)[0]
    offsets = struct.unpack_from(f"<{texture_count}l", miptex, 4)
    names = [
        miptex[offset : offset + 16].split(b"\0", 1)[0].decode("latin-1")
        if offset >= 0
        else ""
        for offset in offsets
    ]
    counts = [1] * texture_count
    groups: dict[str, list[int]] = {}
    for index, name in enumerate(names):
        if len(name) >= 2 and name[0] == "+" and name[1].isdigit():
            groups.setdefault(name[2:], []).append(index)
    for indices in groups.values():
        if len(indices) > 1:
            for index in indices:
                counts[index] = len(indices)

    texinfo = _lump(data, 6)
    texids = [struct.unpack_from("<i", texinfo, offset + 32)[0] for offset in range(0, len(texinfo), 40)]
    faces = _lump(data, 7)
    return tuple(counts[texids[struct.unpack_from("<h", faces, offset + 10)[0]]] for offset in range(0, len(faces), 20))


def bsp_light_keys(data: bytes) -> dict[tuple[int, ...], frozenset[int]]:
    faces = _lump(data, 7)
    switch_styles = _switch_styles(data)
    tuples = {
        tuple(style for style in faces[offset + 12 : offset + 16] if style != 255)
        for offset in range(0, len(faces), 20)
    }
    return {styles: reachable_keys(styles, switch_styles) for styles in tuples if styles}


def bsp_surface_keys(data: bytes) -> dict[tuple[tuple[int, ...], int], frozenset[int]]:
    faces = _lump(data, 7)
    animation_counts = face_animation_counts(data)
    switch_styles = _switch_styles(data)
    variants = {
        (
            tuple(style for style in faces[offset + 12 : offset + 16] if style != 255),
            animation_counts[offset // 20],
        )
        for offset in range(0, len(faces), 20)
    }
    return {
        variant: reachable_surface_keys(variant[0], switch_styles, variant[1])
        for variant in variants
        if variant[0]
    }


def cache_hash(key: int, buckets: int, multiplier: int, shift: int) -> int:
    mixed = (key * multiplier) ^ (key >> shift)
    return (mixed & (buckets - 1)) if (buckets & (buckets - 1)) == 0 else mixed % buckets


def best_hash(keys: frozenset[int], buckets: int) -> HashChoice:
    if buckets < 1 or buckets > 512:
        raise ValueError("surface hash bucket count must be 1..512")
    if len(keys) == 1:
        return HashChoice(0, 0, 0)

    lower_bound = max(0, len(keys) - buckets)
    best = HashChoice(len(keys) - 1, 1, 0)
    for multiplier in range(1, 256):
        for shift in range(21):
            collisions = len(keys) - len(
                {cache_hash(key, buckets, multiplier, shift) for key in keys}
            )
            if collisions < best.collisions:
                best = HashChoice(collisions, multiplier, shift)
                if collisions == lower_bound:
                    return best
    return best


def collision_curve(keys: frozenset[int]) -> tuple[HashChoice, ...]:
    return tuple(best_hash(keys, buckets) for buckets in range(1, 65))


def main(paths: list[str]) -> int:
    state_sets: list[tuple[str, tuple[int, ...], frozenset[int]]] = []
    for path in paths:
        data = open(path, "rb").read()
        for styles, keys in bsp_light_keys(data).items():
            state_sets.append((path, styles, keys))
    if not state_sets:
        raise SystemExit("no lit faces")
    print("buckets worst_collisions multiplier shift map styles")
    for buckets in range(1, 65):
        rows = [
            (best_hash(keys, buckets), len(keys), path, styles)
            for path, styles, keys in state_sets
        ]
        choice, _states, path, styles = max(
            rows, key=lambda row: (row[0].collisions, row[1])
        )
        print(
            f"{buckets:7} {choice.collisions:16} {choice.multiplier:10} "
            f"{choice.shift:5} {path} {styles}"
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
