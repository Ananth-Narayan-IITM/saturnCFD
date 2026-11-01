#!/usr/bin/env python3
"""Standalone analytic comparison for the 2D heat example.

This script is independent of the solver's `u_exact` column: it re-reads the
case parameters from `saturnHeat2D.yaml`, recomputes

    T(x,y,t) = A sin(kx x + ky y - (a.k) t) exp(-alpha (kx^2 + ky^2) t)

and compares it with the numerical profile written by `saturnHeat`.

Usage, from this directory:

    saturnCFD saturnHeat2D.yaml
    python3 compareAnalytic.py

or let the script run the case first:

    python3 compareAnalytic.py --cli /path/to/saturnCFD
"""

from __future__ import annotations

import argparse
import json
import math
import subprocess
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np

HERE = Path(__file__).resolve().parent


def read_simple_yaml(path: Path) -> dict:
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


def exact_solution(x, y, t, alpha, velocity, wave, amplitude):
    kx, ky = wave[0], wave[1]
    omega = velocity[0] * kx + velocity[1] * ky
    decay = alpha * (kx * kx + ky * ky)
    return amplitude * np.sin(kx * x + ky * y - omega * t) * np.exp(-decay * t)


def to_grid(x, y, values):
    xu = np.unique(x)
    yu = np.unique(y)
    grid = np.full((xu.size, yu.size), np.nan)
    ix = np.searchsorted(xu, x)
    iy = np.searchsorted(yu, y)
    grid[ix, iy] = values
    return xu, yu, grid


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--case", type=Path,
                        default=HERE / "saturnHeat2D.yaml")
    parser.add_argument("--cli", default="saturnCFD",
                        help="saturnCFD executable (default: from PATH)")
    parser.add_argument("--no-run", action="store_true",
                        help="use the existing CSV instead of running the solver")
    parser.add_argument("--tolerance", type=float, default=2.0e-3,
                        help="L2 tolerance for PASS/FAIL (default: 2e-3)")
    parser.add_argument("--out", type=Path, default=None,
                        help="plot path without extension")
    args = parser.parse_args()

    case = args.case.resolve()
    if not case.exists():
        raise SystemExit(f"missing case file: {case}")

    values = read_simple_yaml(case)
    alpha = as_float(values, "heat", "alpha", 0.01)
    velocity = (
        as_float(values, "heat", "velocityX", 1.0),
        as_float(values, "heat", "velocityY", 0.5),
    )
    wave = (
        as_float(values, "heat", "kx", 2.0 * math.pi),
        as_float(values, "heat", "ky", 2.0 * math.pi),
    )
    amplitude = as_float(values, "heat", "amplitude", 1.0)
    t_end = as_float(values, "time", "end", 0.2)
    prefix = as_string(values, "output", "prefix", "results/saturnHeat2D/fourier")
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
    # Cell centres that are mathematically equal can differ in the last bits
    # after the centroid arithmetic; round so unique() finds the grid lines.
    x = np.round(np.asarray(data["x"], dtype=float), 10)
    y = np.round(np.asarray(data["y"], dtype=float), 10)
    numerical = np.asarray(data["u_numerical"], dtype=float)

    analytic = exact_solution(x, y, t_end, alpha, velocity, wave, amplitude)
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
    print(f"alpha, a, k : {alpha:g}, ({velocity[0]:g}, {velocity[1]:g}), "
          f"({wave[0]:g}, {wave[1]:g})")
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

    xu, yu, num_grid = to_grid(x, y, numerical)
    _, _, exact_grid = to_grid(x, y, analytic)
    _, _, error_grid = to_grid(x, y, error)

    out = args.out if args.out is not None else output_base.parent / "comparison"
    out.parent.mkdir(parents=True, exist_ok=True)
    fig, axes = plt.subplots(2, 2, figsize=(11.0, 8.0))
    panels = [
        (axes[0, 0], num_grid.T, "saturnHeat"),
        (axes[0, 1], exact_grid.T, "analytic"),
        (axes[1, 0], error_grid.T, "error"),
    ]
    for ax, grid, title in panels:
        if title == "error":
            vmax = float(np.nanmax(np.abs(grid)))
            im = ax.pcolormesh(
                xu, yu, grid, shading="auto", cmap="RdBu_r",
                vmin=-vmax, vmax=vmax,
            )
        else:
            im = ax.pcolormesh(xu, yu, grid, shading="auto", cmap="viridis")
        ax.set_aspect("equal")
        ax.set_xlabel("x [m]")
        ax.set_ylabel("y [m]")
        ax.set_title(title)
        fig.colorbar(im, ax=ax, shrink=0.82)

    # A one-dimensional cut at y = 0.5 makes the wave easy to read.
    dy = float(yu[1] - yu[0]) if yu.size > 1 else 1.0
    cut = np.abs(y - 0.5) <= 0.51 * dy
    order = np.argsort(x[cut])
    ax_cut = axes[1, 1]
    ax_cut.plot(x[cut][order], analytic[cut][order], color="black",
                linewidth=2.0, label="analytic")
    ax_cut.plot(x[cut][order], numerical[cut][order], "o", color="#1f77b4",
                markersize=2.5, label="saturnHeat")
    ax_cut.set_xlabel("x [m] at y=0.5")
    ax_cut.set_ylabel("T [K]")
    ax_cut.set_title("Cut at y = 0.5")
    ax_cut.legend(loc="best")
    ax_cut.grid(alpha=0.3)

    fig.suptitle(
        f"2D heat Fourier mode at t={t_end:g} s "
        f"(alpha={alpha:g}, N={xu.size}x{yu.size})"
    )
    fig.tight_layout(rect=(0, 0, 1, 0.96))
    fig.savefig(out.with_suffix(".png"), dpi=150)
    fig.savefig(out.with_suffix(".pdf"))
    print(f"plot        : {out.with_suffix('.png')}")
    return 0 if passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
