"""MkDocs hook: inject the live DCCRTL.MAC size tables at build time.

Any page containing the marker comment

    <!-- DCCRTL-SIZE-TABLES -->

has that marker replaced with freshly generated runtime size tables computed
from the repository's ``DCCRTL.MAC``.  The runtime is split into PUBLIC-delimited
blocks using the same broad reachability model as ``dccrtlstrip``; for each
curated public symbol the hook reports its own block size (``self``) and the
additional blocks it transitively pulls in beyond the always-present baseline
(``marginal``).  Because this runs on every ``mkdocs build``, the published
numbers never drift out of step with the runtime source.

This hook is the sole consumer of the analysis — there is no standalone CLI.
"""

from __future__ import annotations

import re
from dataclasses import dataclass, field
from pathlib import Path
from typing import Dict, Iterable, List, Optional, Sequence, Set, Tuple

MARKER = "<!-- DCCRTL-SIZE-TABLES -->"

IDENT_RE = re.compile(r"[A-Za-z_?.][A-Za-z0-9_?.$]*")
PUBLIC_RE = re.compile(r"^\s*public\s+(.+)$", re.IGNORECASE)
LABEL_RE = re.compile(r"^\s*([A-Za-z_?.][A-Za-z0-9_?.$]*)\s*:")

REGISTER_NAMES = {
    "a", "b", "c", "d", "e", "h", "l", "af", "bc", "de", "hl", "sp", "ix", "iy",
    "nz", "z", "nc", "pe", "po", "p", "m",
}

# Curated documentation groups.  Entries are runtime public (assembler) symbols,
# not C names.
DOC_GROUPS: List[Tuple[str, List[str]]] = [
    ("Formatted I/O (stdio.h)", [
        "_pffio", "_fprintf", "_printf", "_sprintf", "_vprintf", "_vsprintf",
        "_vfprintf", "_puts", "__fpc", "__fps", "__pchr",
    ]),
    ("Console buffering control (stdio.h)", [
        "_setvbuf", "_setbuf", "_fflush",
    ]),
    ("Console input (stdio.h)", [
        "__gchr", "__gtch", "__kbht",
    ]),
    ("Formatted input (scanf family)", ["_fscanf", "_scanf", "_sscanf"]),
    ("Low-level file I/O", [
        "_fread", "_fwrite", "_fgets", "_fopen", "_write", "_lseek", "_read",
        "_open", "_close", "_unlink", "_fclose",
    ]),
    ("Memory (stdlib.h)", ["_calloc", "__real", "_malloc", "_free"]),
    ("16-bit integer and sort/search", [
        "_qsort", "_bsearch", "__stol", "__stou", "_atol", "_atoi", "_rand", "_srand",
        "_abs", "_labs", "_div", "_ldiv",
    ]),
    ("32-bit long arithmetic", [
        "__lmul", "__lds", "__lms", "__ldu", "__lmu", "__lts", "__ltu",
    ]),
    ("Float operators and conversions", [
        "__fadd", "__fmul", "__fdiv", "__flt", "__feq", "__fif", "__ffi", "__flf", "__ffl",
    ]),
    ("math.h float functions", [
        "_powf", "_tanhf", "_sinhf", "_coshf", "_expf", "_tanf", "_acosf", "_atan2f",
        "_log10f", "_asinf", "_logf", "_cosf", "_sinf", "_atanf", "_sqrtf", "_modff",
        "_fmodf", "_ldexpf", "_nextafterf", "_floorf", "_ceilf", "_frexpf", "_fabsf",
    ]),
    ("string.h / ctype.h", [
        "__sdup", "__sstr", "__stok", "__mset", "__mcpy", "__scpy", "__slen",
        "__caa", "__ctu", "__schr", "__srch", "__mcmp", "__mchr",
    ]),
    ("CP/M extensions and misc", [
        "_perror", "_dopn", "_drd", "_strerror", "_setjmp", "_longjmp", "_bdos",
        "_inp", "_outp", "__stchk",
    ]),
]


@dataclass
class Block:
    start: int
    end: int
    dep: Optional[int] = None
    name: str = ""
    public_names: List[str] = field(default_factory=list)
    labels: List[str] = field(default_factory=list)

    @property
    def line_count(self) -> int:
        return self.end - self.start


def strip_comment(line: str) -> str:
    return line.split(";", 1)[0].rstrip()


def is_public_line(line: str) -> bool:
    return PUBLIC_RE.match(strip_comment(line)) is not None


def parse_publics(line: str) -> List[str]:
    match = PUBLIC_RE.match(strip_comment(line))
    if not match:
        return []
    names: List[str] = []
    for part in match.group(1).split(","):
        part = part.strip()
        ident = IDENT_RE.match(part)
        if ident:
            names.append(ident.group(0))
    return names


