#!/usr/bin/env python3
"""Formal analytic validation for the saturnStokes 2D/3D examples.

Runs (or reuses) both Poiseuille and Couette cases, recomputes the exact profile
from the YAML parameters, prints a combined error table, and draws either a 2D
field comparison or a 3D slice-contour comparison.

Usage, from an example directory:

    saturnCFD poiseuille.yaml
    saturnCFD couette.yaml
    python3 compareAnalytic.py

or let the script run both cases:

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
from matplotlib.cm import ScalarMappable
from matplotlib.colors import Normalize

HERE = Path(__file__).resolve().parent
CASES = ("poiseuille", "couette")


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


def midpoint(values, target=0.5):
    unique = np.unique(values)
    return unique[np.argmin(np.abs(unique - target))]


def exact_profile(y, values, case):
    mu = as_float(values, "stokes", "mu", 1.0)
    y0 = as_float(values, "box", "ymin", 0.0)
    ymax = as_float(values, "box", "ymax", 1.0)
    height = ymax - y0
    if case == "poiseuille":
        G = as_float(values, "stokes", "forceX", 1.0)
        return G / (2.0 * mu) * (y - y0) * (height - (y - y0))
    U = as_float(values, "stokes", "wallVelocityX", 1.0)
    return U * (y - y0) / height


def load_case(case, cli, no_run):
    case_file = HERE / f"{case}.yaml"
    if not case_file.exists():
        raise SystemExit(f"missing case file: {case_file}")
    values = read_simple_yaml(case_file)
    if not no_run:
        command = [cli, case_file.name]
        print("+", " ".join(command))
        subprocess.run(command, cwd=HERE, check=True)

    prefix = values.get("output", {}).get(
        "prefix", f"results/saturnStokes/{case}")
    csv_path = HERE / prefix
    if csv_path.suffix != ".csv":
        csv_path = csv_path.with_suffix(".csv")
    if not csv_path.exists():
        raise SystemExit(f"missing CSV: {csv_path}")

    data = np.genfromtxt(csv_path, delimiter=",", names=True)
    data = np.atleast_1d(data)
    x = np.asarray(data["x"], dtype=float)
    y = np.asarray(data["y"], dtype=float)
    z = np.asarray(data["z"], dtype=float)
    numerical = np.asarray(data["u"], dtype=float)
    pressure = np.asarray(data["p"], dtype=float)
    p_exact = np.asarray(data["p_exact"], dtype=float)
    analytic = exact_profile(y, values, case)
    error = numerical - analytic
    dp = pressure - p_exact
    dp -= np.mean(dp)
    metrics = {
        "case": case,
        "yaml": case_file,
        "csv": csv_path,
        "values": values,
        "x": x,
        "y": y,
        "z": z,
        "numerical": numerical,
        "analytic": analytic,
        "error": error,
        "l1": float(np.mean(np.abs(error))),
        "l2": float(np.sqrt(np.mean(error * error))),
        "linf": float(np.max(np.abs(error))),
        "pL2": float(np.sqrt(np.mean(dp * dp))),
        "solverConsistency": float(
            np.max(np.abs(np.asarray(data["u_exact"], dtype=float) - analytic))
        ),
        "samples": int(x.size),
        "is3d": np.unique(z).size > 1,
    }
    return metrics


def plot_2d(m, out):
    x, y = m["x"], m["y"]
    z_plane = midpoint(m["z"])
    plane = np.isclose(m["z"], z_plane)
    xu, yu, num_grid = to_grid(x[plane], y[plane], m["numerical"][plane])
    _, _, exact_grid = to_grid(x[plane], y[plane], m["analytic"][plane])
    _, _, error_grid = to_grid(x[plane], y[plane], m["error"][plane])

    yu_all = np.unique(y)
    num_profile = np.array(
        [np.mean(m["numerical"][np.isclose(y, yy)]) for yy in yu_all]
    )
    exact_profile_y = np.array(
        [np.mean(m["analytic"][np.isclose(y, yy)]) for yy in yu_all]
    )

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
    ax.plot(exact_profile_y, yu_all, color="black", linewidth=2.0,
            label="analytic")
    ax.plot(num_profile, yu_all, "o", color="#1f77b4", markersize=2.5,
            label="saturnStokes")
    ax.set_xlabel("u [m/s]")
    ax.set_ylabel("y [m]")
    ax.set_title("Profile (averaged over periodic directions)")
    ax.legend(loc="best")
    ax.grid(alpha=0.3)

    values = m["values"]
    fig.suptitle(
        f"saturnStokes {m['case']} at "
        f"{values.get('mesh', {}).get('nx', '?')}x"
        f"{values.get('mesh', {}).get('ny', '?')}x"
        f"{values.get('mesh', {}).get('nz', '?')}  "
        f"L2={m['l2']:.3e}"
    )
    fig.tight_layout(rect=(0, 0, 1, 0.95))
    fig.savefig(out.with_suffix(".png"), dpi=150)
    fig.savefig(out.with_suffix(".pdf"))
    plt.close(fig)


def slice_2d(m, axis, target):
    plane = np.isclose(m[axis], target)
    if axis == "z":
        return to_grid(m["x"][plane], m["y"][plane], m["numerical"][plane]), \
               to_grid(m["x"][plane], m["y"][plane], m["analytic"][plane]), \
               to_grid(m["x"][plane], m["y"][plane], m["error"][plane])
    if axis == "x":
        return to_grid(m["y"][plane], m["z"][plane], m["numerical"][plane]), \
               to_grid(m["y"][plane], m["z"][plane], m["analytic"][plane]), \
               to_grid(m["y"][plane], m["z"][plane], m["error"][plane])
    return to_grid(m["x"][plane], m["z"][plane], m["numerical"][plane]), \
           to_grid(m["x"][plane], m["z"][plane], m["analytic"][plane]), \
           to_grid(m["x"][plane], m["z"][plane], m["error"][plane])


def draw_3d_slices(ax, x, y, z, values, x1, x2, norm, levels, title):
    del levels
    cmap = plt.get_cmap("viridis")
    for x_slice in (x1, x2):
        plane = np.isclose(x, x_slice)
        yu, zu, grid_yz = to_grid(y[plane], z[plane], values[plane])
        Y, Z = np.meshgrid(yu, zu, indexing="ij")
        X = np.full_like(Y, x_slice)
        ax.plot_surface(X, Y, Z, facecolors=cmap(norm(grid_yz)),
                        rstride=1, cstride=1, shade=False,
                        linewidth=0, antialiased=False)
    ax.set_xlabel("x [m]")
    ax.set_ylabel("y [m]")
    ax.set_zlabel("z [m]")
    ax.set_title(title)
    ax.view_init(elev=28, azim=-58)
    ax.grid(False)
    ax.set_box_aspect((1, 1, 1))
    ax.set_xlim(float(np.min(x)), float(np.max(x)))
    ax.set_ylim(float(np.min(y)), float(np.max(y)))
    ax.set_zlim(float(np.min(z)), float(np.max(z)))

def plot_3d(m, out):
    x, y, z = m["x"], m["y"], m["z"]
    xmid, ymid, zmid = midpoint(x), midpoint(y), midpoint(z)
    x_slice1, x_slice2 = midpoint(x, 0.25), midpoint(x, 0.75)
    vmin = float(min(np.min(m["numerical"]), np.min(m["analytic"])))
    vmax = float(max(np.max(m["numerical"]), np.max(m["analytic"])))
    norm = Normalize(vmin=vmin, vmax=vmax)
    levels = np.linspace(vmin, vmax, 24)

    fig = plt.figure(figsize=(12.5, 10.0))
    ax_num = fig.add_subplot(2, 2, 1, projection="3d")
    ax_exact = fig.add_subplot(2, 2, 2, projection="3d")
    draw_3d_slices(ax_num, x, y, z, m["numerical"], x_slice1, x_slice2,
                   norm, levels, "saturnStokes (slices)")
    draw_3d_slices(ax_exact, x, y, z, m["analytic"], x_slice1, x_slice2,
                   norm, levels, "analytic (slices)")

    sm = ScalarMappable(norm=norm, cmap="viridis")
    sm.set_array([])
    cbar = fig.colorbar(sm, ax=[ax_num, ax_exact], orientation="horizontal",
                        fraction=0.035, pad=0.03, shrink=0.55)
    cbar.set_label("u [m/s]")

    yu_all = np.unique(y)
    num_profile = np.array(
        [np.mean(m["numerical"][np.isclose(y, yy)]) for yy in yu_all]
    )
    exact_profile_y = np.array(
        [np.mean(m["analytic"][np.isclose(y, yy)]) for yy in yu_all]
    )
    ax_profile = fig.add_subplot(2, 2, 3)
    ax_profile.plot(exact_profile_y, yu_all, color="black", linewidth=2.0,
                    label="analytic")
    ax_profile.plot(num_profile, yu_all, "o", color="#1f77b4", markersize=2.5,
                    label="saturnStokes")
    ax_profile.set_xlabel("u [m/s]")
    ax_profile.set_ylabel("y [m]")
    ax_profile.set_title("Profile (averaged over x,z)")
    ax_profile.legend(loc="best")
    ax_profile.grid(alpha=0.3)

    ax_error = fig.add_subplot(2, 2, 4)
    ax_error.axis("off")
    ax_error.set_title("Validation summary")
    ax_error.text(
        0.03, 0.92,
        f"case    : {m['case']}\n"
        f"samples : {m['samples']}\n"
        f"L1      : {m['l1']:.4e}\n"
        f"L2      : {m['l2']:.4e}\n"
        f"Linf    : {m['linf']:.4e}\n"
        f"pL2     : {m['pL2']:.4e}\n"
        f"solver  : {m['solverConsistency']:.2e}",
        transform=ax_error.transAxes, va="top", family="monospace",
        fontsize=11,
    )

    values = m["values"]
    fig.suptitle(
        f"saturnStokes {m['case']} 3D at "
        f"{values.get('mesh', {}).get('nx', '?')}x"
        f"{values.get('mesh', {}).get('ny', '?')}x"
        f"{values.get('mesh', {}).get('nz', '?')}  "
        f"L2={m['l2']:.3e}"
    )
    fig.subplots_adjust(left=0.05, right=0.95, top=0.91, bottom=0.07,
                        wspace=0.18, hspace=0.35)
    fig.savefig(out.with_suffix(".png"), dpi=150)
    fig.savefig(out.with_suffix(".pdf"))
    plt.close(fig)

def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--case", choices=["both", *CASES], default="both")
    parser.add_argument("--cli", default="saturnCFD")
    parser.add_argument("--no-run", action="store_true")
    parser.add_argument("--tolerance", type=float, default=2.0e-3)
    parser.add_argument("--out", type=Path, default=None)
    args = parser.parse_args()

    selected = CASES if args.case == "both" else (args.case,)
    results = []
    for case in selected:
        result = load_case(case, args.cli, args.no_run)
        results.append(result)
        out = (args.out.with_name(args.out.name + f"_{case}")
               if args.out is not None
               else result["csv"].parent / f"comparison_{case}")
        if result["is3d"]:
            plot_3d(result, out)
        else:
            plot_2d(result, out)

    print()
    print(f"{'case':12s} {'samples':>8s} {'L1':>11s} {'L2':>11s} "
          f"{'Linf':>11s} {'pL2':>11s} {'status':>7s}")
    all_pass = True
    for r in results:
        passed = r["l2"] <= args.tolerance and r["pL2"] <= 1e-8
        all_pass = all_pass and passed
        print(f"{r['case']:12s} {r['samples']:8d} {r['l1']:11.4e} "
              f"{r['l2']:11.4e} {r['linf']:11.4e} {r['pL2']:11.4e} "
              f"{'PASS' if passed else 'FAIL':>7s}")
        print(f"  solver |u_exact - analytic|max = "
              f"{r['solverConsistency']:.3e}")
        print(f"  plot: {r['csv'].parent / ('comparison_' + r['case'])}.png")

    summary_path = results[0]["csv"].parent.parent / "summary.txt"
    summary_path.parent.mkdir(parents=True, exist_ok=True)
    lines = [
        f"{'case':12s} {'L2':>12s} {'Linf':>12s} {'pL2':>12s} status",
    ]
    for r in results:
        passed = r["l2"] <= args.tolerance and r["pL2"] <= 1e-8
        lines.append(
            f"{r['case']:12s} {r['l2']:12.5e} {r['linf']:12.5e} "
            f"{r['pL2']:12.5e} {'PASS' if passed else 'FAIL'}"
        )
    summary_path.write_text("\n".join(lines) + "\n")
    print(f"\nsummary     : {summary_path}")
    print(
        ("PASS" if all_pass else "FAIL")
        + f": both cases within L2 tolerance {args.tolerance:.1e} and "
          "pressure tolerance 1e-8"
    )
    return 0 if all_pass else 1


if __name__ == "__main__":
    raise SystemExit(main())
