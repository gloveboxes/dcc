#!/usr/bin/env python3
"""Parallel dcc regression runner.

Builds every tests/*.c application directly through the dcc/M80/L80 pipeline,
runs it under ntvcm, and compares stdout with tests/baselines/<app>.txt.
"""

from __future__ import annotations

import argparse
import concurrent.futures
import json
import os
import re
import shutil
import subprocess
import sys
import time
from dataclasses import dataclass
from pathlib import Path


@dataclass(frozen=True)
class WorkItem:
    app: str
    args: str
    stack_size: str
    source_size: int


@dataclass
class Result:
    app: str
    passed: bool
    elapsed: float
    lines: list[str]


PLACEHOLDERS = {
    "{{DATE}}": r"[A-Z][a-z]{2}\s+\d{1,2}\s+\d{4}",
    "{{TIME}}": r"\d{2}:\d{2}:\d{2}",
    "{{SEP}}": r"[/\\]",
}

FLOATIO_RE = re.compile(r"%[-+ #0-9.*]*[fF]")
TOKEN_RE = re.compile(r"\{\{([A-Z]+)\}\}")


def convert_to_crlf(path: Path) -> None:
    if not path.exists():
        return
    text = path.read_text(encoding="utf-8")
    text = text.replace("\r\n", "\n").replace("\r", "\n").replace("\n", "\r\n")
    path.write_text(text, encoding="utf-8", newline="")


def tool(name: str, default: str) -> str:
    value = os.environ.get(name, "").strip()
    return value or default


def run_cmd(argv: list[str], cwd: Path | None = None, env: dict[str, str] | None = None, timeout: int | None = None) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        argv,
        cwd=str(cwd) if cwd else None,
        env=env,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        timeout=timeout,
    )


def resolve_source(app: str) -> Path | None:
    base = Path(app).stem
    lower = base.lower()
    upper = base.upper()
    for candidate in (
        Path("tests") / f"{base}.c",
        Path("tests") / f"{base}.C",
        Path("tests") / f"{lower}.c",
        Path("tests") / f"{upper}.C",
        Path(f"{base}.c"),
        Path(f"{base}.C"),
        Path(f"{lower}.c"),
        Path(f"{upper}.C"),
    ):
        if candidate.is_file():
            return candidate
    return None


def copy_if_missing(src: Path, dest: Path) -> None:
    if src.exists() and not dest.exists():
        try:
            shutil.copyfile(src, dest)
        except OSError:
            pass


