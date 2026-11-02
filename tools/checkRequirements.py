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
    if req.get("module"):
        command = shutil.which("python3")
        if command is None:
            return "missing", None, None
        module = req["module"]
        args = [
            command, "-c",
            f"import {module}; print({module}.__version__)",
        ]
    else:
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
    if proc.returncode != 0:
        lines = [line.strip() for line in output.splitlines() if line.strip()]
        detail = lines[-1] if lines else f"module {req.get('module')} missing"
        return "missing", None, detail
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




_NVIDIA_GPU = None


def has_nvidia_gpu() -> bool:
    global _NVIDIA_GPU
    if _NVIDIA_GPU is not None:
        return _NVIDIA_GPU

    smi = shutil.which("nvidia-smi")
    if smi:
        try:
            proc = subprocess.run(
                [smi, "-L"], capture_output=True, text=True, timeout=10,
                check=False
            )
            if proc.returncode == 0 and proc.stdout.strip():
                _NVIDIA_GPU = True
                return True
        except (OSError, subprocess.SubprocessError):
            pass

    lspci = shutil.which("lspci")
    if lspci:
        try:
            proc = subprocess.run(
                [lspci], capture_output=True, text=True, timeout=10, check=False
            )
            if "nvidia" in ((proc.stdout or "") + (proc.stderr or "")).lower():
                _NVIDIA_GPU = True
                return True
        except (OSError, subprocess.SubprocessError):
            pass

    devices = Path("/sys/bus/pci/devices")
    if devices.is_dir():
        for vendor in devices.glob("*/vendor"):
            try:
                if vendor.read_text().strip().lower() == "0x10de":
                    _NVIDIA_GPU = True
                    return True
            except OSError:
                pass

    _NVIDIA_GPU = False
    return False


def requirement_applies(req: dict) -> bool:
    condition = req.get("condition")
    if condition == "nvidia-gpu":
        return has_nvidia_gpu()
    return True


def kokkos_settings(data: dict) -> dict:
    cfg = data.get("kokkos", {})
    return {
        "version": cfg.get("version", "4.7.04"),
        "cpu_prefix": Path(os.path.expanduser(
            cfg.get("cpu_prefix", "~/.local/kokkos-cpu"))),
        "cuda_prefix": Path(os.path.expanduser(
            cfg.get("cuda_prefix", "~/.local/kokkos-cuda"))),
        "source_cache": Path(os.path.expanduser(
            cfg.get("source_cache", "~/.cache/saturnCFD/kokkos"))),
    }


def kokkos_config(prefix: Path):
    for libdir in ("lib", "lib64", "share"):
        config = prefix / libdir / "cmake" / "Kokkos" / "KokkosConfig.cmake"
        if config.is_file():
            return config
    return None


def find_kokkos(prefixes, want_cuda: bool):
    for prefix in prefixes:
        config = kokkos_config(prefix)
        if config is None:
            continue
        header = prefix / "include" / "kokkos" / "KokkosCore_config.h"
        if not header.is_file():
            header = prefix / "include" / "KokkosCore_config.h"
        text = header.read_text(errors="ignore") if header.is_file() else ""
        has_cuda = "KOKKOS_ENABLE_CUDA" in text
        has_openmp = "KOKKOS_ENABLE_OPENMP" in text
        has_serial = "KOKKOS_ENABLE_SERIAL" in text
        if want_cuda and not has_cuda:
            continue
        if not want_cuda and not has_openmp:
            continue
        return prefix
    return None


def report_kokkos(data: dict):
    cfg = kokkos_settings(data)
    gpu = has_nvidia_gpu()
    root = os.environ.get("KOKKOS_ROOT")
    cpu_candidates = []
    if root:
        cpu_candidates.append(Path(os.path.expanduser(root)))
    cpu_candidates += [cfg["cpu_prefix"], Path.home() / ".local",
                       Path("/usr/local"), Path("/usr")]
    cuda_candidates = []
    if root:
        cuda_candidates.append(Path(os.path.expanduser(root)))
    cuda_candidates += [cfg["cuda_prefix"]]

    cpu = find_kokkos(cpu_candidates, want_cuda=False)
    cuda = find_kokkos(cuda_candidates, want_cuda=True) if gpu else None

    print()
    print("Kokkos:")
    if cpu:
        print(f"  [OK     ] kokkos-cpu  {cpu}")
    else:
        print("  [MISSING] kokkos-cpu")
    if gpu:
        if cuda:
            print(f"  [OK     ] kokkos-cuda {cuda}")
        else:
            print("  [MISSING] kokkos-cuda")
    else:
        print("  [SKIP   ] kokkos-cuda (no NVIDIA GPU)")

    missing = []
    if cpu is None:
        missing.append("kokkos-cpu")
    if gpu and cuda is None:
        missing.append("kokkos-cuda")
    return missing


