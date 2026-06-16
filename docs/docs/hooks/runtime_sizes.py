"""MkDocs hook: inject the live DCCRTL.MAC size tables at build time.

Any page containing the marker comment

    <!-- DCCRTL-SIZE-TABLES -->

has that marker replaced with freshly generated runtime size tables, produced by
``scripts/dccrtl_size_report.py`` against the repository's ``DCCRTL.MAC``. This
keeps the published numbers from drifting out of step with the runtime source —
they are recomputed every time the docs are built.
"""

from __future__ import annotations

import importlib.util
import sys
from pathlib import Path
from types import SimpleNamespace

MARKER = "<!-- DCCRTL-SIZE-TABLES -->"


def _load_report_module(repo_root: Path):
    script = repo_root / "scripts" / "dccrtl_size_report.py"
    if not script.exists():
        raise FileNotFoundError(f"size report script not found: {script}")
    spec = importlib.util.spec_from_file_location("dccrtl_size_report", script)
    module = importlib.util.module_from_spec(spec)
    assert spec and spec.loader
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


def _generate_tables(config) -> str:
    repo_root = Path(config.config_file_path).resolve().parents[2]
    runtime = repo_root / "DCCRTL.MAC"
    if not runtime.exists():
        raise FileNotFoundError(f"runtime source not found: {runtime}")

    module = _load_report_module(repo_root)
    args = SimpleNamespace(
        runtime=str(runtime),
        symbols=None,
        all_publics=False,
        sort="as-listed",
        format="markdown",
    )
    report = module.build_report(args)
    body = module.markdown_report(args, report)

    # Drop the script's own top-level "# DCCRTL.MAC size report" heading so it
    # does not collide with the host page's title; keep the stats + tables.
    out_lines = []
    for line in body.splitlines():
        if line.startswith("# "):
            continue
        out_lines.append(line)
    return "\n".join(out_lines).strip()


def on_page_markdown(markdown, page, config, files):
    if MARKER not in markdown:
        return markdown
    return markdown.replace(MARKER, _generate_tables(config))