def build_app(app: str, mode: str, build_dir: Path, emulator: str, env: dict[str, str]) -> bool:
    use_peep = mode.lower() in {"peep", "opt", "optimized", "o", "1", "yes", "true"}
    base = Path(app).stem
    lower = base.lower()
    upper = base.upper()

    source = resolve_source(app)
    if source is None:
        raise RuntimeError(f"Source file not found for: {app}")

    build_dir.mkdir(parents=True, exist_ok=True)
    copy_if_missing(Path("m80.com"), build_dir / "m80.com")
    copy_if_missing(Path("l80.com"), build_dir / "l80.com")

    app_mac = build_dir / f"{upper}.MAC"
    app_rel = build_dir / f"{upper}.REL"
    app_com = build_dir / f"{upper}.COM"
    peep_tmp = build_dir / "_PEEPOUT.MAC"
    rtl_src = build_dir / "DCCRTL.MAC"
    rtl_min = build_dir / "RTLMIN.MAC"
    root_rtl = Path("DCCRTL.MAC")
    if not root_rtl.exists():
        raise RuntimeError("Runtime not found: DCCRTL.MAC")

    for stale in (
        app_mac,
        app_rel,
        app_com,
        build_dir / f"{upper}.PRN",
        peep_tmp,
        rtl_src,
        rtl_min,
        build_dir / "RTLMIN.REL",
        build_dir / "RTLMIN.PRN",
    ):
        try:
            stale.unlink()
        except FileNotFoundError:
            pass

    source_text = source.read_text(encoding="utf-8")
    floatio = bool(FLOATIO_RE.search(source_text))
    stack_check = "-fstack-check" if env.get("DCC_FORCE_STACK_CHECK") == "1" or "DCC_STACK_CHECK" in source_text else None
    stack_size = env.get("DCC_STACK_SIZE") or "512"

    dcc = tool("DCC", "dcc")
    dccpeep = tool("DCCPEEP", "dccpeep")
    dccrtlstrip = tool("DCCRTLSTRIP", "dccrtlstrip")
    ntvcm = tool("NTVCM", emulator)
    m80 = tool("M80", "m80")
    l80 = tool("L80", "l80")

    dcc_args = [dcc]
    if stack_check:
        dcc_args.append(stack_check)
    if floatio:
        dcc_args.append("-ffloatio")
    dcc_args.extend(["-stack", str(stack_size), str(source), "-o", str(app_mac)])
    cp = run_cmd(dcc_args, env=env)
    if cp.returncode != 0 or not app_mac.exists():
        raise RuntimeError(f"Compilation failed for {app}: {cp.stdout.strip()}")

    if use_peep:
        cp = run_cmd([dccpeep, str(app_mac), str(peep_tmp)], env=env)
        if cp.returncode != 0 or not peep_tmp.exists():
            raise RuntimeError(f"dccpeep failed for {app}: {cp.stdout.strip()}")
        os.replace(peep_tmp, app_mac)

    convert_to_crlf(app_mac)

    cp = run_cmd([ntvcm, m80, f"={upper}.MAC", "/X", "/O", "/Z", "/L"], cwd=build_dir, env=env)
    if cp.returncode != 0:
        raise RuntimeError(f"M80 failed for {app}: {cp.stdout.strip()}")

    strip_args = [dccrtlstrip]
    if floatio:
        strip_args.extend(["-k", "_pffio"])
    strip_args.extend(["-r", str(root_rtl), "-o", str(rtl_min), str(app_mac)])
    cp = run_cmd(strip_args, env=env)
    if cp.returncode != 0 or not rtl_min.exists():
        raise RuntimeError(f"dccrtlstrip failed for {app}: {cp.stdout.strip()}")

    convert_to_crlf(rtl_min)

    cp = run_cmd([ntvcm, m80, "=RTLMIN.MAC", "/X", "/O", "/Z"], cwd=build_dir, env=env)
    if cp.returncode != 0:
        raise RuntimeError(f"M80 RTLMIN failed for {app}: {cp.stdout.strip()}")

    cp = run_cmd([ntvcm, l80, f"/P:100,RTLMIN,{upper},{upper}/N/E"], cwd=build_dir, env=env)
    if cp.returncode != 0 or not app_com.exists():
        raise RuntimeError(f"L80 failed for {app}: {cp.stdout.strip()}")

    return True


def fixture_files() -> list[str]:
    tests = Path("tests")
    if not tests.is_dir():
        return []
    fixtures = []
    for path in tests.iterdir():
        if not path.is_file():
            continue
        if path.name.startswith("."):
            continue
        if path.suffix in {".c", ".json", ".md"}:
            continue
        fixtures.append(path.name)
    return fixtures


def copy_fixture_upper(name: str, dest_dir: Path) -> None:
    dest = dest_dir / name.upper()
    needle = name.lower()
    for directory in (Path("tests"), Path(".")):
        if not directory.is_dir():
            continue
        for src in directory.iterdir():
            if src.is_file() and src.name.lower() == needle:
                try:
                    shutil.copyfile(src, dest)
                except OSError:
                    pass
                return


def normalize_output(text: str) -> str:
    return text.replace("\r\n", "\n").rstrip("\n")


def matches_baseline(actual: str, baseline: str) -> bool:
    if not re.search(r"\{\{[A-Z]+\}\}", baseline):
        return actual == baseline
    parts: list[str] = []
    last = 0
    for match in TOKEN_RE.finditer(baseline):
        parts.append(re.escape(baseline[last:match.start()]))
        token = "{{" + match.group(1) + "}}"
        parts.append(PLACEHOLDERS.get(token, re.escape(match.group(0))))
        last = match.end()
    parts.append(re.escape(baseline[last:]))
    return re.fullmatch("".join(parts), actual) is not None


