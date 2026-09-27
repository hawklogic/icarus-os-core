#!/usr/bin/env python3
"""Fail if an SVC dispatch case uses a caller pointer it has not validated.

SVC implementations run privileged, so the MPU does not stop them from
touching whatever address a task passes in a register.  Every case of an SVC
dispatcher that turns a caller argument into a pointer must therefore check
it first with a caller-buffer validator (svc_user_buffer_ok(),
svc_user_opt_buffer_ok(), svc_user_string_ok(), svc_buffer_allowed(), or
bkpram_range_ok() for an offset into backup SRAM).  The allowlist policy is
host-tested; this check guards the per-case wiring, which the host build
cannot exercise because the dispatcher is target-only code.

For each ``switch`` with ``case SVC_...:`` labels it:

- takes the dispatcher's frame parameter (``uint32_t *stack_frame``): each
  ``stack_frame[N]`` is caller register N, and prologue copies such as
  ``uint32_t arg0 = stack_frame[0];`` (optionally cast, or one of several
  comma-separated declarators) are the same register; any other prologue
  read of a caller register fails;
- follows locals assigned from those values inside the case (not through
  function calls);
- finds every cast of such a value, or of an expression built from it, to a
  pointer type: ``(const uint8_t *)(uintptr_t)arg1``,
  ``(void *)(BASE + offset)``.  When the cast initialises a pointer local,
  the local's later uses count instead of the cast;
- requires a validator call whose first argument is the same caller
  register, earlier in the same case.

Casts to function pointers (a task entry point, a callback) are not caller
buffers.  The cases that only store one are listed in
FUNCTION_POINTER_ALLOWLIST with the reason; any other function-pointer cast
of a caller argument fails, a data pointer in an allowlisted case is still
checked, and an allowlist entry that no longer matches fails the check.
Cast types are classified from the typedefs in Core/Inc and USB_DEVICE; an
unknown type name applied to a caller argument fails rather than being
assumed not to be a pointer.

Limits: the check is textual and tracks caller registers, not addresses.  It
does not prove that the validator's result gates the use, that its length
argument is right, or that arithmetic on a validated register stays inside
the checked range (the host tests cover the policy itself).  A caller
register passed as an integer to a function that casts it inside is not
followed.  In the dispatcher prologue only the register copies are read; the
SVC number is read through the hardware-stacked PC and is not checked here.

    python3 tools/check_svc_pointer_checks.py [files...]   (default: Core/Src/**/*.c)
"""
import re
import sys
from pathlib import Path

# Calls that validate the caller register named by their first argument.
VALIDATOR = re.compile(
    r"\b(svc_user_\w+_ok|svc_buffer_allowed|bkpram_range_ok)\s*\(")

# (case label, caller register) -> why a function-pointer cast is safe there.
FUNCTION_POINTER_ALLOWLIST = {
    ("SVC_OS_REGISTER_TASK", 0):
        "task entry point: stored in the task control block; the task "
        "starts in unprivileged thread mode",
    ("SVC_CS_SET_CALLBACK", 0):
        "mismatch callback: stored; cs_check_all() calls it in the caller's "
        "unprivileged thread after the SVC has returned",
}

LABEL = re.compile(r"\b(?:case\s+([A-Za-z_]\w*)|default)\s*:")
SWITCH = re.compile(r"\bswitch\s*\(")
IDENT = re.compile(r"[A-Za-z_]\w*")
ASSIGN = re.compile(r"\b([A-Za-z_]\w*)\s*(?:[-+*/%&|^]|<<|>>)?=(?!=)")
ASSIGN_OP = re.compile(r"\s*(?:[-+*/%&|^]|<<|>>)?=(?!=)")
CALL = re.compile(r"\b([A-Za-z_]\w*)\s*\(")
QUALIFIERS = re.compile(
    r"\b(const|volatile|struct|enum|union|signed|unsigned)\b")

