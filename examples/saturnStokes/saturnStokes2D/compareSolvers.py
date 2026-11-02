#!/usr/bin/env python3
"""Compare the direct LU and fieldsplit Stokes solvers.

Runs the same case and mesh twice, once with ``linearSolver: direct`` and
once with ``linearSolver: fieldsplit``, then reports wall time, peak
process-tree resident memory, the analytic error norms and the difference
between the two numerical solutions.

The serial fieldsplit configuration uses GMRES + ILU on velocity and
preonly + Jacobi on pressure. For ``--ranks > 1`` it uses the production
GAMG/GAMG configuration, because the local ILU factorisations are not a good
parallel preconditioner here. Memory is the summed RSS of the launcher and its
MPI children, sampled from /proc.

Usage, from the example directory:

    python3 compareSolvers.py --cli ../../../buildCPU/saturnCFD
    python3 compareSolvers.py --cli ../../../buildCPU/saturnCFD --n 256
    python3 compareSolvers.py --cli ../../../buildCPU/saturnCFD --ranks 4
"""

from __future__ import annotations

import argparse
import json
import math
import os
import subprocess
import time
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np

HERE = Path(__file__).resolve().parent
DIM = 2


def yaml_for(case: str, n: int, solver: str, prefix: Path, ranks: int) -> str:
    mesh = f"""mesh:
  type: box
  nx: {n}
  ny: {n}
  nz: 1
  box:
    xmin: 0.0
    xmax: 1.0
    ymin: 0.0
    ymax: 1.0
    zmin: 0.0
    zmax: 1.0
"""
    if case == "poiseuille":
        stokes = "  case: poiseuille\n  mu: 1.0\n  forceX: 1.0\n"
    else:
        stokes = "  case: couette\n  mu: 1.0\n  wallVelocityX: 1.0\n"
    stokes += f"  linearSolver: {solver}\n"
    if solver == "fieldsplit":
        if ranks == 1:
            stokes += (
                "  velocityKsp: gmres\n"
                "  velocityPc: ilu\n"
                "  pressureKsp: preonly\n"
                "  pressurePc: jacobi\n"
            )
        else:
            stokes += (
                "  velocityKsp: gmres\n"
                "  velocityPc: gamg\n"
                "  pressureKsp: preonly\n"
                "  pressurePc: gamg\n"
            )
    return f"""solver: saturnStokes

units:
  system: SI

{mesh}
stokes:
{stokes}
output:
  prefix: {prefix}
  csv: true
  json: true
  exact: true
  vtkEvery: 0
"""


def tree_rss(pid: int) -> int:
    """Sum VmRSS over the process and all descendants, in bytes."""
    total = 0
    stack = [pid]
    seen: set[int] = set()
    while stack:
        p = stack.pop()
        if p in seen:
            continue
        seen.add(p)
        try:
            with open(f"/proc/{p}/status", encoding="utf-8") as fh:
                for line in fh:
                    if line.startswith("VmRSS:"):
                        total += int(line.split()[1]) * 1024
                        break
        except (FileNotFoundError, ProcessLookupError):
            continue
        try:
            with open(f"/proc/{p}/task/{p}/children", encoding="utf-8") as fh:
                stack.extend(int(x) for x in fh.read().split())
        except (FileNotFoundError, ProcessLookupError):
            pass
    return total


def run(cmd: list[str], log: Path) -> tuple[int, float, int]:
    t0 = time.perf_counter()
    with open(log, "w", encoding="utf-8") as out:
        proc = subprocess.Popen(
            cmd, cwd=HERE, stdout=out, stderr=subprocess.STDOUT
        )
        peak = tree_rss(proc.pid)
        while proc.poll() is None:
            peak = max(peak, tree_rss(proc.pid))
            time.sleep(0.05)
        proc.wait()
    return proc.returncode, time.perf_counter() - t0, peak


