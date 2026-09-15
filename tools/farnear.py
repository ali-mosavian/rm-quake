#!/usr/bin/env python3
"""farnear.py -- find a far address handed to a near pointer.

    tools/farnear.py [FILE.c ...]

Medium model: a plain `T *` parameter is near, and passing it the address
of something reached through a far pointer drops the segment without a
word from BC -- the callee then reads DGROUP at that offset. Also flags an
explicit `(T *)` cast of such an address, which hides the same thing.

Crude on purpose: a far expression is `&v->f`, `&v[i].f` or `&v[i]` where
`v` is a local or parameter declared `T far *v`, or `x->f` / `x.f` where
`f` is a struct field declared `T far *f`. Exits 1 when it finds any.
"""

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
FAR_DECL = re.compile(r"\b\w+\s+far\s*\*\s*(\w+)")
FIELD_DECL = re.compile(r"^\s*(?:const\s+)?\w+\s+far\s*\*\s*(\w+)\s*;", re.M)
# column 0 and a type before the name: an indented call matches the same
# shape and would overwrite the definition's parameter kinds
FUNC_DEF = re.compile(
    r"^(?!(?:return|else|if|while|for|switch|do)\b)[A-Za-z_][\w \*]*?[\s\*](\w+)\s*\(([^;{)]*)\)\s*(?:\{|;)", re.M
)
CALL = re.compile(r"\b(\w+)\s*\(")


def sources(pattern: str) -> list[Path]:
    found = sorted(p for p in (ROOT / "src").rglob(pattern) if "test" not in p.relative_to(ROOT).parts)
    if not found:
        sys.exit(f"farnear: no {pattern} under src/, so nothing was checked")
    return found


def far_fields() -> set[str]:
    return {m.group(1) for h in sources("*.h") for m in FIELD_DECL.finditer(h.read_text(errors="replace"))}


def param_kinds() -> dict[str, list[str]]:
    kinds: dict[str, list[str]] = {}
    for f in sources("*.h") + sources("*.c"):
        text = f.read_text(errors="replace")
        for m in FUNC_DEF.finditer(text):
            params = [p.strip() for p in m.group(2).split(",")]
            if params == ["void"] or params == [""]:
                continue
            kinds[m.group(1)] = [
                "far" if re.search(r"\bfar\b", p) else ("near" if "*" in p else "val") for p in params
            ]
    return kinds


def split_args(s: str) -> tuple[list[str], int]:
    depth, cur, args = 0, "", []
    for i, ch in enumerate(s):
        if ch in "([":
            depth += 1
        elif ch in ")]":
            if depth == 0:
                args.append(cur.strip())
                return args, i
            depth -= 1
        elif ch == "," and depth == 0:
            args.append(cur.strip())
            cur = ""
            continue
        cur += ch
    return args, len(s)


def is_far_address(arg: str, far_vars: set[str], fields: set[str]) -> bool:
    arg = re.sub(r"^\(\s*\w+\s*\*\s*\)\s*", "", arg)
    if not arg.startswith("&"):
        return False
    base = arg[1:].strip().lstrip("(")
    if m := re.match(r"(\w+)\s*(->|\[)", base):
        if m.group(1) in far_vars:
            return True
    return any(re.search(rf"(->|\.)\s*{f}\s*(\[|->)", base) for f in fields)


def scan(path: Path, kinds: dict[str, list[str]], fields: set[str]) -> list[str]:
    text = path.read_text(errors="replace")
    text = re.sub(r"/\*.*?\*/", lambda m: re.sub(r"[^\n]", " ", m.group(0)), text, flags=re.S)
    far_vars = {m.group(1) for m in FAR_DECL.finditer(text)}
    found = []
    for lineno, line in enumerate(text.splitlines(), 1):
        for m in re.finditer(r"\(\s*\w+\s*\*\s*\)\s*&", line):
            if is_far_address(line[m.start():].split(",")[0].rstrip(") ;"), far_vars, fields):
                found.append(f"{path.name}:{lineno}: cast of a far address to near: {line.strip()}")
        for m in CALL.finditer(line):
            fn = m.group(1)
            if fn not in kinds:
                continue
            args, _ = split_args(line[m.end():])
            for i, a in enumerate(args):
                if i < len(kinds[fn]) and kinds[fn][i] == "near" and is_far_address(a, far_vars, fields):
                    found.append(f"{path.name}:{lineno}: {fn} arg {i + 1} is near, given {a}")
    return found


def main() -> int:
    files = [Path(a) for a in sys.argv[1:]] or sources("*.c")
    kinds, fields = param_kinds(), far_fields()
    found = [f for p in files for f in scan(p, kinds, fields)]
    print("\n".join(found) if found else "farnear: none")
    return 1 if found else 0


if __name__ == "__main__":
    sys.exit(main())
