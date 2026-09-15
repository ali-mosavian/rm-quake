"""bufcheck.py -- does every sprintf into a fixed char array fit its longest output?

    python3 tools/bufcheck.py src/*/*.c

For each `char name[N]` in scope -- every array of a `char a[N], b[M];` declaration, shadowed by an
inner pointer, macro-sized array or function parameter of the same name -- and each
`sprintf( name, "format", ... )` into it, the longest output is the format's literal text plus every
conversion at its widest for a 16-bit int -- %d 6, %ld 11, %u 5, %lu 10, %x 4, %lx 8, %c 1, or its
width if wider; %s of a declared char array its size less one, of a string literal its length -- plus
the NUL. Exits 1 when one can overrun its array.

Listed as unchecked, not passed: %s of anything else without a precision and the float conversions,
which have no widest; and a sprintf into anything but a declared array -- a pointer, an append.
"""

import re
import sys
from collections.abc import Callable

LITERAL = r"\"(?:[^\"\\\n]|\\.)*\"|'(?:[^'\\\n]|\\.)*'"
TOK = re.compile(
    r"/\*.*?\*/|//[^\n]*"
    r"|\bsprintf\s*\(\s*(?P<dst>[^,\"]+?)\s*,\s*(?P<fmt>(?:\"(?:[^\"\\\n]|\\.)*\"\s*)+)"
    rf"|{LITERAL}"
    r"|\bchar\s+(?P<decl>[^;(){}\"=]*)[;=]"
    r"|(?P<brace>[{}])",
    re.S,
)
DECLARATOR = re.compile(r"(\w+)\s*(?:\[\s*([^\]]*?)\s*\])?\s*$")
STRING = re.compile(r"\"((?:[^\"\\\n]|\\.)*)\"")
ESCAPE = re.compile(r"\\(x[0-9a-fA-F]+|[0-7]{1,3}|.)")
CONV = re.compile(r"%[-+ #0]*(\d*)(?:\.(\d+))?([hlL]?)([diouxXcsfeEgGp%])")
WIDEST = {"d": 6, "i": 6, "o": 6, "u": 5, "x": 4, "X": 4, "c": 1, "p": 9, "%": 1}
WIDEST_LONG = {"d": 11, "i": 11, "o": 11, "u": 10, "x": 8, "X": 8}


def literal_len(body: str) -> int:
    return len(ESCAPE.sub("e", body))


def call_args(text: str, at: int) -> list[str]:
    # the arguments after the format, split at the call's own commas
    args: list[str] = []
    depth, start, i = 0, at, at
    while i < len(text):
        if lit := re.compile(LITERAL).match(text, i):
            i = lit.end()
            continue
        match text[i]:
            case "(" | "[":
                depth += 1
            case ")" | "]" if depth:
                depth -= 1
            case ")":
                args.append(text[start:i])
                break
            case "," if not depth:
                args.append(text[start:i])
                start = i + 1
        i += 1
    return [a.strip() for a in args if a.strip()]


def declarators(decl: str) -> list[tuple[str, int | None]]:
    # each name, with its size when it is an array of a number: a pointer or a macro size is None
    return [(m[1], int(m[2]) if m[2] and m[2].isdigit() else None)
            for part in decl.split(",") if (m := DECLARATOR.search(part.strip()))]


def params(text: str, brace: int) -> list[str]:
    # the parameter names of the function whose body opens at brace; none for anything else
    head = text[:brace].rstrip()
    if not head.endswith(")"):
        return []
    depth = 0
    for i in range(len(head) - 1, -1, -1):
        match head[i]:
            case ")":
                depth += 1
            case "(":
                depth -= 1
                if not depth:
                    return [name for name, _ in declarators(head[i + 1:-1])]
    return []


def longest(fmt: str, args: list[str], size_of: Callable[[str], int | None]) -> int | None:
    body = "".join(STRING.findall(fmt))
    total = literal_len(CONV.sub("", body)) + 1
    arg = iter(args)
    for width, prec, size, conv in CONV.findall(body):
        a = next(arg, "") if conv != "%" else ""
        match conv:
            case "s" if prec:
                n = int(prec)
            case "s" if lit := STRING.fullmatch(a):
                n = literal_len(lit[1])
            case "s" if (have := size_of(a)) is not None:
                n = have - 1
            case "s" | "f" | "e" | "E" | "g" | "G":
                return None
            case _:
                n = (WIDEST_LONG if size == "l" else WIDEST).get(conv, WIDEST.get(conv, 0))
        total += max(n, int(width or 0))
    return total


def blank_comments(text: str) -> str:
    # spaces for every comment character, so offsets still index text
    return re.sub(rf"{LITERAL}|/\*.*?\*/|//[^\n]*",
                  lambda m: m[0] if m[0][0] in "\"'" else re.sub(r"[^\n]", " ", m[0]), text, flags=re.S)


def check(path: str) -> tuple[list[str], list[str]]:
    text = open(path, encoding="latin-1").read()
    code = blank_comments(text)
    depth = 0
    arrays: dict[str, list[tuple[int | None, int]]] = {}
    over: list[str] = []
    unchecked: list[str] = []

    def size_of(name: str) -> int | None:
        return arrays[name][-1][0] if arrays.get(name) else None

    for m in TOK.finditer(text):
        line = text.count("\n", 0, m.start()) + 1
        if m["brace"] == "{":
            if not depth:
                for name in params(code, m.start()):
                    arrays.setdefault(name, []).append((None, 1))
            depth += 1
        elif m["brace"] == "}":
            depth -= 1
            for scopes in arrays.values():
                while scopes and scopes[-1][1] > depth:
                    scopes.pop()
        elif m["decl"] is not None:
            for name, size in declarators(m["decl"]):
                arrays.setdefault(name, []).append((size, depth))
        elif m["fmt"]:
            dst = m["dst"].strip()
            where = f"{path}:{line}: sprintf( {dst}, {m['fmt'].strip()[:48]}"
            have = size_of(dst)
            need = longest(m["fmt"], call_args(text, m.end()), size_of)
            if have is None:
                unchecked.append(f"{where} -- not a declared array")
            elif need is None:
                unchecked.append(f"{where} -- no widest conversion")
            elif need > have:
                over.append(f"{where} -- needs {need} of {dst}[{have}]")
    return over, unchecked


def main() -> None:
    results = [check(p) for p in sys.argv[1:]]
    over = [o for r in results for o in r[0]]
    unchecked = [u for r in results for u in r[1]]
    for o in over:
        print("OVERRUN", o)
    for u in unchecked:
        print("unchecked", u)
    print(f"bufcheck: {len(over)} can overrun, {len(unchecked)} unchecked")
    sys.exit(1 if over else 0)


if __name__ == "__main__":
    main()
