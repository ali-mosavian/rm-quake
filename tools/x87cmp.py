"""x87cmp.py -- does a module's float code survive a change of compiler flags?

    LIST=1 COLD= tools/bcc.sh src/game/mdl_ai.c a.asm           # -Ox
    LIST=1 COLD=mdl_ai tools/bcc.sh src/game/mdl_ai.c b.asm     # -Ox -Os
    python3 tools/x87cmp.py a.asm b.asm [FUNC ...]          # every proc in a.asm without FUNC

Walks both listings' control-flow graphs in lockstep from each function's entry. A segment runs
through labels and unconditional jumps to the next conditional jump, jump table or ret; paired
segments compare their x87 instructions, 32-bit float copies and calls, with local stack slots
mapped one to one. A condition inverted on one side matches with its successors swapped.
Exits 1 on any mismatch.

Blind to integer code, which is the compiler's to get right, and to where F_SCOPY@ calls sit,
which it prints as a note.
"""

import re
import sys

type Ins = list[tuple[str, str]]
type Listing = tuple[Ins, dict[str, int], dict[str, list[str]]]
type Term = tuple

INV = {"jb": "jae", "jae": "jb", "jbe": "ja", "ja": "jbe", "je": "jne", "jne": "je",
       "jl": "jge", "jge": "jl", "jle": "jg", "jg": "jle"}
SWAP = {"jb": "ja", "ja": "jb", "jbe": "jae", "jae": "jbe", "je": "je", "jne": "jne",
        "jl": "jg", "jg": "jl", "jle": "jge", "jge": "jle"}


def procs(path: str) -> list[str]:
    text = open(path, encoding="latin-1").read()
    return re.findall(r"^(\S+)\s+proc\b", text, re.M)


def pool(lines: list[str]) -> bytes:
    out = bytearray()
    on = False
    for ln in lines:
        if re.match(r"^s@\s+label\s+byte", ln):
            on = True
            continue
        if not on:
            continue
        m = re.match(r"^\s*db\s+(.*)$", ln)
        if not m:
            break
        arg = m.group(1).strip()
        if arg.startswith("'"):
            out += arg[1:-1].replace("''", "'").encode("latin-1")
        else:
            out.append(int(arg) & 0xFF)
    return bytes(out)


# A constant by its value, not its place in the module's pool: a function moved to another module
# reads the same numbers from other offsets.
def by_value(arg: str, data: bytes) -> str:
    def value(m: re.Match) -> str:
        size = {"dword": 4, "qword": 8, "tbyte": 10}[m.group(1)]
        at = int(m.group(2) or 0)
        if at + size > len(data):   # a cut without its pool: the offset is all there is
            return m.group(0)
        return f"{m.group(1)} const({data[at:at + size].hex()})"

    return re.sub(r"(dword|qword|tbyte)\s+ptr\s+DGROUP:s@(?:\+(\d+))?", value, arg)


def load(path: str, func: str) -> Listing:
    lines = open(path, encoding="latin-1").read().replace("\r", "").split("\n")
    data = pool(lines)
    tables: dict[str, list[str]] = {}
    cur = None
    for ln in lines:
        if m := re.match(r"^(@\S+)\s+label\s+word", ln):
            cur = m.group(1)
            tables[cur] = []
            continue
        if (m := re.match(r"^\s*dw\s+(.*)$", ln)) and cur:
            tables[cur] += [x.strip() for x in m.group(1).split(",")]
            continue
        cur = None
    body: list[str] = []
    on = False
    for ln in lines:
        f = ln.split()
        if len(f) >= 2 and f[0] == func and f[1] == "proc":
            on = True
            continue
        if len(f) >= 2 and f[0] == func and f[1] == "endp":
            break
        if on:
            body.append(ln)
    ins: Ins = []
    labels: dict[str, int] = {}
    for ln in body:
        s = ln.split(";")[0].strip()
        if not s:
            continue
        if re.match(r"^@\S+:$", s):
            labels[s[:-1]] = len(ins)
            continue
        parts = s.split(None, 1)
        ins.append((parts[0], by_value(parts[1].strip(), data) if len(parts) > 1 else ""))
    return ins, labels, tables


def norm(op: str, arg: str) -> str | None:
    arg = re.sub(r"\s+", "", arg)
    arg = re.sub(r"\[(bx|si|di)", "[R", arg)
    arg = arg.replace("short", "")
    if op.startswith("f") and op not in ("fwait", "fstsw", "fnstsw"):
        return f"{op} {arg}"
    if op == "call":
        return f"call {arg.replace('farptr', '').replace('nearptr', '')}"
    if op == "mov" and "eax" in arg:
        return f"mov {re.sub(r'(?<![a-z])(bx|si|di)(?![a-z])', 'R', arg)}"
    return None


