#!/usr/bin/env python3
"""Independent analytic comparison for the saturnStokes examples.

Recomputes the plane Poiseuille or Couette profile from the case YAML and
compares it with the CSV written by saturnStokes. Works for both the 2D and
3D example directories.

Usage, from an example directory:

    saturnCFD poiseuille.yaml
    python3 compareAnalytic.py --case poiseuille

or let the script run the case first:

    python3 compareAnalytic.py --case couette --cli /path/to/saturnCFD
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


def to_grid(x, y, values):
    xu = np.unique(x)
    yu = np.unique(y)
    grid = np.full((xu.size, yu.size), np.nan)
    grid[np.searchsorted(xu, x), np.searchsorted(yu, y)] = values
    return xu, yu, grid


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--case", choices=["poiseuille", "couette"],
                        default="poiseuille")
    parser.add_argument("--cli", default="saturnCFD")
    parser.add_argument("--no-run", action="store_true")
    parser.add_argument("--tolerance", type=float, default=2.0e-3)
    parser.add_argument("--out", type=Path, default=None)
    args = parser.parse_args()

    case_file = HERE / f"{args.case}.yaml"
    if not case_file.exists():
        raise SystemExit(f"missing case file: {case_file}")
    values = read_simple_yaml(case_file)

    if not args.no_run:
        command = [args.cli, case_file.name]
        print("+", " ".join(command))
        subprocess.run(command, cwd=HERE, check=True)

    prefix = values.get("output", {}).get(
        "prefix", f"results/saturnStokes/{args.case}")
    csv_path = HERE / prefix
    if csv_path.suffix != ".csv":
        csv_path = csv_path.with_suffix(".csv")
    json_path = csv_path.with_suffix(".json")
    if not csv_path.exists():
        raise SystemExit(f"missing CSV: {csv_path}")

    data = np.genfromtxt(csv_path, delimiter=",", names=True)
    data = np.atleast_1d(data)
    x = np.asarray(data["x"], dtype=float)
    y = np.asarray(data["y"], dtype=float)
    z = np.asarray(data["z"], dtype=float)
    u = np.asarray(data["u"], dtype=float)

    mu = as_float(values, "stokes", "mu", 1.0)
    y0 = as_float(values, "box", "ymin", 0.0)
    ymax = as_float(values, "box", "ymax", 1.0)
    H = ymax - y0
    if args.case == "poiseuille":
        G = as_float(values, "stokes", "forceX", 1.0)
        analytic = G / (2.0 * mu) * (y - y0) * (H - (y - y0))
    else:
        U = as_float(values, "stokes", "wallVelocityX", 1.0)
        analytic = U * (y - y0) / H

    error = u - analytic
    l1 = float(np.mean(np.abs(error)))
    l2 = float(math.sqrt(np.mean(error * error)))
    linf = float(np.max(np.abs(error)))
    solver_exact = np.asarray(data["u_exact"], dtype=float)
    consistency = float(np.max(np.abs(solver_exact - analytic)))

    print()
    print(f"case        : {case_file}")
    print(f"samples     : {x.size}")
    print(f"L1 error    : {l1:.6e}")
    print(f"L2 error    : {l2:.6e}")
    print(f"Linf error  : {linf:.6e}")
    print(f"max |u_exact - independent analytic| : {consistency:.3e}")
    if json_path.exists():
        summary = json.loads(json_path.read_text())
        if "exact" in summary:
            print(
                "solver JSON : "
                f"L2={summary['exact']['l2']:.6e} "
                f"pL2={summary['exact']['pL2']:.6e}"
            )

    passed = l2 <= args.tolerance
    print(
        ("PASS" if passed else "FAIL")
        + f": L2 {l2:.6e} {'<=' if passed else '>'} tolerance "
        f"{args.tolerance:.1e}"
    )

    # The exact profile depends only on y. Average over x (and z) for a clean
    # 1D comparison; use a z-midplane for the 2D field panels.
    z_plane = z[np.argmin(np.abs(z - 0.5))]
    plane = np.isclose(z, z_plane)
    xu, yu, num_grid = to_grid(x[plane], y[plane], u[plane])
    _, _, exact_grid = to_grid(x[plane], y[plane], analytic[plane])
    _, _, error_grid = to_grid(x[plane], y[plane], error[plane])

    yu_all = np.unique(y)
    num_profile = np.array(
        [np.mean(u[np.isclose(y, yy)]) for yy in yu_all]
    )
    exact_profile = np.array(
        [np.mean(analytic[np.isclose(y, yy)]) for yy in yu_all]
    )

    out = args.out if args.out is not None else csv_path.parent / "comparison"
    out.parent.mkdir(parents=True, exist_ok=True)
    fig, axes = plt.subplots(2, 2, figsize=(11.0, 8.0))
    panels = [
        (axes[0, 0], num_grid.T, "saturnStokes"),
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

    ax = axes[1, 1]
    ax.plot(exact_profile, yu_all, color="black", linewidth=2.0,
            label="analytic")
    ax.plot(num_profile, yu_all, "o", color="#1f77b4", markersize=2.5,
            label="saturnStokes")
    ax.set_xlabel("u [m/s]")
    ax.set_ylabel("y [m]")
    ax.set_title("Profile (averaged over periodic directions)")
    ax.legend(loc="best")
    ax.grid(alpha=0.3)

    dim = int(values.get("mesh", {}).get("nz", "1")) if "mesh" in values else 1
    title = (
        f"saturnStokes {args.case} at "
        f"{values.get('mesh', {}).get('nx', '?')}x"
        f"{values.get('mesh', {}).get('ny', '?')}x"
        f"{values.get('mesh', {}).get('nz', '?')}"
    )
    fig.suptitle(title)
    fig.tight_layout(rect=(0, 0, 1, 0.96))
    fig.savefig(out.with_suffix(".png"), dpi=150)
    fig.savefig(out.with_suffix(".pdf"))
    print(f"plot        : {out.with_suffix('.png')}")
    return 0 if passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