VALUE_BUILTINS = {
    "void", "bool", "_Bool", "char", "short", "int", "long", "float",
    "double", "size_t", "ptrdiff_t", "uintptr_t", "intptr_t",
    "int8_t", "int16_t", "int32_t", "int64_t",
    "uint8_t", "uint16_t", "uint32_t", "uint64_t",
}
# Words that may directly precede the '(' of a cast.
NON_CALL_WORDS = {"return", "case"}
# Words followed by '(' that are not function calls.
NOT_CALLS = {"sizeof", "_Alignof", "if", "while", "for", "switch", "return"}


def strip_code(text: str) -> str:
    """Blank comments and string/char literals, keeping offsets and lines."""
    out = list(text)
    i, n = 0, len(text)

    def blank(a: int, b: int):
        for k in range(a, b):
            if out[k] != "\n":
                out[k] = " "

    while i < n:
        if text.startswith("//", i):
            j = text.find("\n", i)
            j = n if j < 0 else j
            blank(i, j)
            i = j
        elif text.startswith("/*", i):
            j = text.find("*/", i + 2)
            j = n if j < 0 else j + 2
            blank(i, j)
            i = j
        elif text[i] in "\"'":
            q, j = text[i], i + 1
            while j < n and text[j] != q:
                j += 2 if text[j] == "\\" else 1
            blank(i + 1, min(j, n))           # keep the quotes themselves
            i = j + 1
        else:
            i += 1
    return "".join(out)


def match_forward(text: str, i: int, open_c: str, close_c: str) -> int:
    """Index of the bracket closing the one at i, or -1."""
    depth = 0
    for j in range(i, len(text)):
        if text[j] == open_c:
            depth += 1
        elif text[j] == close_c:
            depth -= 1
            if depth == 0:
                return j
    return -1


def match_backward(text: str, i: int) -> int:
    """Index of the '(' opening the ')' at i, or -1."""
    depth = 0
    for j in range(i, -1, -1):
        if text[j] == ")":
            depth += 1
        elif text[j] == "(":
            depth -= 1
            if depth == 0:
                return j
    return -1


def prev_nonspace(text: str, i: int) -> int:
    while i >= 0 and text[i].isspace():
        i -= 1
    return i


def next_nonspace(text: str, i: int) -> int:
    while i < len(text) and text[i].isspace():
        i += 1
    return i


def word_before(text: str, i: int) -> str:
    """Identifier ending right before index i (blanks skipped), or ''."""
    k = prev_nonspace(text, i - 1)
    j = k
    while j >= 0 and (text[j].isalnum() or text[j] == "_"):
        j -= 1
    return text[j + 1:k + 1]


def harvest_typedefs(texts):
    """Map typedef name -> 'fn' | 'data' | 'value'."""
    kinds = {}
    for t in texts:
        for m in re.finditer(r"\btypedef\b", t):
            i, depth = m.end(), 0
            while i < len(t):
                if t[i] == "{":
                    depth += 1
                elif t[i] == "}":
                    depth -= 1
                elif t[i] == ";" and depth == 0:
                    break
                i += 1
            flat = t[m.end():i]
            while re.search(r"\{[^{}]*\}", flat):
                flat = re.sub(r"\{[^{}]*\}", " ", flat)
            flat = re.sub(r"__attribute__\s*\(\(.*?\)\)", " ", flat).strip()
            fn = re.search(r"\(\s*\*\s*([A-Za-z_]\w*)\s*\)\s*\(", flat)
            if fn:
                kinds[fn.group(1)] = "fn"
                continue
            nm = re.search(r"([A-Za-z_]\w*)\s*(?:\[[^\]]*\]\s*)*$", flat)
            if nm:
                kinds[nm.group(1)] = "data" if "*" in flat[:nm.start()] \
                    else "value"
    return kinds


def classify_type(content: str, typedefs) -> str:
    """'fn', 'data', 'value' or 'unknown' for a type name, '' otherwise."""
    t = QUALIFIERS.sub(" ", content).strip()
    if re.fullmatch(r"[\w\s]*\(\s*\*\s*\)\s*\([\w\s,\*]*\)", t):
        return "fn"                           # e.g. void (*)(void)
    if "*" in t:
        return "data" if re.fullmatch(r"[\w\s]+\*[\s\*]*", t) else ""
    if t == "":
        return "value" if content.strip() else ""   # plain "unsigned"
    words = t.split()
    if not all(IDENT.fullmatch(w) for w in words):
        return ""
    name = words[-1]
    if name in VALUE_BUILTINS:
        return "value"
    return typedefs.get(name, "unknown")