def segment(
    ins: Ins, labels: dict[str, int], tables: dict[str, list[str]], pc: int
) -> tuple[tuple[list[str], list[int]], Term]:
    seq: list[str] = []
    scopy: list[int] = []
    seen: set[int] = set()
    while pc < len(ins):
        if pc in seen:
            return (seq, scopy), ("loop", pc)
        seen.add(pc)
        op, arg = ins[pc]
        tgt = arg.replace("short", "").strip()
        if op == "jmp" and "ptr" in arg:
            name = re.search(r"(@\S+?)\[", arg).group(1)
            return (seq, scopy), ("table", [labels[t] for t in tables[name]])
        if op == "jmp":
            pc = labels[tgt]
            continue
        if op in INV:
            return (seq, scopy), ("cond", op, labels[tgt], pc + 1)
        if op in ("ret", "retf"):
            return (seq, scopy), ("ret",)
        n = norm(op, arg)
        if n == "call F_SCOPY@":
            scopy.append(len(seq))
        elif n:
            seq.append(n)
        pc += 1
    return (seq, scopy), ("end",)


def compare(a_path: str, b_path: str, func: str) -> int:
    A, B = load(a_path, func), load(b_path, func)
    if not A[0] or not B[0]:
        # an empty walk compares nothing and would read as identical
        print(f"{func}: not a proc in {a_path if not A[0] else b_path}")
        print(f"{func}: 0 segment pairs, 1 mismatches, 0 x87 instructions in A")
        return 1
    todo: list[tuple[int, int]] = [(0, 0)]
    done: set[tuple[int, int]] = set()
    bad = pairs = 0
    notes: list[str] = []
    fwd: dict[str, str] = {}
    back: dict[str, str] = {}

    def slot_eq(x: str, y: str) -> bool:
        rx = re.findall(r"\[bp-(\d+)\]", x)
        ry = re.findall(r"\[bp-(\d+)\]", y)
        if re.sub(r"\[bp-\d+\]", "[L]", x) != re.sub(r"\[bp-\d+\]", "[L]", y) or len(rx) != len(ry):
            return False
        for u, v in zip(rx, ry):
            if fwd.setdefault(u, v) != v or back.setdefault(v, u) != u:
                notes.append(f"SLOT CONFLICT bp-{u} <-> bp-{v}")
                return False
        return True

    while todo:
        pa, pb = todo.pop()
        if (pa, pb) in done:
            continue
        done.add((pa, pb))
        pairs += 1
        (sa, ca), ta = segment(*A, pa)
        (sb, cb), tb = segment(*B, pb)
        if ca != cb:
            notes.append(f"SCOPY at A pc {pa} {ca} / B pc {pb} {cb}")
        ok_seq = len(sa) == len(sb) and all(slot_eq(x, y) for x, y in zip(sa, sb))
        if not ok_seq or ta[0] != tb[0]:
            bad += 1
            print(f"MISMATCH at A pc {pa} / B pc {pb}: term {ta[0]} vs {tb[0]}")
            for i in range(max(len(sa), len(sb))):
                x = sa[i] if i < len(sa) else "-"
                y = sb[i] if i < len(sb) else "-"
                print(f"  {'  ' if x == y else '!!'} {x:40} | {y}")
            if ta[0] != tb[0]:
                continue
        match ta[0]:
            case "cond":
                if ta[1] == tb[1]:
                    todo += [(ta[2], tb[2]), (ta[3], tb[3])]
                elif INV[ta[1]] == tb[1]:
                    todo += [(ta[2], tb[3]), (ta[3], tb[2])]
                else:
                    bad += 1
                    print(f"COND MISMATCH A pc {pa} {ta[1]} / B pc {pb} {tb[1]}")
                    # operands swapped (jge -> jle) is the likeliest cause; the integer compare is not
                    # read, so it stays a mismatch, but the walk goes on past it
                    if SWAP.get(ta[1]) == tb[1]:
                        todo += [(ta[2], tb[2]), (ta[3], tb[3])]
            case "table":
                if len(ta[1]) != len(tb[1]):
                    bad += 1
                    print("TABLE SIZE MISMATCH")
                todo += list(zip(ta[1], tb[1]))
            case "loop":
                todo.append((ta[1], tb[1]))
    fops = sum(1 for op, _ in A[0] if op.startswith("f"))
    for n in dict.fromkeys(notes):
        print(n)
    print(f"{func}: {pairs} segment pairs, {bad} mismatches, {fops} x87 instructions in A")
    return bad


def main() -> None:
    a_path, b_path = sys.argv[1:3]
    funcs = sys.argv[3:] or procs(a_path)
    bad = sum(compare(a_path, b_path, f) for f in funcs)
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
