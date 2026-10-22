"""Print decompiled functions from local/re/decomp.c.

Usage: python -I getfn.py FUN_140039b00 [more names...]
       python -I getfn.py --callers FUN_140039b00   (functions whose body mentions the name)
       python -I getfn.py --grep REGEX              (matching lines, prefixed by function)
"""
import re
import sys
from pathlib import Path

DECOMP = Path(__file__).resolve().parents[3] / "li-re" / "decomp.c"


def load():
    text = DECOMP.read_text(encoding="utf-8", errors="replace")
    parts = re.split(r"^// ==== (\S+) @ (\S+)\n", text, flags=re.M)
    return {parts[i]: (parts[i + 1], parts[i + 2]) for i in range(1, len(parts) - 2, 3)}


def main():
    fns = load()
    args = sys.argv[1:]
    if args and args[0] == "--grep":
        pat = re.compile(args[1])
        for f, (_, body) in fns.items():
            for line in body.splitlines():
                if pat.search(line):
                    print(f"{f}: {line.strip()}")
        return
    if args and args[0] == "--callers":
        for name in args[1:]:
            pat = re.compile(r"\b" + re.escape(name) + r"\b")
            hits = [f for f, (_, body) in fns.items() if f != name and pat.search(body)]
            print(f"{name}: {' '.join(hits)}")
        return
    for name in args:
        if name not in fns:
            print(f"// {name}: not found")
            continue
        addr, body = fns[name]
        print(f"// ==== {name} @ {addr}\n{body}")


if __name__ == "__main__":
    main()