def parse_label(line: str) -> Optional[str]:
    match = LABEL_RE.match(strip_comment(line))
    return match.group(1) if match else None


def is_number_token(token: str) -> bool:
    t = token.strip()
    if not t:
        return False
    if t[0] in "+-":
        t = t[1:]
    if not t:
        return False
    if t.lower().startswith("0x"):
        body = t[2:]
        return bool(body) and all(ch in "0123456789abcdefABCDEF" for ch in body)
    if t[-1:].lower() == "h":
        body = t[:-1]
        return bool(body) and all(ch in "0123456789abcdefABCDEF" for ch in body)
    return t.isdigit()


def is_runtime_ref_name(name: str) -> bool:
    low = name.lower()
    return bool(name) and low not in REGISTER_NAMES and not is_number_token(name)


def parse_refs_from_line(line: str) -> Set[str]:
    clean = strip_comment(line)
    refs: Set[str] = set()
    if not clean.strip():
        return refs

    label = parse_label(clean)
    if label:
        clean = clean.split(":", 1)[1]

    parts = clean.strip().split(None, 1)
    if not parts:
        return refs
    op = parts[0].lower()
    rest = parts[1] if len(parts) > 1 else ""

    def add_name(name: str) -> None:
        if is_runtime_ref_name(name):
            refs.add(name)

    if op == "extrn":
        for name in parse_publics("public " + rest):
            add_name(name)
        return refs

    if op == "public":
        return refs

    if op in {"call", "jp", "jr"}:
        operands = [p.strip() for p in rest.split(",")]
        if len(operands) >= 2 and operands[0].lower() in REGISTER_NAMES:
            candidate = operands[1]
        else:
            candidate = operands[0] if operands else ""
        match = IDENT_RE.search(candidate)
        if match:
            add_name(match.group(0))
        return refs

    if op == "dw":
        for operand in rest.split(","):
            match = IDENT_RE.search(operand.strip())
            if match:
                add_name(match.group(0))
        return refs

    if op == "ld":
        operands = [p.strip() for p in rest.split(",", 1)]
        if len(operands) == 2:
            rhs = operands[1].strip()
            if rhs.startswith("("):
                rhs = rhs[1:]
            match = IDENT_RE.match(rhs)
            if match:
                add_name(match.group(0))
        if operands:
            lhs = operands[0].strip()
            if lhs.startswith("("):
                lhs = lhs[1:]
                match = IDENT_RE.match(lhs)
                if match:
                    add_name(match.group(0))
        return refs

    return refs


def find_public_label(lines: Sequence[str], names: Sequence[str], start: int, end: int) -> List[int]:
    positions: List[int] = []
    wanted = {name.lower() for name in names}
    for index in range(start, end):
        label = parse_label(lines[index])
        if label and label.lower() in wanted:
            positions.append(index)
    return sorted(set(positions))


def build_blocks(lines: Sequence[str]) -> Tuple[List[Block], Dict[str, int], int]:
    blocks: List[Block] = []
    sym_to_block: Dict[str, int] = {}
    first_block_start = len(lines)
    index = 0

    def add_sym(name: str, block_index: int) -> None:
        sym_to_block.setdefault(name, block_index)
        sym_to_block.setdefault(name.lower(), block_index)

    while index < len(lines):
        if not is_public_line(lines[index]):
            index += 1
            continue

        run_start = index
        first_block_start = min(first_block_start, run_start)
        run_end = run_start
        while run_end < len(lines) and is_public_line(lines[run_end]):
            run_end += 1

        next_run = run_end
        while next_run < len(lines) and not is_public_line(lines[next_run]):
            next_run += 1

        public_names: List[str] = []
        for line_index in range(run_start, run_end):
            public_names.extend(parse_publics(lines[line_index]))

        starts = find_public_label(lines, public_names, run_end, next_run)
        if len(starts) <= 1:
            block_index = len(blocks)
            block = Block(run_start, next_run, name=public_names[0] if public_names else f"block{block_index}")
            blocks.append(block)
            for line_index in range(run_start, next_run):
                for name in parse_publics(lines[line_index]):
                    block.public_names.append(name)
                    add_sym(name, block_index)
                label = parse_label(lines[line_index])
                if label:
                    block.labels.append(label)
                    add_sym(label, block_index)
            index = next_run
            continue

        prelude_index = len(blocks)
        prelude = Block(run_start, starts[0], name="public_prelude")
        blocks.append(prelude)
        for line_index in range(run_start, starts[0]):
            label = parse_label(lines[line_index])
            if label:
                prelude.labels.append(label)
                add_sym(label, prelude_index)

        for offset, block_start in enumerate(starts):
            block_end = starts[offset + 1] if offset + 1 < len(starts) else next_run
            block_index = len(blocks)
            label = parse_label(lines[block_start]) or f"block{block_index}"
            block = Block(block_start, block_end, dep=prelude_index, name=label)
            blocks.append(block)
            for line_index in range(block_start, block_end):
                label = parse_label(lines[line_index])
                if label:
                    block.labels.append(label)
                    add_sym(label, block_index)
                for name in parse_publics(lines[line_index]):
                    block.public_names.append(name)
                    add_sym(name, block_index)

        for name in public_names:
            if name.lower() not in sym_to_block:
                prelude.public_names.append(name)
                add_sym(name, prelude_index)

        index = next_run

    return blocks, sym_to_block, first_block_start