def cast_chain(text: str, pos: int, typedefs):
    """Casts written directly before index pos, innermost first.

    Returns ([(kind, type text)], start of the outermost cast).
    """
    kinds, start = [], pos
    i = prev_nonspace(text, pos - 1)
    while i >= 0 and text[i] == ")":
        j = match_backward(text, i)
        if j < 0:
            break
        w = word_before(text, j)
        if w and w not in NON_CALL_WORDS:
            break                             # call, if (...), sizeof (...)
        kind = classify_type(text[j + 1:i], typedefs)
        if not kind:
            break
        kinds.append((kind, " ".join(text[j + 1:i].split())))
        start = j
        i = prev_nonspace(text, j - 1)
    return kinds, start


def pointer_casts(text: str, s: int, e: int, typedefs):
    """Pointer casts applied to text[s:e] or to an expression built from it.

    Walks out through enclosing parenthesised expressions (stopping at a
    call's argument list, a condition or the statement boundary) and looks at
    the casts in front of each level.  Returns (kind or None, start, end,
    [unknown type names]).
    """
    found, unknown = None, []
    while True:
        kinds, s = cast_chain(text, s, typedefs)
        for kind, name in kinds:
            if kind == "unknown":
                unknown.append(name)
            elif kind == "data" or (kind == "fn" and found is None):
                found = kind
        depth, j = 0, s - 1
        while j >= 0:
            c = text[j]
            if c == ")":
                depth += 1
            elif c == "(":
                if depth == 0:
                    break
                depth -= 1
            elif c in ";{}" and depth == 0:
                j = -1
                break
            j -= 1
        if j < 0:
            return found, s, e, unknown
        w = word_before(text, j)
        if w and w not in NON_CALL_WORDS:
            return found, s, e, unknown
        close = match_forward(text, j, "(", ")")
        if close < 0:
            return found, s, e, unknown
        s, e = j, close + 1


