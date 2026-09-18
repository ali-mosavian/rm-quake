"""The monster animation slots shared by the asset packer and demo converter."""

from __future__ import annotations

import re


MDL_NSETS = 16

# Runtime slot order from mdltypes.h. A dash is an intentionally empty slot;
# every other name must exist in the original model.
MONSTER_MODELS = (
    ("soldier", "stand,run,death,pain,shoot,prowl_,deathc,painb,painc"),
    ("knight", "stand,runb,death,pain,"
     "attackb1+attackb2+attackb3+attackb4+attackb5+attackb6+attackb7+attackb8+attackb9+attackb10,"
     "walk,deathb,painb,-,-,-,runattack"),
    ("dog", "stand,run,death,pain,attack,walk,deathb,painb,-,-,-,-,-,leap"),
    ("ogre", "stand,run,death,pain,shoot1+shoot2+shoot2+shoot3+shoot4+shoot5+shoot6,"
     "walk,bdeath,painb,painc,paind,paine,swing,smash"),
    ("demon", "stand,run,death,pain,attacka,walk,-,-,-,-,-,-,-,leap"),
    ("zombie", "stand,run,-,paina,atta,walk,-,painb,painc,paind,paine,"
     "attb1+attb2+attb3+attb4+attb5+attb6+attb7+attb8+attb9+attb10+attb11+attb12+attb13+attb13,"
     "attc,-,cruc_"),
    ("wizard", "hover:8,fly,death,pain,"
     "magatt1+magatt2+magatt3+magatt4+magatt5+magatt6+magatt5+magatt4+magatt3+magatt2,hover:8"),
    ("shambler", "stand,run,death,pain,"
     "magic1+magic2+magic3+magic4+magic5+magic6+magic9+magic10+magic11+magic12,"
     "walk,-,-,-,-,-,smash,swingr,-,-,swingl"),
)

MDL_SETS = {kind: model for kind, model in enumerate(MONSTER_MODELS)}
DEMO_MONSTERS = {name: (kind, spec) for kind, (name, spec) in MDL_SETS.items()}


def parse_sets(spec: str | None) -> list[tuple[str, int]]:
    parts = spec.split(",") if spec is not None else ["stand", "walk", "run"]
    if len(parts) > MDL_NSETS:
        raise ValueError(f"{len(parts)} animation sets; header holds {MDL_NSETS}")
    result = []
    for part in parts:
        name, colon, limit = part.partition(":")
        if not name or (colon and (not limit.isdigit() or int(limit) < 1)):
            raise ValueError(f"invalid animation set {part!r}")
        result.append((name, int(limit) if colon else 0))
    return result


def set_indices(names: list[str], name: str, limit: int = 0) -> list[int]:
    if name == "-":
        if limit:
            raise ValueError("an empty animation slot cannot have a limit")
        return []
    if "+" in name:
        if limit:
            raise ValueError("an exact animation sequence cannot have a limit")
        matches = []
        for frame in name.split("+"):
            try:
                matches.append(names.index(frame))
            except ValueError as error:
                raise ValueError(f"animation frame {frame!r} is absent") from error
        return matches
    if name == "*":
        matches = list(range(len(names)))
    else:
        matches = [i for i, frame in enumerate(names) if re.sub(r"\d+$", "", frame) == name]
        if not matches:
            raise ValueError(f"animation set {name!r} is absent")
        seen = set()
        matches = [i for i in matches if not (names[i] in seen or seen.add(names[i]))]
    if limit > len(matches):
        raise ValueError(f"animation set {name!r} has {len(matches)} frames, not {limit}")
    return matches[:limit] if limit else matches


def select_indices(names: list[str], spec: str | None) -> tuple[list[int], list[int]]:
    groups = [set_indices(names, name, limit) for name, limit in parse_sets(spec)]
    return [index for group in groups for index in group], [len(group) for group in groups]
