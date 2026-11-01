#!/usr/bin/env python3
"""Standalone analytic comparison for the 3D heat example.

Reads the case parameters from saturnHeat3D.yaml, recomputes

    T(x,y,z,t) = A sin(k.x - (a.k) t) exp(-alpha |k|^2 t)

and compares it with the CSV written by saturnHeat.

Usage:
    saturnCFD saturnHeat3D.yaml
    python3 compareAnalytic.py
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


def exact_solution(x, y, z, t, alpha, velocity, wave, amplitude):
    kx, ky, kz = wave
    omega = velocity[0] * kx + velocity[1] * ky + velocity[2] * kz
    decay = alpha * (kx * kx + ky * ky + kz * kz)
    return (
        amplitude
        * np.sin(kx * x + ky * y + kz * z - omega * t)
        * np.exp(-decay * t)
    )


def grid_at_plane(x, y, z, values, z_target):
    z_near = z[np.argmin(np.abs(z - z_target))]
    mask = np.isclose(z, z_near)
    xu = np.unique(x[mask])
    yu = np.unique(y[mask])
    grid = np.full((xu.size, yu.size), np.nan)
    ix = np.searchsorted(xu, x[mask])
    iy = np.searchsorted(yu, y[mask])
    grid[ix, iy] = values[mask]
    return xu, yu, grid, z_near


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--case", type=Path,
                        default=HERE / "saturnHeat3D.yaml")
    parser.add_argument("--cli", default="saturnCFD")
    parser.add_argument("--no-run", action="store_true")
    parser.add_argument("--tolerance", type=float, default=5.0e-3)
    parser.add_argument("--out", type=Path, default=None)
    args = parser.parse_args()

    case = args.case.resolve()
    if not case.exists():
        raise SystemExit(f"missing case file: {case}")

    values = read_simple_yaml(case)
    alpha = as_float(values, "heat", "alpha", 0.01)
    velocity = (
        as_float(values, "heat", "velocityX", 1.0),
        as_float(values, "heat", "velocityY", 0.5),
        as_float(values, "heat", "velocityZ", 0.25),
    )
    wave = (
        as_float(values, "heat", "kx", 2.0 * math.pi),
        as_float(values, "heat", "ky", 2.0 * math.pi),
        as_float(values, "heat", "kz", 2.0 * math.pi),
    )
    amplitude = as_float(values, "heat", "amplitude", 1.0)
    t_end = as_float(values, "time", "end", 0.1)
    prefix = as_string(values, "output", "prefix", "results/saturnHeat3D/fourier")
    output_base = case.parent / prefix
    csv_path = output_base.with_suffix(".csv")
    json_path = output_base.with_suffix(".json")

    if not args.no_run:
        command = [args.cli, case.name]
        print("+", " ".join(command))
        subprocess.run(command, cwd=case.parent, check=True)

    if not csv_path.exists():
        raise SystemExit(f"missing profile CSV: {csv_path}")

    data = np.genfromtxt(csv_path, delimiter=",", names=True)
    data = np.atleast_1d(data)
    # Cell centres that are mathematically equal can differ in the last bits
    # after the ISO-surface/centroid arithmetic; round to a safe tolerance so
    # unique() finds the real grid lines.
    x = np.round(np.asarray(data["x"], dtype=float), 10)
    y = np.round(np.asarray(data["y"], dtype=float), 10)
    z = np.round(np.asarray(data["z"], dtype=float), 10)
    numerical = np.asarray(data["u_numerical"], dtype=float)

    analytic = exact_solution(x, y, z, t_end, alpha, velocity, wave, amplitude)
    error = numerical - analytic
    l1 = float(np.mean(np.abs(error)))
    l2 = float(math.sqrt(np.mean(error * error)))
    linf = float(np.max(np.abs(error)))
    solver_exact = np.asarray(data["u_exact"], dtype=float)
    consistency = float(np.max(np.abs(solver_exact - analytic)))

    print()
    print(f"case        : {case}")
    print(f"samples     : {x.size}")
    print(f"t_end       : {t_end:g} s")
    print(f"alpha       : {alpha:g}")
    print(f"velocity    : ({velocity[0]:g}, {velocity[1]:g}, {velocity[2]:g})")
    print(f"wave vector : ({wave[0]:g}, {wave[1]:g}, {wave[2]:g})")
    print(f"L1 error    : {l1:.6e}")
    print(f"L2 error    : {l2:.6e}")
    print(f"Linf error  : {linf:.6e}")
    print(f"max |u_exact - independent analytic| : {consistency:.3e}")

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

    xu, yu, num_grid, z_plane = grid_at_plane(x, y, z, numerical, 0.5)
    _, _, exact_grid, _ = grid_at_plane(x, y, z, analytic, 0.5)
    _, _, error_grid, _ = grid_at_plane(x, y, z, error, 0.5)

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
            im = ax.pcolormesh(xu, yu, grid, shading="auto", cmap="RdBu_r",
                               vmin=-vmax, vmax=vmax)
        else:
            im = ax.pcolormesh(xu, yu, grid, shading="auto", cmap="viridis")
        ax.set_aspect("equal")
        ax.set_xlabel("x [m]")
        ax.set_ylabel("y [m]")
        ax.set_title(f"{title} (z={z_plane:g})")
        fig.colorbar(im, ax=ax, shrink=0.82)

    # Diagonal cut x = y = z.
    h = float(xu[1] - xu[0]) if xu.size > 1 else 1.0
    diag = (np.abs(x - y) < 0.51 * h) & (np.abs(y - z) < 0.51 * h)
    order = np.argsort(x[diag])
    ax_diag = axes[1, 1]
    ax_diag.plot(x[diag][order], analytic[diag][order], color="black",
                 linewidth=2.0, label="analytic")
    ax_diag.plot(x[diag][order], numerical[diag][order], "o",
                 color="#1f77b4", markersize=2.5, label="saturnHeat")
    ax_diag.set_xlabel("x = y = z [m]")
    ax_diag.set_ylabel("T [K]")
    ax_diag.set_title("Diagonal cut")
    ax_diag.legend(loc="best")
    ax_diag.grid(alpha=0.3)

    fig.suptitle(
        f"3D heat Fourier mode at t={t_end:g} s "
        f"(alpha={alpha:g}, N={xu.size}x{yu.size}x{np.unique(z).size})"
    )
    fig.tight_layout(rect=(0, 0, 1, 0.96))
    fig.savefig(out.with_suffix(".png"), dpi=150)
    fig.savefig(out.with_suffix(".pdf"))
    print(f"plot        : {out.with_suffix('.png')}")
    return 0 if passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