class Checker:
    """Checks every SVC dispatch in one source file."""

    def __init__(self, path: Path, typedefs):
        self.path = path
        self.text = strip_code(path.read_text(encoding="utf-8",
                                              errors="replace"))
        self.typedefs = dict(typedefs)
        self.typedefs.update(harvest_typedefs([self.text]))
        self.errors = []
        self.cases = 0
        self.validated_uses = 0
        self.allowlisted = set()

    def error(self, pos: int, msg: str):
        line = self.text.count("\n", 0, pos) + 1
        self.errors.append(f"{self.path}:{line}: {msg}")

    def function_header(self, pos: int):
        """(header text, index of its '{') of the function containing pos."""
        depth, top = 0, -1
        for i in range(pos):
            c = self.text[i]
            if c == "{":
                if depth == 0:
                    top = i
                depth += 1
            elif c == "}":
                depth -= 1
        if top < 0:
            return "", 0
        k = max(self.text.rfind(";", 0, top), self.text.rfind("}", 0, top))
        return self.text[k + 1:top], top

    def run(self) -> int:
        found = 0
        t = self.text
        for m in SWITCH.finditer(t):
            close = match_forward(t, m.end() - 1, "(", ")")
            if close < 0:
                continue
            brace = next_nonspace(t, close + 1)
            if brace >= len(t) or t[brace] != "{":
                continue
            end = match_forward(t, brace, "{", "}")
            off = brace + 1
            depth, d = [], 0
            for c in t[off:end]:
                depth.append(d)
                if c == "{":
                    d += 1
                elif c == "}":
                    d -= 1
            labels = [(lm.group(1) or "default", lm.start(), lm.end())
                      for lm in LABEL.finditer(t, off, end)
                      if depth[lm.start() - off] == 0]
            if not any(lbl.startswith("SVC_") for lbl, _, _ in labels):
                continue
            found += 1
            self.check_dispatch(off, end, labels)
        return found

    def check_dispatch(self, off: int, end: int, labels):
        t = self.text
        header, fn_open = self.function_header(off)
        pm = re.search(r"\(([^()]*)\)\s*$", header)
        frames = re.findall(r"\*\s*([A-Za-z_]\w*)\s*(?:,|$)",
                            pm.group(1)) if pm else []
        if len(frames) != 1:
            self.error(off, "SVC dispatch: cannot identify the exception "
                            "frame parameter")
            return
        frame = frames[0]
        # Prologue copies of a caller register, with or without casts, alone
        # or as one of several comma-separated declarators.
        prologue = t[fn_open:off]
        aliases, understood = {}, set()
        for m in re.finditer(
                r"\b([A-Za-z_]\w*)\s*=\s*(?:\(\s*[^()]*\)\s*)*(?P<f>" +
                re.escape(frame) + r")\s*\[\s*(\d+)\s*\]\s*[;,]", prologue):
            aliases[m.group(1)] = int(m.group(3))
            understood.add(m.start("f"))
        # Any other prologue read of a caller register is a copy this check
        # cannot follow, so it fails rather than miss one.  The stacked PC
        # (index 6), read to find the SVC number, is not a caller argument.
        for m in re.finditer(r"\b" + re.escape(frame) + r"\s*\[\s*(\d+)\s*\]",
                             prologue):
            if m.start() not in understood and m.group(1) != "6":
                self.error(fn_open + m.start(),
                           f"SVC dispatch prologue reads {frame}"
                           f"[{m.group(1)}] in a form this check cannot "
                           f"follow; copy it as 'name = {frame}[N];'")
        # Fall-through labels share the code that follows them.
        pending = []
        for idx, (lbl, _, le) in enumerate(labels):
            nxt = labels[idx + 1][1] if idx + 1 < len(labels) else end
            pending.append(lbl)
            if t[le:nxt].strip():
                self.cases += 1
                self.check_case(pending, le, nxt, frame, aliases)
                pending = []

    def check_case(self, names, s, e, frame, prologue_aliases):
        t = self.text
        label = "/".join(names)
        local = {}        # integer local -> caller registers it holds
        ptr = {}          # pointer local -> (caller registers, kind)
        validated = []    # (position, caller registers)
        uses = []         # (position, caller registers, kind, source text)

        spans = []
        for m in VALIDATOR.finditer(t, s, e):
            close = match_forward(t, m.end() - 1, "(", ")")
            if close >= 0:
                spans.append((m.start(), m.end(), close))

        def in_validator(p):
            return any(a <= p <= c for a, _, c in spans)

        def operand(m):
            """(end, registers, pointer-local kind), 'whole', or None."""
            name = m.group(0)
            if name == frame:
                im = re.match(r"\s*\[\s*(\d+)\s*\]", t[m.end():e])
                if not im:
                    return "whole"
                return m.end() + im.end(), {int(im.group(1))}, None
            if name in ptr:
                return m.end(), ptr[name][0], ptr[name][1]
            if name in local:
                return m.end(), local[name], None
            if name in prologue_aliases:
                return m.end(), {prologue_aliases[name]}, None
            return None

        def registers_in(a, b):
            regs = set()
            for m in IDENT.finditer(t, a, b):
                op = operand(m)
                if op and op != "whole":
                    regs |= op[1]
            return regs

        events = []
        for m in ASSIGN.finditer(t, s, e):
            semi = t.find(";", m.end(), e)
            if semi >= 0 and not in_validator(m.start()):
                events.append((semi, 0, ("assign", m.group(1), m.end(), semi)))
        for vs, ve, vc in spans:
            events.append((vs, 1, ("validator", ve, vc)))
        for m in IDENT.finditer(t, s, e):
            events.append((m.start(), 2, ("ident", m)))
        events.sort(key=lambda ev: (ev[0], ev[1]))

        for pos, _, ev in events:
            if ev[0] == "validator":
                _, ve, vc = ev
                depth, j = 0, ve
                while j < vc:                 # end of the first argument
                    if t[j] in "([":
                        depth += 1
                    elif t[j] in ")]":
                        depth -= 1
                    elif t[j] == "," and depth == 0:
                        break
                    j += 1
                validated.append((pos, registers_in(ve, j)))
                continue
            if ev[0] == "assign":
                _, lhs, a, b = ev
                calls = [c for c in CALL.findall(t, a, b)
                         if c not in NOT_CALLS and c not in VALUE_BUILTINS
                         and c not in self.typedefs]
                regs = registers_in(a, b)
                if regs and not calls and lhs not in ptr:
                    local[lhs] = regs
                continue

            m = ev[1]
            if t[max(0, m.start() - 1)] == "." or \
                    t[max(0, m.start() - 2):m.start()] == "->":
                continue                      # struct member
            if in_validator(m.start()):
                continue
            op = operand(m)
            if op is None:
                continue
            if op == "whole":
                self.error(m.start(), f"{label}: the exception frame "
                                      f"'{frame}' is passed on whole, so its "
                                      f"pointer arguments cannot be checked")
                continue
            op_end, regs, ptr_kind = op
            if ASSIGN_OP.match(t, op_end):
                continue                      # assignment target
            if ptr_kind is not None:
                uses.append((m.start(), regs, ptr_kind, m.group(0)))
                continue
            kind, xs, xe, unknown = pointer_casts(t, m.start(), op_end,
                                                  self.typedefs)
            for u in unknown:
                self.error(m.start(), f"{label}: cannot tell whether cast "
                                      f"type '{u}' is a pointer; declare it "
                                      f"in a header under Core/Inc")
            if kind is None:
                continue
            # "name = <cast>;" makes a pointer local: its uses count.
            eq = prev_nonspace(t, xs - 1)
            nx = next_nonspace(t, xe)
            if eq > 0 and t[eq] == "=" and t[eq - 1] not in "=!<>" and \
                    nx < e and t[nx] == ";" and word_before(t, eq):
                ptr[word_before(t, eq)] = (regs, kind)
                continue
            uses.append((m.start(), regs, kind, t[xs:xe]))

        for pos, regs, kind, src in uses:
            if kind == "fn":
                keys = {(n, r) for r in regs for n in names
                        if (n, r) in FUNCTION_POINTER_ALLOWLIST}
                if {r for _, r in keys} == regs:
                    self.allowlisted |= keys
                    continue
            checked = set()
            for vpos, vregs in validated:
                if vpos < pos:
                    checked |= vregs
            if regs <= checked:
                self.validated_uses += 1
                continue
            what = "function" if kind == "fn" else "data"
            hint = ("; if the kernel only stores it and never calls it "
                    "privileged, add it to FUNCTION_POINTER_ALLOWLIST"
                    if kind == "fn" else "")
            self.error(pos, f"{label}: caller argument "
                            f"{', '.join(f'r{r}' for r in sorted(regs))} "
                            f"used as a {what} pointer "
                            f"('{' '.join(src.split())}') without a "
                            f"preceding validator on it{hint}")


