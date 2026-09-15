"""bufcheck.py -- does every sprintf into a fixed char array fit its longest output?

    python3 tools/bufcheck.py src/*.c

For each `char name[N]` in scope and each `sprintf( name, "format", ... )` into it, the longest
output is the format's literal text plus every conversion at its widest for a 16-bit int -- %d 6,
%ld 11, %u 5, %lu 10, %x 4, %lx 8, %c 1, or its width if wider -- plus the NUL. Exits 1 when one
can overrun its array.

Listed as unchecked, not passed: %s without a precision and the float conversions, which have no
widest; and a sprintf into anything but a declared array -- a pointer, an append.
"""

import re
import sys

TOK = re.compile(
    r"/\*.*?\*/|//[^\n]*"
    r"|\bsprintf\s*\(\s*(?P<dst>[^,\"]+?)\s*,\s*(?P<fmt>(?:\"(?:[^\"\\\n]|\\.)*\"\s*)+)"
    r"|\"(?:[^\"\\\n]|\\.)*\"|'(?:[^'\\\n]|\\.)*'"
    r"|\bchar\s+(?P<name>\w+)\s*\[\s*(?P<size>\d+)\s*\]"
    r"|(?P<brace>[{}])",
    re.S,
)
CONV = re.compile(r"%[-+ #0]*(\d*)(?:\.(\d+))?([hlL]?)([diouxXcsfeEgGp%])")
WIDEST = {"d": 6, "i": 6, "o": 6, "u": 5, "x": 4, "X": 4, "c": 1, "p": 9, "%": 1}
WIDEST_LONG = {"d": 11, "i": 11, "o": 11, "u": 10, "x": 8, "X": 8}


def longest(fmt: str) -> int | None:
    body = "".join(re.findall(r"\"((?:[^\"\\\n]|\\.)*)\"", fmt))
    body = re.sub(r"\\(x[0-9a-fA-F]+|[0-7]{1,3}|.)", "e", body)
    total = len(CONV.sub("", body)) + 1
    for width, prec, size, conv in CONV.findall(body):
        match conv:
            case "s" if prec:
                n = int(prec)
            case "s" | "f" | "e" | "E" | "g" | "G":
                return None
            case _:
                n = (WIDEST_LONG if size == "l" else WIDEST).get(conv, WIDEST.get(conv, 0))
        total += max(n, int(width or 0))
    return total


def check(path: str) -> tuple[list[str], list[str]]:
    text = open(path, encoding="latin-1").read()
    depth = 0
    arrays: dict[str, list[tuple[int, int]]] = {}
    over: list[str] = []
    unchecked: list[str] = []
    for m in TOK.finditer(text):
        line = text.count("\n", 0, m.start()) + 1
        if m["brace"] == "{":
            depth += 1
        elif m["brace"] == "}":
            depth -= 1
            for scopes in arrays.values():
                while scopes and scopes[-1][1] > depth:
                    scopes.pop()
        elif m["name"]:
            arrays.setdefault(m["name"], []).append((int(m["size"]), depth))
        elif m["fmt"]:
            dst = m["dst"].strip()
            where = f"{path}:{line}: sprintf( {dst}, {m['fmt'].strip()[:48]}"
            scopes = arrays.get(dst)
            need = longest(m["fmt"])
            if not scopes:
                unchecked.append(f"{where} -- not a declared array")
            elif need is None:
                unchecked.append(f"{where} -- no widest conversion")
            elif need > scopes[-1][0]:
                over.append(f"{where} -- needs {need} of {dst}[{scopes[-1][0]}]")
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
