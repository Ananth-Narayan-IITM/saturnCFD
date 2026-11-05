#!/usr/bin/env python3
"""Shared helpers for the standalone ``saturnCFD`` benchmark scripts.

The benchmark scripts deliberately use only the Python standard library so
that they can be run on a login node or a minimal benchmark host without
NumPy or Matplotlib.  They parse the solver's CSV profile, recompute the
analytic solution independently, and compare the CPU and GPU profiles.
"""

from __future__ import annotations

import csv
import json
import math
import os
import shutil
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

# Default L2 error accepted as "the discretisation is doing its job".
DEFAULT_TOLERANCE = 5.0e-3
# CPU and GPU are the same discrete scheme; only round-off should separate
# them.  Relative L2 is a robust scale-free check.
DEFAULT_AGREEMENT = 1.0e-8


class BenchmarkError(RuntimeError):
    """A benchmark run or a consistency check failed."""


def resolve_cli(value: str) -> str:
    """Resolve an executable name or an explicit path."""
    path = Path(value)
    if path.exists():
        return str(path.resolve())
    found = shutil.which(value)
    if found:
        return found
    raise BenchmarkError(
        f"missing executable: {value}\n"
        "install the backends with tools/compileSaturnCFD, or pass a path"
    )


def run_case(cli: str, case: Path, cwd: Path,
             env: dict[str, str] | None = None) -> subprocess.CompletedProcess:
    """Run one case and raise a useful error if the solver fails."""
    case = Path(case)
    result = subprocess.run(
        [cli, case.name],
        cwd=cwd,
        env=env,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    if result.returncode != 0:
        raise BenchmarkError(
            f"{cli} failed ({result.returncode})\n"
            f"case: {case}\n"
            f"--- stdout ---\n{result.stdout}\n"
            f"--- stderr ---\n{result.stderr}"
        )
    return result


def cpu_environment(threads: int) -> dict[str, str]:
    return {
        **os.environ,
        "OMP_NUM_THREADS": str(threads),
        "OMP_PROC_BIND": "spread",
        "OMP_PLACES": "threads",
    }


def load_json(path: Path) -> dict:
    with open(path, encoding="utf-8") as stream:
        return json.load(stream)


def load_csv(path: Path) -> dict[str, list[float]]:
    """Read a solver profile CSV into ``{column: [values]}``."""
    with open(path, newline="", encoding="utf-8") as stream:
        reader = csv.DictReader(stream)
        if reader.fieldnames is None:
            raise BenchmarkError(f"empty profile CSV: {path}")
        data: dict[str, list[float]] = {name: [] for name in reader.fieldnames}
        for row in reader:
            for name in reader.fieldnames:
                data[name].append(float(row[name]))
    if not data:
        raise BenchmarkError(f"empty profile CSV: {path}")
    return data


def error_metrics(numerical: list[float], exact: list[float]) -> dict[str, float]:
    """L1/L2/Linf of ``numerical - exact``."""
    if len(numerical) != len(exact):
        raise BenchmarkError(
            f"profile length mismatch: {len(numerical)} vs {len(exact)}"
        )
    if not numerical:
        raise BenchmarkError("empty profile")
    count = len(numerical)
    l1 = 0.0
    l2 = 0.0
    linf = 0.0
    for n, e in zip(numerical, exact):
        value = n - e
        l1 += abs(value)
        l2 += value * value
        linf = max(linf, abs(value))
    return {
        "l1": l1 / count,
        "l2": math.sqrt(l2 / count),
        "linf": linf,
        "samples": count,
    }


def difference_metrics(a: list[float], b: list[float]) -> dict[str, float]:
    """Max/RMS/L2 difference and relative L2 between two profiles."""
    if len(a) != len(b):
        raise BenchmarkError(f"profile length mismatch: {len(a)} vs {len(b)}")
    if not a:
        raise BenchmarkError("empty profile")
    max_abs = 0.0
    diff2 = 0.0
    ref2 = 0.0
    for x, y in zip(a, b):
        d = x - y
        max_abs = max(max_abs, abs(d))
        diff2 += d * d
        ref2 += x * x
    rms = math.sqrt(diff2 / len(a))
    l2 = math.sqrt(diff2)
    relative = math.sqrt(diff2 / ref2) if ref2 > 0 else (0.0 if diff2 == 0 else math.inf)
    return {
        "maxAbs": max_abs,
        "rms": rms,
        "l2": l2,
        "relativeL2": relative,
    }


def check_same_grid(a: dict[str, list[float]],
                    b: dict[str, list[float]],
                    coordinates: tuple[str, ...] = ("x", "y", "z"),
                    tolerance: float = 1.0e-12) -> None:
    """Fail if two profiles do not describe the same points."""
    for name in coordinates:
        if name not in a or name not in b:
            continue
        if len(a[name]) != len(b[name]):
            raise BenchmarkError(
                f"coordinate '{name}' length mismatch: "
                f"{len(a[name])} vs {len(b[name])}"
            )
        for left, right in zip(a[name], b[name]):
            if not math.isclose(left, right, rel_tol=tolerance,
                                abs_tol=tolerance):
                raise BenchmarkError(
                    f"grid mismatch in '{name}': {left:.17g} vs {right:.17g}"
                )


def independent_consistency(profile: dict[str, list[float]],
                            analytic: list[float],
                            column: str = "u_exact") -> float:
    """Max distance between the solver's exact column and our analytic value."""
    if column not in profile:
        return 0.0
    solver = profile[column]
    if len(solver) != len(analytic):
        raise BenchmarkError("exact-column length mismatch")
    return max((abs(s - a) for s, a in zip(solver, analytic)), default=0.0)


def format_error_values(metrics: dict[str, float]) -> str:
    return (
        f"L1={metrics['l1']:.6e}  "
        f"L2={metrics['l2']:.6e}  "
        f"Linf={metrics['linf']:.6e}"
    )


def format_metrics(label: str, metrics: dict[str, float]) -> str:
    return f"{label:<10s} {format_error_values(metrics)}"


def write_text(path: Path, text: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text.rstrip() + "\n", encoding="utf-8")


def write_json(path: Path, payload: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")


def print_header(title: str) -> None:
    print(title)
    print("=" * len(title))