def run_app(app: str, run_args: str, build_dir: Path, emulator: str, env: dict[str, str], timeout: int) -> str:
    upper = app.upper()
    argv = [emulator, f"{upper}.COM"]
    if run_args:
        argv.extend(run_args.split())
    stdin_data = "x\n" if app == "tkbd" else None
    cp = subprocess.run(
        argv,
        cwd=str(build_dir),
        env=env,
        input=stdin_data,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        timeout=timeout,
    )
    return cp.stdout


def invoke_app_test(item: WorkItem, modes: list[str], build_root: Path, baseline_dir: Path, emulator: str, stack_check: bool, fixtures: list[str], timeout: int) -> Result:
    start = time.perf_counter()
    lines: list[str] = []
    passed = True
    build_dir = build_root / item.app
    build_dir.mkdir(parents=True, exist_ok=True)
    for fixture in fixtures:
        copy_fixture_upper(fixture, build_dir)

    env = os.environ.copy()
    if stack_check:
        env["DCC_FORCE_STACK_CHECK"] = "1"
    else:
        env.pop("DCC_FORCE_STACK_CHECK", None)
    if item.stack_size:
        env["DCC_STACK_SIZE"] = str(item.stack_size)
    else:
        env.pop("DCC_STACK_SIZE", None)

    for mode in modes:
        try:
            build_app(item.app, mode, build_dir, emulator, env)
            lines.append(f"  Building {item.app} ({mode})... done")
        except Exception as exc:
            lines.append(f"  Building {item.app} ({mode})... FAILED: {exc}")
            passed = False
            break

        try:
            output = run_app(item.app, item.args, build_dir, emulator, env, timeout)
        except Exception as exc:
            lines.append(f"    ERROR running {item.app}: {exc}")
            passed = False
            continue

        baseline_path = baseline_dir / f"{item.app}.txt"
        if baseline_path.is_file():
            expected = normalize_output(baseline_path.read_text(encoding="utf-8"))
            actual = normalize_output(output)
            if matches_baseline(actual, expected):
                lines.append("    Output matches baseline")
            else:
                preview = " | ".join(actual.split("\n")[:3])
                lines.append(f"    OUTPUT MISMATCH (vs {baseline_path})")
                lines.append(f"    Got: {preview}")
                passed = False
        else:
            lines.append(f"    ERROR: no baseline at {baseline_path} (every app must have one)")
            passed = False

    return Result(item.app, passed, time.perf_counter() - start, lines)


def load_overrides(path: Path) -> dict[str, dict[str, object]]:
    if not path.exists():
        return {}
    data = json.loads(path.read_text(encoding="utf-8"))
    return {app["name"]: app for app in data.get("apps", [])}


