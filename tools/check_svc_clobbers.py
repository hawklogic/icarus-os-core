#!/usr/bin/env python3
"""Fail if an inline-asm block that issues an SVC lacks a "memory" clobber.

An SVC handler reads and writes caller memory through pointers passed in
registers (out-parameters, buffers) and changes scheduler state.  Without a
"memory" clobber the compiler may keep values cached in registers across the
asm, or assume that a local whose address was passed is unchanged: a wrapper
that reads an out-parameter after the SVC then sees its initial value.  Host
tests cannot catch this because the host build never executes the asm, so
this check runs with the host test suite.

    python3 tools/check_svc_clobbers.py [files...]   (default: Core/Src/**/*.c)
"""
import re
import sys
from pathlib import Path

ASM_START = re.compile(r"__asm__\s+(?:volatile|__volatile__)\s*\(")
SVC_INSN = re.compile(r"\bsvc\b")


def block_end(text: str, open_idx: int) -> int:
    depth, in_str, i = 0, False, open_idx
    while i < len(text):
        c = text[i]
        if in_str:
            if c == "\\":
                i += 2
                continue
            if c == '"':
                in_str = False
        elif c == '"':
            in_str = True
        elif c == "(":
            depth += 1
        elif c == ")":
            depth -= 1
            if depth == 0:
                return i
        i += 1
    raise ValueError("unbalanced asm block")


def offenders(path: Path):
    text = path.read_text(encoding="utf-8", errors="replace")
    for m in ASM_START.finditer(text):
        open_idx = m.end() - 1
        body = text[open_idx + 1:block_end(text, open_idx)]
        if SVC_INSN.search(body) and '"memory"' not in body:
            yield text.count("\n", 0, m.start()) + 1


def main(argv) -> int:
    root = Path(__file__).resolve().parents[1]
    files = [Path(a) for a in argv] or sorted((root / "Core" / "Src").rglob("*.c"))
    bad = [(f, line) for f in files for line in offenders(f)]
    for f, line in bad:
        print(f"{f}:{line}: SVC asm without a \"memory\" clobber", file=sys.stderr)
    if bad:
        return 1
    print(f"check_svc_clobbers: {len(files)} files OK")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
