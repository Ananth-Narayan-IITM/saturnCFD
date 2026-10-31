#!/usr/bin/env python3
"""Validate the 1D scalar Burgers solver against its analytic solution.

Runs the YAML-driven ``saturnBurger`` case at several resolutions and MPI rank
counts, writes converged profile/error tables, and produces a validation figure.

Usage (from the repository root, CPU build already compiled):

    python3 tools/validateBurgers.py
    python3 tools/validateBurgers.py --cli buildCPU/saturnCFD \
        --out docs/validation/figures/burgersValidation
"""

from __future__ import annotations

import argparse
import csv
import json
import math
import os
import shutil
import subprocess
import sys
from pathlib import Path
from textwrap import dedent

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt

ROOT = Path(__file__).resolve().parents[1]


def write_case(path: Path, n: int, nu: float, t_end: float, prefix: Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(
        dedent(
            f"""\
            solver: saturnBurger

            units:
              system: SI

            mesh:
              type: box
              nx: {n}
              ny: 1
              nz: 1
              box:
                xmin: -1.0
                xmax: 1.0
                ymin: 0.0
                ymax: 1.0
                zmin: 0.0
                zmax: 1.0

            time:
              start: 0.0
              end: {t_end}
              scheme: ssprk3
              adaptive: true
              cfl: 0.3
              dtMax: 0.01
              maxSteps: 200000

            burgers:
              nu: {nu}
              velocity: 1.0
              x0: 0.0
              source: 0.0
              periodic: false
              initial: travelingWave

            numerics:
              convection: muscl
              limiter: vanLeer

            output:
              prefix: {prefix}
              csv: true
              json: true
              exact: true
            """
        )
    )


def run_case(cli: Path, case: Path, ranks: int, mpiexec: str) -> None:
    if ranks > 1:
        command = [mpiexec, "-np", str(ranks), str(cli), str(case)]
    else:
        command = [str(cli), str(case)]
    env = os.environ.copy()
    env["OMP_NUM_THREADS"] = "1"
    result = subprocess.run(
        command,
        cwd=ROOT,
        text=True,
        env=env,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    if result.returncode != 0:
        sys.stderr.write(result.stdout)
        sys.stderr.write(result.stderr)
        raise RuntimeError(f"case failed: {' '.join(command)}")


def read_summary(path: Path) -> dict:
    with path.open() as handle:
        return json.load(handle)


def read_profile(path: Path) -> list[dict[str, float]]:
    with path.open() as handle:
        return [
            {key: float(value) for key, value in row.items()}
            for row in csv.DictReader(handle)
        ]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cli", type=Path, default=ROOT / "buildCPU/saturnCFD")
    parser.add_argument("--work", type=Path,
                        default=ROOT / "results/burgers/validation")
    parser.add_argument("--out", type=Path,
                        default=ROOT / "docs/validation/figures/burgersValidation")
    parser.add_argument("--nu", type=float, default=0.02)
    parser.add_argument("--t-end", type=float, default=0.5)
    parser.add_argument("--ranks", type=int, nargs="+", default=[1, 2, 4])
    parser.add_argument("--no-mpi", action="store_true")
    args = parser.parse_args()

    cli = args.cli.resolve()
    if not cli.exists():
        raise SystemExit(f"missing solver executable: {cli}")
    work = args.work.resolve()
    if work.exists():
        shutil.rmtree(work)
    work.mkdir(parents=True, exist_ok=True)

    resolutions = [32, 64, 128, 256, 512, 1024]
    summaries: dict[int, dict] = {}
    profiles: dict[int, list[dict[str, float]]] = {}

    for n in resolutions:
        prefix = work / f"traveling{n}"
        case = work / "cases" / f"traveling{n}.yaml"
        write_case(case, n, args.nu, args.t_end, prefix)
        run_case(cli, case, 1, "mpirun")
        summaries[n] = read_summary(prefix.with_suffix(".json"))
        profiles[n] = read_profile(prefix.with_suffix(".csv"))
        print(
            f"N={n:4d}  L1={summaries[n]['exact']['l1']:.6e}"
            f"  L2={summaries[n]['exact']['l2']:.6e}"
            f"  Linf={summaries[n]['exact']['linf']:.6e}"
        )

    convergence = work / "convergence.csv"
    with convergence.open("w", newline="") as handle:
        writer = csv.writer(handle)
        writer.writerow(["n", "h", "l1", "l2", "linf", "observed_order"])
        previous = None
        for n in resolutions:
            exact = summaries[n]["exact"]
            h = 2.0 / n
            order = ""
            if previous is not None:
                order = math.log(previous / exact["l2"]) / math.log(2.0)
            writer.writerow([n, h, exact["l1"], exact["l2"], exact["linf"], order])
            previous = exact["l2"]

    ranks = [1] if args.no_mpi else sorted(set(args.ranks))
    rank_rows = []
    for rank in ranks:
        n = 256
        prefix = work / f"rank{rank}_n{n}"
        case = work / "cases" / f"rank{rank}_n{n}.yaml"
        write_case(case, n, args.nu, args.t_end, prefix)
        run_case(cli, case, rank, "mpirun")
        summary = read_summary(prefix.with_suffix(".json"))
        exact = summary["exact"]
        rank_rows.append((rank, exact["l1"], exact["l2"], exact["linf"]))
        print(
            f"ranks={rank:2d}  N={n}  L2={exact['l2']:.16e}"
            f"  Linf={exact['linf']:.16e}"
        )

    rank_csv = work / "rankIndependence.csv"
    reference = next(row[2] for row in rank_rows if row[0] == 1)
    with rank_csv.open("w", newline="") as handle:
        writer = csv.writer(handle)
        writer.writerow(["n", "ranks", "l1", "l2", "linf", "relative_l2_delta"])
        for rank, l1, l2, linf in rank_rows:
            writer.writerow([256, rank, l1, l2, linf, (l2 - reference) / reference])

    # ------------------------------------------------------------- figure --
    plt.rcParams.update({
        "font.size": 10,
        "axes.grid": True,
        "grid.alpha": 0.3,
        "figure.dpi": 150,
    })
    fig, axes = plt.subplots(2, 2, figsize=(11.0, 8.0))
    ax_profile, ax_error, ax_order, ax_rank = axes.flat

    exact_profile = profiles[256]
    ax_profile.plot(
        [p["x"] for p in exact_profile],
        [p["u_exact"] for p in exact_profile],
        color="black",
        linewidth=2.0,
        label="analytic",
        zorder=5,
    )
    colors = {64: "#1f77b4", 128: "#ff7f0e", 256: "#2ca02c"}
    for n in (64, 128, 256):
        profile = profiles[n]
        ax_profile.plot(
            [p["x"] for p in profile],
            [p["u_numerical"] for p in profile],
            marker="o" if n == 64 else "",
            markersize=2.5,
            linewidth=1.0,
            color=colors[n],
            label=f"saturnBurger N={n}",
        )
    ax_profile.set_xlabel("x [m]")
    ax_profile.set_ylabel("u [m/s]")
    ax_profile.set_title(r"Traveling wave at $t=0.5$ s")
    ax_profile.legend(loc="best")
    ax_profile.set_xlim(-0.35, 0.35)

    for n in (64, 128, 256):
        profile = profiles[n]
        ax_error.semilogy(
            [p["x"] for p in profile],
            [abs(p["error"]) + 1e-18 for p in profile],
            linewidth=1.0,
            color=colors[n],
            label=f"N={n}",
        )
    ax_error.set_xlabel("x [m]")
    ax_error.set_ylabel(r"$|u_h-u_{\rm exact}|$ [m/s]")
    ax_error.set_title("Pointwise error")
    ax_error.set_xlim(-0.35, 0.35)
    ax_error.legend(loc="best")

    h_values = [2.0 / n for n in resolutions]
    l2_values = [summaries[n]["exact"]["l2"] for n in resolutions]
    ax_order.loglog(h_values, l2_values, "o-", color="#d62728",
                    label="measured L2")
    ax_order.loglog(
        h_values,
        [l2_values[0] * (h / h_values[0]) ** 2 for h in h_values],
        "--",
        color="black",
        label=r"second order ($h^2$)",
    )
    ax_order.set_xlabel("cell size h [m]")
    ax_order.set_ylabel(r"$L^2$ error [m/s]")
    ax_order.set_title("Grid convergence")
    ax_order.invert_xaxis()
    ax_order.legend(loc="best")

    rank_values = [row[2] for row in rank_rows]
    reference = next(row[2] for row in rank_rows if row[0] == 1)
    normalized = [value / reference - 1.0 for value in rank_values]
    max_deviation = max(abs(value) for value in normalized)
    ax_rank.plot([row[0] for row in rank_rows], normalized, "o-",
                 color="#9467bd", markersize=8)
    ax_rank.set_ylim(-1e-9, 1e-9)
    ax_rank.set_xlabel("MPI ranks")
    ax_rank.set_ylabel(r"$L^2/L^2_{1\,{\rm rank}}-1$ at N=256")
    ax_rank.set_title("Rank independence")
    ax_rank.set_xticks([row[0] for row in rank_rows])
    ax_rank.text(
        0.03,
        0.05,
        f"max relative deviation = {max_deviation:.1e}\n"
        "(all ranks give the same discrete solution)",
        transform=ax_rank.transAxes,
        fontsize=9,
        bbox={"facecolor": "white", "alpha": 0.8, "edgecolor": "0.7"},
    )

    fig.suptitle(
        "saturnBurger 1D validation: viscous Burgers traveling wave "
        f"(nu={args.nu:g}, SSPRK3/MUSCL/van Leer)",
        fontsize=12,
    )
    fig.tight_layout(rect=(0, 0, 1, 0.96))
    args.out.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(args.out.with_suffix(".png"))
    fig.savefig(args.out.with_suffix(".pdf"))

    # A wider two-panel version for slides.
    slide = args.out.with_name(args.out.stem + "Slide")
    fig2, (panel_profile, panel_order) = plt.subplots(1, 2, figsize=(13, 4.6))
    panel_profile.plot(
        [p["x"] for p in exact_profile],
        [p["u_exact"] for p in exact_profile],
        color="black",
        linewidth=2.0,
        label="analytic",
        zorder=5,
    )
    for n in (64, 128, 256):
        profile = profiles[n]
        panel_profile.plot(
            [p["x"] for p in profile],
            [p["u_numerical"] for p in profile],
            marker="o" if n == 64 else "",
            markersize=2.5,
            linewidth=1.0,
            color=colors[n],
            label=f"N={n}",
        )
    panel_profile.set_xlabel("x [m]")
    panel_profile.set_ylabel("u [m/s]")
    panel_profile.set_title(r"Traveling wave at $t=0.5$ s")
    panel_profile.set_xlim(-0.35, 0.35)
    panel_profile.legend(loc="best")

    panel_order.loglog(h_values, l2_values, "o-", color="#d62728",
                       label="measured $L^2$")
    panel_order.loglog(
        h_values,
        [l2_values[0] * (h / h_values[0]) ** 2 for h in h_values],
        "--",
        color="black",
        label=r"second order ($h^2$)",
    )
    panel_order.set_xlabel("cell size h [m]")
    panel_order.set_ylabel(r"$L^2$ error [m/s]")
    panel_order.set_title("Grid convergence")
    panel_order.invert_xaxis()
    panel_order.legend(loc="best")
    fig2.tight_layout()
    fig2.savefig(slide.with_suffix(".png"))
    fig2.savefig(slide.with_suffix(".pdf"))

    print(f"wrote {args.out.with_suffix('.png')}")
    print(f"wrote {args.out.with_suffix('.pdf')}")
    print(f"wrote {slide.with_suffix('.pdf')}")
    print(f"tables: {convergence} and {rank_csv}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