def load(prefix: Path):
    with open(prefix.with_suffix(".json"), encoding="utf-8") as fh:
        meta = json.load(fh)
    data = np.genfromtxt(
        prefix.with_suffix(".csv"), delimiter=",", names=True
    )
    return meta, np.atleast_1d(data)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--case", choices=["poiseuille", "couette"],
                        default="poiseuille")
    parser.add_argument("--n", type=int, default=128,
                        help="cells per direction (default 128)")
    parser.add_argument("--ranks", type=int, default=1)
    parser.add_argument("--cli", default="saturnCFD")
    parser.add_argument("--mpiexec", default="mpiexec")
    parser.add_argument("--timeout", type=int, default=1800)
    args = parser.parse_args()
    if args.n < 8:
        raise SystemExit("--n must be at least 8")

    out_dir = HERE / "results" / "solvers"
    out_dir.mkdir(parents=True, exist_ok=True)
    tag = f"{args.case}_{args.n}x{args.n}_np{args.ranks}"

    results = {}
    for solver in ("direct", "fieldsplit"):
        prefix = out_dir / f"{tag}_{solver}"
        cfg = out_dir / f"{tag}_{solver}.yaml"
        cfg.write_text(
            yaml_for(args.case, args.n, solver, prefix, args.ranks),
            encoding="utf-8",
        )
        cmd = [args.cli, str(cfg)]
        if args.ranks > 1:
            cmd = [args.mpiexec, "-n", str(args.ranks), *cmd]
        log = out_dir / f"{tag}_{solver}.log"
        print("+", " ".join(cmd), flush=True)
        code, wall, peak = run(cmd, log)
        if code != 0:
            print(f"  failed with exit code {code}; see {log}")
            return code
        meta, data = load(prefix)
        results[solver] = {
            "wall": wall,
            "peak": peak,
            "its": meta.get("iterations", 0),
            "residual": meta.get("residual", 0.0),
            "l2": meta["exact"]["l2"],
            "pL2": meta["exact"]["pL2"],
            "linf": meta["exact"]["linf"],
            "data": data,
        }

    d = results["direct"]
    f = results["fieldsplit"]
    if d["data"].size != f["data"].size:
        raise SystemExit("direct and fieldsplit CSV sizes differ")
    du = f["data"]["u"] - d["data"]["u"]
    dv = f["data"]["v"] - d["data"]["v"]
    dw = f["data"]["w"] - d["data"]["w"]
    dp = f["data"]["p"] - d["data"]["p"]
    dp -= np.mean(dp)
    vel_rms = float(np.sqrt(np.mean(du * du + dv * dv + dw * dw)))
    vel_max = float(np.max(np.sqrt(du * du + dv * dv + dw * dw)))
    p_rms = float(np.sqrt(np.mean(dp * dp)))

    lines = [
        f"case        : {args.case}",
        f"mesh        : {args.n} x {args.n} (2D), ranks {args.ranks}",
        f"fieldsplit  : {'ILU/Jacobi (serial-tuned)' if args.ranks == 1 else 'GAMG/GAMG (production)'}",
        "",
        f"{'solver':12s} {'its':>5s} {'wall [s]':>10s} {'peak [MB]':>10s} "
        f"{'L2':>11s} {'pL2':>11s}",
    ]
    for solver in ("direct", "fieldsplit"):
        r = results[solver]
        lines.append(
            f"{solver:12s} {r['its']:5d} {r['wall']:10.3f} "
            f"{r['peak'] / 2**20:10.1f} {r['l2']:11.4e} {r['pL2']:11.4e}"
        )
    lines += [
        "",
        f"fieldsplit - direct: velocity rms {vel_rms:.3e}, max {vel_max:.3e}",
        f"                     pressure  rms {p_rms:.3e} (mean removed)",
        f"speedup (direct/fieldsplit): {d['wall'] / f['wall']:.2f}",
        f"memory ratio (direct/fieldsplit): {d['peak'] / f['peak']:.2f}",
    ]
    text = "\n".join(lines)
    print("\n" + text)
    (out_dir / f"{tag}_summary.txt").write_text(text + "\n", encoding="utf-8")

    fig, axes = plt.subplots(1, 3, figsize=(11.5, 4.2))
    colors = ["#4c72b0", "#55a868"]

    axes[0].bar(["direct", "fieldsplit"], [d["wall"], f["wall"]],
                color=colors)
    axes[0].set_ylabel("wall time [s]")
    axes[0].set_title("Time")
    for i, v in enumerate([d["wall"], f["wall"]]):
        axes[0].text(i, v, f"{v:.2f}", ha="center", va="bottom", fontsize=9)

    dm = d["peak"] / 2**20
    fm = f["peak"] / 2**20
    axes[1].bar(["direct", "fieldsplit"], [dm, fm], color=colors)
    axes[1].set_ylabel("peak RSS [MB]")
    axes[1].set_title("Memory (process tree)")
    for i, v in enumerate([dm, fm]):
        axes[1].text(i, v, f"{v:.0f}", ha="center", va="bottom", fontsize=9)

    axes[2].bar(["velocity", "pressure"], [vel_rms, p_rms],
                color=["#c44e52", "#8172b3"])
    axes[2].set_yscale("log")
    axes[2].set_ylabel("fieldsplit - direct (rms)")
    axes[2].set_title("Solution difference")
    for i, v in enumerate([vel_rms, p_rms]):
        axes[2].text(i, max(v, 1e-30), f"{v:.2e}", ha="center", va="bottom",
                     fontsize=8)

    for ax in axes:
        ax.grid(alpha=0.3, axis="y")
    fig.suptitle(
        f"saturnStokes {args.case} {args.n}$^2$, {args.ranks} rank(s)"
    )
    fig.tight_layout(rect=(0, 0, 1, 0.93))
    fig.savefig(out_dir / f"{tag}_comparison.png", dpi=150)
    fig.savefig(out_dir / f"{tag}_comparison.pdf")
    plt.close(fig)
    print(f"\nsummary : {out_dir / (tag + '_summary.txt')}")
    print(f"figure  : {out_dir / (tag + '_comparison.png')}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
