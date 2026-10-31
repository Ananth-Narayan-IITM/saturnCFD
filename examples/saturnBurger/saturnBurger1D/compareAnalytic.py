#!/usr/bin/env python3
"""Standalone analytic comparison for the 1D Burgers example.

This script is intentionally independent of the solver's `u_exact` column: it
re-reads the case parameters from `saturnBurger.yaml`, recomputes

    u(x,t) = U/2 * (1 - tanh(U*(x - x0 - U*t/2)/(4*nu)))

and compares it with the numerical profile written by `saturnBurger`.

Usage, from this directory:

    saturnCFD saturnBurger.yaml
    python3 compareAnalytic.py

or let the script run the case first:

    python3 compareAnalytic.py --cli /path/to/saturnCFD

The script writes `comparison.png` and `.pdf` and exits
non-zero if the L2 error exceeds `--tolerance` (default 1e-4).
"""

from __future__ import annotations

import argparse
import json
import math
import subprocess
import sys
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np

HERE = Path(__file__).resolve().parent


def read_simple_yaml(path: Path) -> dict:
    """Read the small, flat-by-section YAML used by this example."""
    sections: dict[str, dict[str, str]] = {}
    top: dict[str, str] = {}
    section: str | None = None
    for raw in path.read_text().splitlines():
        line = raw.split("#", 1)[0].rstrip()
        if not line.strip():
            continue
        if not line.startswith((" ", "\t")) and line.rstrip().endswith(":"):
            section = line.strip()[:-1]
            sections.setdefault(section, {})
            continue
        if ":" not in line:
            continue
        key, value = line.split(":", 1)
        key = key.strip()
        value = value.strip().strip('"').strip("'")
        if not value:
            continue
        if section is None:
            top[key] = value
        else:
            sections[section][key] = value
    return {**top, **sections}


def as_float(values: dict, section: str, key: str, default: float) -> float:
    try:
        return float(values[section][key])
    except (KeyError, ValueError):
        return default


def as_string(values: dict, section: str, key: str, default: str) -> str:
    try:
        return str(values[section][key])
    except KeyError:
        return default


def exact_solution(x, t, speed, nu, x0):
    return 0.5 * speed * (
        1.0
        - np.tanh(
            speed * (x - x0 - 0.5 * speed * t) / (4.0 * nu)
        )
    )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--case", type=Path, default=HERE / "saturnBurger.yaml")
    parser.add_argument("--cli", default="saturnCFD",
                        help="saturnCFD executable (default: from PATH)")
    parser.add_argument("--no-run", action="store_true",
                        help="use the existing CSV instead of running the solver")
    parser.add_argument("--tolerance", type=float, default=1.0e-4,
                        help="L2 tolerance for PASS/FAIL (default: 1e-4)")
    parser.add_argument("--out", type=Path, default=None,
                        help="plot path without extension")
    args = parser.parse_args()

    case = args.case.resolve()
    if not case.exists():
        raise SystemExit(f"missing case file: {case}")

    values = read_simple_yaml(case)
    nu = as_float(values, "burgers", "nu", 0.02)
    speed = as_float(values, "burgers", "velocity", 1.0)
    x0 = as_float(values, "burgers", "x0", 0.0)
    t_end = as_float(values, "time", "end", 0.5)
    prefix = as_string(values, "output", "prefix", "results")
    output_base = case.parent / prefix
    csv_path = output_base.with_suffix(".csv")
    json_path = output_base.with_suffix(".json")

    if not args.no_run:
        command = [args.cli, case.name]
        print("+", " ".join(command))
        subprocess.run(command, cwd=case.parent, check=True)

    if not csv_path.exists():
        raise SystemExit(
            f"missing profile CSV: {csv_path}\n"
            "run the case first or set output.csv: true"
        )

    data = np.genfromtxt(csv_path, delimiter=",", names=True)
    data = np.atleast_1d(data)
    x = np.asarray(data["x"], dtype=float)
    numerical = np.asarray(data["u_numerical"], dtype=float)

    analytic = exact_solution(x, t_end, speed, nu, x0)
    error = numerical - analytic
    l1 = float(np.mean(np.abs(error)))
    l2 = float(math.sqrt(np.mean(error * error)))
    linf = float(np.max(np.abs(error)))

    solver_exact = np.asarray(data["u_exact"], dtype=float)
    exact_consistency = float(np.max(np.abs(solver_exact - analytic)))

    print()
    print(f"case        : {case}")
    print(f"samples     : {x.size}")
    print(f"t_end       : {t_end:g} s")
    print(f"U, nu, x0   : {speed:g}, {nu:g}, {x0:g}")
    print(f"L1 error    : {l1:.6e}")
    print(f"L2 error    : {l2:.6e}")
    print(f"Linf error  : {linf:.6e}")
    print(f"max |u_exact column - independent analytic| : {exact_consistency:.3e}")

    if json_path.exists():
        summary = json.loads(json_path.read_text())
        if "exact" in summary:
            print(
                "solver JSON : "
                f"L1={summary['exact']['l1']:.6e} "
                f"L2={summary['exact']['l2']:.6e} "
                f"Linf={summary['exact']['linf']:.6e}"
            )

    passed = l2 <= args.tolerance
    print(
        ("PASS" if passed else "FAIL")
        + f": L2 {l2:.6e} {'<=' if passed else '>'} tolerance {args.tolerance:.1e}"
    )

    out = args.out if args.out is not None else output_base.parent / "comparison"
    out.parent.mkdir(parents=True, exist_ok=True)
    fig, (ax_profile, ax_error) = plt.subplots(
        2, 1, figsize=(9.0, 6.4), sharex=True,
        gridspec_kw={"height_ratios": [2.2, 1.0]},
    )
    ax_profile.plot(x, analytic, color="black", linewidth=2.0,
                    label="analytic")
    ax_profile.plot(x, numerical, "o", color="#1f77b4", markersize=2.5,
                    label="saturnBurger")
    ax_profile.set_ylabel("u [m/s]")
    ax_profile.set_title(
        f"1D Burgers traveling wave at t={t_end:g} s "
        f"(U={speed:g}, nu={nu:g}, N={x.size})"
    )
    ax_profile.legend(loc="best")
    ax_profile.grid(alpha=0.3)

    ax_error.axhline(0.0, color="0.6", linewidth=0.8)
    ax_error.plot(x, error, color="#d62728", linewidth=1.2)
    ax_error.set_xlabel("x [m]")
    ax_error.set_ylabel(r"$u_h-u_{\rm exact}$ [m/s]")
    ax_error.grid(alpha=0.3)
    ax_error.text(
        0.02, 0.90,
        f"L1={l1:.3e}   L2={l2:.3e}   Linf={linf:.3e}",
        transform=ax_error.transAxes,
        fontsize=9,
        va="top",
        bbox={"facecolor": "white", "alpha": 0.85, "edgecolor": "0.7"},
    )

    fig.tight_layout()
    fig.savefig(out.with_suffix(".png"), dpi=150)
    fig.savefig(out.with_suffix(".pdf"))
    print(f"plot        : {out.with_suffix('.png')}")
    return 0 if passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