def closure_for_roots(lines: Sequence[str], blocks: Sequence[Block], sym_to_block: Dict[str, int], roots: Iterable[str]) -> Set[int]:
    root_set = set(roots)
    kept: Set[int] = set()
    changed = True
    while changed:
        changed = False
        for root in list(root_set):
            block_index = sym_to_block.get(root, sym_to_block.get(root.lower()))
            if block_index is None:
                continue
            if block_index not in kept:
                kept.add(block_index)
                changed = True
            dep = blocks[block_index].dep
            if dep is not None and dep not in kept:
                kept.add(dep)
                changed = True
        for block_index in list(kept):
            dep = blocks[block_index].dep
            if dep is not None and dep not in kept:
                kept.add(dep)
                changed = True
            for line_index in range(blocks[block_index].start, blocks[block_index].end):
                before = len(root_set)
                root_set.update(parse_refs_from_line(lines[line_index]))
                if len(root_set) != before:
                    changed = True
    return kept


def block_public_label(block: Block) -> str:
    if block.public_names:
        return "/".join(block.public_names[:3]) + ("/..." if len(block.public_names) > 3 else "")
    if block.labels:
        return "/".join(block.labels[:3]) + ("/..." if len(block.labels) > 3 else "")
    return block.name


def compute_entry(symbol: str, lines: Sequence[str], blocks: Sequence[Block], sym_to_block: Dict[str, int], baseline: Set[int]) -> Dict[str, object]:
    block_index = sym_to_block.get(symbol, sym_to_block.get(symbol.lower()))
    if block_index is None:
        return {"symbol": symbol, "found": False, "self": 0, "marginal": 0, "pulls_in": []}

    kept = closure_for_roots(lines, blocks, sym_to_block, ["start", symbol])
    extra = sorted(kept - baseline)
    return {
        "symbol": symbol,
        "found": True,
        "self": blocks[block_index].line_count,
        "marginal": sum(blocks[index].line_count for index in extra),
        "pulls_in": [block_public_label(blocks[index]) for index in extra if index != block_index],
    }


def render_tables(runtime: Path) -> str:
    lines = runtime.read_text(encoding="utf-8", errors="replace").splitlines()
    blocks, sym_to_block, first_block_start = build_blocks(lines)
    baseline = closure_for_roots(lines, blocks, sym_to_block, ["start"])
    baseline_lines = sum(blocks[index].line_count for index in baseline)
    preamble_lines = first_block_start if first_block_start != len(lines) else 0

    out: List[str] = []
    out.append(f"Runtime: `{runtime.name}`")
    out.append("")
    out.append(f"- Total runtime lines: **{len(lines)}**")
    out.append(f"- Preamble lines before the first `public`: {preamble_lines}")
    out.append(f"- Parsed public blocks: {len(blocks)}")
    out.append(f"- Always-present baseline: **~{baseline_lines} lines** in {len(baseline)} blocks")
    out.append("")

    for name, symbols in DOC_GROUPS:
        out.append(f"### {name}")
        out.append("")
        out.append("| Symbol | self | marginal | pulls in |")
        out.append("| --- | ---: | ---: | --- |")
        for symbol in symbols:
            entry = compute_entry(symbol, lines, blocks, sym_to_block, baseline)
            if not entry["found"]:
                out.append(f"| `{entry['symbol']}` | - | - | not found |")
                continue
            pulls = ", ".join(f"`{n}`" for n in entry["pulls_in"][:12])
            if len(entry["pulls_in"]) > 12:
                pulls += ", ..."
            if not pulls:
                pulls = "nothing beyond own block"
            out.append(f"| `{entry['symbol']}` | {entry['self']} | ~{entry['marginal']} | {pulls} |")
        out.append("")

    return "\n".join(out).strip()


def _generate_tables(config) -> str:
    repo_root = Path(config.config_file_path).resolve().parents[2]
    runtime = repo_root / "DCCRTL.MAC"
    if not runtime.exists():
        raise FileNotFoundError(f"runtime source not found: {runtime}")
    return render_tables(runtime)


def on_page_markdown(markdown, page, config, files):
    if MARKER not in markdown:
        return markdown
    return markdown.replace(MARKER, _generate_tables(config))
