#!/usr/bin/env python3
"""Check and optionally install the host requirements in requirements.toml."""

from __future__ import annotations

import argparse
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

try:
    import tomllib
except ModuleNotFoundError:
    print("error: Python 3.11+ is required (tomllib is missing)", file=sys.stderr)
    sys.exit(2)

ROOT = Path(__file__).resolve().parents[1]


def parse_version(text: str, pattern: str):
    match = re.search(pattern, text)
    if not match:
        return None
    return tuple(int(part) for part in match.group(1).split("."))


def padded(value: tuple[int, ...], size: int) -> tuple[int, ...]:
    return value + (0,) * max(0, size - len(value))


def at_least(found: tuple[int, ...], minimum: tuple[int, ...]) -> bool:
    size = max(len(found), len(minimum))
    return padded(found, size) >= padded(minimum, size)


def run_requirement(req: dict, default_regex: str):
    command = shutil.which(req["command"])
    if command is None:
        return "missing", None, None

    args = [command] + list(req.get("args", ["--version"]))
    try:
        proc = subprocess.run(
            args, capture_output=True, text=True, timeout=20, check=False
        )
    except (OSError, subprocess.SubprocessError) as exc:
        return "error", None, str(exc)

    output = (proc.stdout or "") + (proc.stderr or "")
    version = parse_version(output, req.get("version_regex", default_regex))
    first_line = next((line.strip() for line in output.splitlines() if line.strip()), "")
    return "found", version, first_line


def check_cxx20(compiler: str) -> bool:
    command = shutil.which(compiler)
    if command is None:
        return False
    source = "#include <version>\nint main() { return 0; }\n"
    try:
        proc = subprocess.run(
            [command, "-std=c++20", "-x", "c++", "-", "-fsyntax-only"],
            input=source,
            text=True,
            capture_output=True,
            timeout=30,
            check=False,
        )
        return proc.returncode == 0
    except (OSError, subprocess.SubprocessError):
        return False


def version_text(value) -> str:
    if value is None:
        return "unknown"
    return ".".join(str(part) for part in value)


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Check saturnCFD host requirements from requirements.toml"
    )
    parser.add_argument(
        "--toml",
        default=str(ROOT / "requirements.toml"),
        help="requirements TOML file",
    )
    parser.add_argument(
        "--install",
        action="store_true",
        help="install missing apt packages (uses sudo when not root)",
    )
    parser.add_argument(
        "--core-only",
        action="store_true",
        help="with --install, install only required/core packages",
    )
    parser.add_argument(
        "--strict",
        action="store_true",
        help="treat missing optional requirements as failure",
    )
    args = parser.parse_args()

    path = Path(args.toml)
    if not path.exists():
        print(f"error: requirements file not found: {path}", file=sys.stderr)
        return 2

    with path.open("rb") as handle:
        data = tomllib.load(handle)

    project = data.get("project", {})
    default_regex = data.get("defaults", {}).get(
        "version_regex", r"(\d+(?:\.\d+){1,3})"
    )

    print(f"{project.get('name', 'saturnCFD')} host requirements")
    print("=" * 68)
    print(f"requirements: {path}")
    print(f"python:       {sys.version.split()[0]}")
    print()

    failures = []
    missing_apt = []
    missing_core_apt = []
    optional_missing = []

    for req in data.get("requirements", []):
        req_id = req.get("id", req.get("command", "?"))
        required = bool(req.get("required", False))
        group = req.get("group", "core")
        minimum = parse_version(req.get("min_version", "0"), r"(\d+(?:\.\d+)*)")
        status, found, detail = run_requirement(req, default_regex)

        if status == "missing":
            state = "MISSING"
            failures.append(req_id) if required else optional_missing.append(req_id)
            if req.get("apt"):
                missing_apt.extend(req["apt"].split())
                if required:
                    missing_core_apt.extend(req["apt"].split())
        elif status == "error":
            state = "ERROR"
            failures.append(req_id) if required else optional_missing.append(req_id)
        elif found is None:
            state = "UNKNOWN"
            failures.append(req_id) if required else optional_missing.append(req_id)
        elif minimum is not None and not at_least(found, minimum):
            state = "OLD"
            failures.append(req_id) if required else optional_missing.append(req_id)
            if req.get("apt"):
                missing_apt.extend(req["apt"].split())
                if required:
                    missing_core_apt.extend(req["apt"].split())
        else:
            state = "OK"

        requirement = "required" if required else "optional"
        minimum_text = version_text(minimum) if minimum is not None else "-"
        found_text = version_text(found)
        print(
            f"[{state:7}] {req_id:14} {found_text:12} "
            f">= {minimum_text:10} {group:7} {requirement:8} "
            f"{req.get('purpose', '')}"
        )
        if state in {"MISSING", "ERROR", "UNKNOWN"} and detail:
            print(f"          {detail}")

    cxx20 = check_cxx20("c++")
    print(
        f"[{'OK' if cxx20 else 'FAIL':7}] {'c++20':14} "
        f"{'available' if cxx20 else 'not available':12} "
        f"{'':10} core    required C++20 language support"
    )
    if not cxx20:
        failures.append("c++20")

    fetched = data.get("fetched", [])
    if fetched:
        print()
        print("Fetched and compiled by CMake:")
        for item in fetched:
            print(
                f"  {item.get('id', '?'):12} {item.get('version', '?'):10} "
                f"{item.get('source', '')}"
            )

    notes = data.get("notes", [])
    if notes:
        print()
        print("Notes:")
        for note in notes:
            print(f"  - {note}")

    print()
    if failures:
        print(f"Required failures: {', '.join(failures)}")
    else:
        print("Required requirements: OK")
    if optional_missing:
        print(f"Optional missing/outdated: {', '.join(sorted(set(optional_missing)))}")

    if args.install and missing_apt:
        packages = sorted(set(missing_core_apt if args.core_only else missing_apt))
        if not packages:
            print("\nNo missing core apt packages to install.")
            return 1 if failures else 0
        command = ["apt-get", "install", "-y"] + packages
        if os.geteuid() != 0:
            if shutil.which("sudo") is None:
                print("\nerror: sudo is not available; install packages manually", file=sys.stderr)
                return 1
            command = ["sudo"] + command
        print("\nInstalling apt packages: " + " ".join(packages))
        try:
            subprocess.run(["sudo", "apt-get", "update"] if os.geteuid() != 0 else ["apt-get", "update"], check=False)
            subprocess.run(command, check=False)
        except (OSError, subprocess.SubprocessError) as exc:
            print(f"error: package installation failed: {exc}", file=sys.stderr)
            return 1
        print("Re-run this checker after installation.")
    elif args.install:
        print("\nNo missing apt packages to install.")

    if failures:
        return 1
    if args.strict and optional_missing:
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