def elapsed_text(seconds: float) -> str:
    if seconds >= 60:
        minutes = int(seconds // 60)
        rem = seconds - minutes * 60
        return f"{minutes}m {rem:.1f}s"
    return f"{seconds:.2f}s"


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description="Build and run all dcc tests")
    parser.add_argument("--emulator", default="ntvcm")
    parser.add_argument("--no-stack-check", action="store_true")
    parser.add_argument("--build-dir", default="build")
    parser.add_argument("--baseline-dir", default="tests/baselines")
    parser.add_argument("--mode", choices=("peep", "nopeep", "both"), default="both")
    parser.add_argument("--run-timeout", type=int, default=60)
    parser.add_argument("--serial", action="store_true")
    parser.add_argument("--throttle-limit", type=int, default=os.cpu_count() or 1)
    parser.add_argument("--app", help="build one app and exit (ma.ps1 compatibility path)")
    args = parser.parse_args(argv)

    if args.app:
        env = os.environ.copy()
        try:
            build_app(args.app, args.mode, Path(args.build_dir), args.emulator, env)
        except Exception as exc:
            print(exc, file=sys.stderr)
            return 1
        print(f"Build successful: {Path(args.build_dir) / (args.app.upper() + '.COM')}")
        return 0

    stack_check = not args.no_stack_check
    if stack_check:
        print("--- stack-check: building every app with -fstack-check (default; use --no-stack-check to disable) ---")
    else:
        print("--- stack-check disabled (--no-stack-check) ---")

    overrides = load_overrides(Path("tests/_test_overrides.json"))
    test_files = sorted(path.stem for path in Path("tests").glob("*.c"))
    if not test_files:
        print("No test files found in tests", file=sys.stderr)
        return 1
    print(f"Found {len(test_files)} test applications")

    baseline_dir = Path(args.baseline_dir)
    if baseline_dir.is_dir():
        baseline_count = len(list(baseline_dir.glob("*.txt")))
        print(f"Using per-app baselines from {baseline_dir} ({baseline_count} files)")
    else:
        print(f"No baseline directory at {baseline_dir}; running without verification")

    skipped = 0
    work: list[WorkItem] = []
    for app in test_files:
        override = overrides.get(app, {})
        if override.get("ignore"):
            skipped += 1
            continue
        source = resolve_source(app)
        size = source.stat().st_size if source else 0
        stack_size = str(override.get("stack_size") or os.environ.get("STACK_SIZE") or "")
        work.append(WorkItem(app=app, args=str(override.get("args") or ""), stack_size=stack_size, source_size=size))
    work.sort(key=lambda item: item.source_size, reverse=True)

    modes = ["peep", "nopeep"] if args.mode == "both" else [args.mode]
    fixtures = fixture_files()

    print()
    print("========================================")
    print("STARTING BUILD AND RUN SUITE")
    if not args.serial:
        print(f"(parallel, throttle = {args.throttle_limit})")
    print("========================================")

    start = time.perf_counter()
    results: list[Result] = []
    total = len(work)
    build_root = Path(args.build_dir)

    if args.serial:
        for index, item in enumerate(work, 1):
            result = invoke_app_test(item, modes, build_root, baseline_dir, args.emulator, stack_check, fixtures, args.run_timeout)
            results.append(result)
            status = "PASS" if result.passed else "FAIL"
            print(f"[{index:3d}/{total}] {status}  {result.app:<12} {elapsed_text(result.elapsed):>8} | {'Stack Check Enabled' if stack_check else 'No Stack Check'}")
            if not result.passed:
                for line in result.lines:
                    if any(word in line for word in ("FAILED", "MISMATCH", "WARNING", "ERROR")):
                        print(f"        {line.strip()}")
    else:
        with concurrent.futures.ThreadPoolExecutor(max_workers=args.throttle_limit) as executor:
            future_map = {
                executor.submit(invoke_app_test, item, modes, build_root, baseline_dir, args.emulator, stack_check, fixtures, args.run_timeout): item
                for item in work
            }
            done = 0
            for future in concurrent.futures.as_completed(future_map):
                result = future.result()
                results.append(result)
                done += 1
                status = "PASS" if result.passed else "FAIL"
                print(f"[{done:3d}/{total}] {status}  {result.app:<12} {elapsed_text(result.elapsed):>8} | {'Stack Check Enabled' if stack_check else 'No Stack Check'}")
                if not result.passed:
                    for line in result.lines:
                        if any(word in line for word in ("FAILED", "MISMATCH", "WARNING", "ERROR")):
                            print(f"        {line.strip()}")

    elapsed = time.perf_counter() - start
    passed = sum(1 for result in results if result.passed)
    failed = len(results) - passed
    failed_apps = [result.app for result in results if not result.passed]

    print()
    print("========================================")
    print("TEST SUITE SUMMARY")
    print("========================================")
    print(f"  Total apps:   {len(test_files)}")
    print(f"  Passed:       {passed}")
    print(f"  Failed:       {failed}")
    print(f"  Skipped:      {skipped}")
    print(f"  Total time:   {elapsed_text(elapsed)}")
    if failed_apps:
        print()
        print("Failed apps:")
        for app in failed_apps:
            print(f"  - {app}")
    print()
    if failed == 0:
        print(">>> SUCCESS: All tests passed <<<")
        return 0
    print(f">>> FAILURE: {failed} test(s) failed <<<")
    return 1


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