def kokkos_source(data: dict, install: bool):
    cfg = kokkos_settings(data)
    candidates = [
        ROOT / "buildCPU" / "_deps" / "kokkos-src",
        ROOT / "buildGPU" / "_deps" / "kokkos-src",
        cfg["source_cache"],
    ]
    for candidate in candidates:
        if (candidate / "CMakeLists.txt").is_file():
            return candidate
    if not install:
        return None

    cache = cfg["source_cache"]
    if cache.exists():
        shutil.rmtree(cache)
    cache.parent.mkdir(parents=True, exist_ok=True)
    print(f"Cloning Kokkos {cfg['version']} into {cache}")
    try:
        subprocess.run(
            ["git", "clone", "--depth", "1", "--branch", cfg["version"],
             "https://github.com/kokkos/kokkos.git", str(cache)],
            check=True,
        )
    except (OSError, subprocess.SubprocessError) as exc:
        print(f"error: cannot clone Kokkos: {exc}", file=sys.stderr)
        return None
    return cache


def build_kokkos(source: Path, prefix: Path, cuda: bool, version: str) -> bool:
    if not source:
        return False
    source = source.resolve()
    if not (source / "CMakeLists.txt").is_file():
        return False
    build = Path.home() / ".cache" / "saturnCFD" / (
        "build-kokkos-cuda" if cuda else "build-kokkos-cpu")
    if build.exists():
        shutil.rmtree(build)
    prefix.mkdir(parents=True, exist_ok=True)

    command = [
        "cmake", "-S", str(source), "-B", str(build),
        "-DCMAKE_BUILD_TYPE=Release",
        f"-DCMAKE_INSTALL_PREFIX={prefix}",
        "-DKokkos_ENABLE_TESTS=OFF",
        "-DKokkos_ENABLE_EXAMPLES=OFF",
    ]
    if cuda:
        wrapper = source / "bin" / "nvcc_wrapper"
        if not wrapper.is_file() or shutil.which("nvcc") is None:
            print("error: CUDA Kokkos needs nvcc and the Kokkos nvcc_wrapper",
                  file=sys.stderr)
            return False
        command += [
            "-DKokkos_ENABLE_CUDA=ON",
            "-DKokkos_ENABLE_CUDA_LAMBDA=ON",
            "-DKokkos_ENABLE_OPENMP=OFF",
            "-DKokkos_ENABLE_SERIAL=ON",
            "-DKokkos_ARCH_NATIVE=ON",
            f"-DCMAKE_CXX_COMPILER={wrapper}",
        ]
    else:
        command += [
            "-DKokkos_ENABLE_CUDA=OFF",
            "-DKokkos_ENABLE_OPENMP=ON",
            "-DKokkos_ENABLE_SERIAL=ON",
        ]

    print(f"Building Kokkos {version} ({'CUDA' if cuda else 'OpenMP'}) -> {prefix}")
    try:
        subprocess.run(command, check=True)
        subprocess.run(["cmake", "--build", str(build), "-j"], check=True)
        subprocess.run(["cmake", "--install", str(build)], check=True)
    except (OSError, subprocess.SubprocessError) as exc:
        print(f"error: Kokkos build failed: {exc}", file=sys.stderr)
        return False
    return True


def ensure_kokkos(data: dict, install: bool, core_only: bool) -> None:
    if not install:
        return
    missing = report_kokkos(data)
    if not missing:
        return
    cfg = kokkos_settings(data)
    source = kokkos_source(data, install=True)
    if source is None:
        print("error: no Kokkos source available to compile", file=sys.stderr)
        return

    if "kokkos-cpu" in missing:
        build_kokkos(source, cfg["cpu_prefix"], cuda=False,
                     version=cfg["version"])
    if "kokkos-cuda" in missing and has_nvidia_gpu() and not core_only:
        build_kokkos(source, cfg["cuda_prefix"], cuda=True,
                     version=cfg["version"])
    report_kokkos(data)

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
        if not requirement_applies(req):
            print(
                f"[{'SKIP':7}] {req_id:14} {'-':12} {'-':10} "
                f"{group:7} {'optional':8} {req.get('purpose', '')} "
                "(condition not met)"
            )
            continue
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

    kokkos_missing = report_kokkos(data)
    optional_missing.extend(kokkos_missing)

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

    if args.install:
        packages = sorted(set(missing_core_apt if args.core_only else missing_apt))
        if packages:
            command = ["apt-get", "install", "-y"] + packages
            if os.geteuid() != 0:
                if shutil.which("sudo") is None:
                    print("\nerror: sudo is not available; install packages manually", file=sys.stderr)
                    return 1
                command = ["sudo"] + command
            print("\nInstalling apt packages: " + " ".join(packages))
            try:
                update = (["sudo", "apt-get", "update"]
                          if os.geteuid() != 0 else ["apt-get", "update"])
                subprocess.run(update, check=False)
                subprocess.run(command, check=False)
            except (OSError, subprocess.SubprocessError) as exc:
                print(f"error: package installation failed: {exc}", file=sys.stderr)
                return 1
        else:
            print("\nNo missing apt packages to install.")

        ensure_kokkos(data, install=True, core_only=args.core_only)

    if failures:
        return 1
    if args.strict and optional_missing:
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
