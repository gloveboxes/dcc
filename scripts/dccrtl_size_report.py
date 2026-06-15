#!/usr/bin/env python3
"""Generate DCCRTL.MAC runtime block size and reachability reports.

The report is intended for agents and maintainers updating the MkDocs appendix
when DCCRTL.MAC changes. It parses PUBLIC-delimited runtime blocks using the
same broad model as dccrtlstrip, computes the always-present baseline reachable
from start, then computes the marginal runtime lines pulled by selected public
symbols.

Examples:
    python3 scripts/dccrtl_size_report.py > /tmp/dccrtl-size.md
    python3 scripts/dccrtl_size_report.py --all-publics --sort marginal
    python3 scripts/dccrtl_size_report.py --symbols _printf,_pffio,_malloc
    python3 scripts/dccrtl_size_report.py --format json --all-publics
"""

from __future__ import annotations

import argparse
import json
import re
from dataclasses import dataclass, field
from pathlib import Path
from typing import Dict, Iterable, List, Optional, Sequence, Set, Tuple

IDENT_RE = re.compile(r"[A-Za-z_?.][A-Za-z0-9_?.$]*")
PUBLIC_RE = re.compile(r"^\s*public\s+(.+)$", re.IGNORECASE)
LABEL_RE = re.compile(r"^\s*([A-Za-z_?.][A-Za-z0-9_?.$]*)\s*:")

REGISTER_NAMES = {
    "a", "b", "c", "d", "e", "h", "l", "af", "bc", "de", "hl", "sp", "ix", "iy",
    "nz", "z", "nc", "pe", "po", "p", "m",
}

# Curated report groups matching docs/dccrtlstrip-inclusion-table.md.
# Entries are runtime public symbols, not C names.
DOC_GROUPS: List[Tuple[str, List[str]]] = [
    ("Formatted I/O (stdio.h)", [
        "_pffio", "_fprintf", "_printf", "_sprintf", "_vprintf", "_vsprintf",
        "_vfprintf", "_puts", "__fpc", "__fps", "__pchr",
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
        "_perror", "_dopn", "_drd", "_strerror", "_setjmp", "_longjmp", "_bdos", "_inp", "_outp",
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
        return {
            "symbol": symbol,
            "found": False,
            "self": 0,
            "marginal": 0,
            "pulls_in": [],
            "block": None,
        }

    kept = closure_for_roots(lines, blocks, sym_to_block, ["start", symbol])
    extra = sorted(kept - baseline)
    return {
        "symbol": symbol,
        "found": True,
        "self": blocks[block_index].line_count,
        "marginal": sum(blocks[index].line_count for index in extra),
        "pulls_in": [block_public_label(blocks[index]) for index in extra if index != block_index],
        "block": block_public_label(blocks[block_index]),
        "block_index": block_index,
        "reachable_blocks": len(kept),
    }


def iter_symbols(args: argparse.Namespace, blocks: Sequence[Block]) -> List[Tuple[str, List[str]]]:
    if args.symbols:
        return [("Selected symbols", [part.strip() for part in args.symbols.split(",") if part.strip()])]
    if args.all_publics:
        names: List[str] = []
        for block in blocks:
            names.extend(block.public_names)
        return [("All public symbols", sorted(set(names)))]
    return DOC_GROUPS


def markdown_report(args: argparse.Namespace, data: Dict[str, object]) -> str:
    lines: List[str] = []
    lines.append("# DCCRTL.MAC size report")
    lines.append("")
    lines.append(f"Runtime: `{data['runtime']}`")
    lines.append(f"Total runtime lines: {data['total_lines']}")
    lines.append(f"Preamble lines before first public: {data['preamble_lines']}")
    lines.append(f"Parsed public blocks: {data['block_count']}")
    lines.append(f"Always-present baseline: ~{data['baseline_lines']} lines in {data['baseline_blocks']} blocks")
    lines.append("")
    lines.append("Line counts are source-line estimates, not exact bytes. `marginal` is the")
    lines.append("additional runtime source lines reachable from a symbol beyond the baseline")
    lines.append("reachable from `start`.")
    lines.append("")

    for group in data["groups"]:  # type: ignore[index]
        entries = group["entries"]
        if args.sort == "marginal":
            entries = sorted(entries, key=lambda item: (-int(item["marginal"]), str(item["symbol"])))
        elif args.sort == "symbol":
            entries = sorted(entries, key=lambda item: str(item["symbol"]))
        lines.append(f"## {group['name']}")
        lines.append("")
        lines.append("| Symbol | self | marginal | pulls in |")
        lines.append("| --- | ---: | ---: | --- |")
        for entry in entries:
            if not entry["found"]:
                lines.append(f"| `{entry['symbol']}` | - | - | not found |")
                continue
            pulls = ", ".join(f"`{name}`" for name in entry["pulls_in"][:12])
            if len(entry["pulls_in"]) > 12:
                pulls += ", ..."
            if not pulls:
                pulls = "nothing beyond own block"
            lines.append(f"| `{entry['symbol']}` | {entry['self']} | ~{entry['marginal']} | {pulls} |")
        lines.append("")
    return "\n".join(lines)


def build_report(args: argparse.Namespace) -> Dict[str, object]:
    runtime = Path(args.runtime)
    lines = runtime.read_text(encoding="utf-8", errors="replace").splitlines()
    blocks, sym_to_block, first_block_start = build_blocks(lines)
    baseline = closure_for_roots(lines, blocks, sym_to_block, ["start"])

    groups = []
    for name, symbols in iter_symbols(args, blocks):
        entries = [compute_entry(symbol, lines, blocks, sym_to_block, baseline) for symbol in symbols]
        groups.append({"name": name, "entries": entries})

    return {
        "runtime": str(runtime),
        "total_lines": len(lines),
        "preamble_lines": first_block_start if first_block_start != len(lines) else 0,
        "block_count": len(blocks),
        "baseline_lines": sum(blocks[index].line_count for index in baseline),
        "baseline_blocks": len(baseline),
        "groups": groups,
    }


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Generate DCCRTL.MAC runtime size reports")
    parser.add_argument("--runtime", default="DCCRTL.MAC", help="path to DCCRTL.MAC")
    parser.add_argument("--symbols", help="comma-separated runtime public symbols to report")
    parser.add_argument("--all-publics", action="store_true", help="report every PUBLIC symbol")
    parser.add_argument("--sort", choices=["as-listed", "marginal", "symbol"], default="as-listed")
    parser.add_argument("--format", choices=["markdown", "json"], default="markdown")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    report = build_report(args)
    if args.format == "json":
        print(json.dumps(report, indent=2))
    else:
        print(markdown_report(args, report))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