def main(argv) -> int:
    root = Path(__file__).resolve().parents[1]
    files = [Path(a) for a in argv] or \
        sorted((root / "Core" / "Src").rglob("*.c"))
    headers = sorted((root / "Core" / "Inc").rglob("*.h")) + \
        sorted((root / "USB_DEVICE").rglob("*.h"))
    typedefs = harvest_typedefs(
        strip_code(h.read_text(encoding="utf-8", errors="replace"))
        for h in headers)

    errors, dispatchers, cases, validated, allowlisted = [], 0, 0, 0, set()
    for f in files:
        c = Checker(f, typedefs)
        dispatchers += c.run()
        errors += c.errors
        cases += c.cases
        validated += c.validated_uses
        allowlisted |= c.allowlisted
    if dispatchers == 0:
        errors.append("no SVC dispatch (switch with 'case SVC_...:') found")
    else:
        for name, reg in sorted(set(FUNCTION_POINTER_ALLOWLIST) - allowlisted):
            errors.append(f"FUNCTION_POINTER_ALLOWLIST: stale entry {name} "
                          f"r{reg} (no function-pointer cast of it found)")
    for err in errors:
        print(err, file=sys.stderr)
    if errors:
        return 1
    print(f"check_svc_pointer_checks: {dispatchers} dispatch, {cases} cases, "
          f"{validated} pointer uses validated, {len(allowlisted)} "
          f"function pointers allowlisted: OK")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
