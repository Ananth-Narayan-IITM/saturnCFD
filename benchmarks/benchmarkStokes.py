#!/usr/bin/env python3
"""Benchmark the steady ``saturnStokes`` direct solve on the CPU.

The CUDA build does not include PETSc yet (``cmake --preset gpu`` keeps
``SATURN_ENABLE_PETSC=OFF``), so ``saturnStokes`` is only available in the CPU
build.  This script therefore benchmarks the CPU direct solver and validates
its solution against the analytic Poiseuille or Couette profile.

The direct LU path is used on purpose: it is the reference solver and does not
need the iterative fieldsplit configuration.  When a CUDA-enabled PETSc build
becomes available, the same case can be run with ``--cpu <gpu-binary>``; the
script itself is backend-agnostic.

Example (from the repository root):

    python3 benchmarks/benchmarkStokes.py --cpu saturnCPU --dim 3d
"""

from __future__ import annotations

import argparse
import math
import sys
from pathlib import Path
from textwrap import dedent

from benchmarkCommon import (
    BenchmarkError,
    cpu_environment,
    error_metrics,
    format_metrics,
    independent_consistency,
    load_csv,
    load_json,
    resolve_cli,
    run_case,
    write_json,
    write_text,
)

ROOT = Path(__file__).resolve().parents[1]


def write_case(path: Path, case: str, nx: int, ny: int, nz: int,
               mu: float, force_x: float, wall_velocity_x: float,
               prefix: Path) -> None:
    lines = [
        "solver: saturnStokes",
        "",
        "units:",
        "  system: SI",
        "",
        "mesh:",
        "  type: box",
        f"  nx: {nx}",
        f"  ny: {ny}",
        f"  nz: {nz}",
        "  box:",
        "    xmin: 0.0",
        "    xmax: 1.0",
        "    ymin: 0.0",
        "    ymax: 1.0",
        "    zmin: 0.0",
        "    zmax: 1.0",
        "",
        "stokes:",
        f"  case: {case}",
        f"  mu: {mu}",
    ]
    if case == "poiseuille":
        lines.append(f"  forceX: {force_x}")
    elif case == "couette":
        lines.append(f"  wallVelocityX: {wall_velocity_x}")
    else:
        raise BenchmarkError(f"unsupported Stokes case: {case}")
    lines += [
        "  linearSolver: direct",
        "",
        "output:",
        f"  prefix: {prefix}",
        "  csv: true",
        "  json: true",
        "  exact: true",
        "  vtkEvery: 0",
        "",
    ]
    path.write_text("\n".join(lines), encoding="utf-8")


def analytic_profile(profile: dict[str, list[float]], case: str, mu: float,
                     force_x: float, wall_velocity_x: float,
                     ymin: float = 0.0, ymax: float = 1.0):
    """Independent Poiseuille/Couette reference and pressure."""
    height = ymax - ymin
    ue: list[float] = []
    pe: list[float] = []
    for y in profile["y"]:
        yy = y - ymin
        if case == "poiseuille":
            ue.append(force_x / (2.0 * mu) * yy * (height - yy))
        elif case == "couette":
            ue.append(wall_velocity_x * yy / height)
        else:
            raise BenchmarkError(f"unsupported Stokes case: {case}")
        pe.append(0.0)
    return ue, pe


def vector_error(profile: dict[str, list[float]], ue: list[float],
                 ve: list[float], we: list[float]) -> dict[str, float]:
    magnitudes = [
        math.sqrt((u - a) ** 2 + (v - b) ** 2 + (w - c) ** 2)
        for u, v, w, a, b, c in zip(profile["u"], profile["v"], profile["w"],
                                    ue, ve, we)
    ]
    count = len(magnitudes)
    return {
        "l1": sum(magnitudes) / count,
        "l2": math.sqrt(sum(v * v for v in magnitudes) / count),
        "linf": max(magnitudes, default=0.0),
        "samples": count,
    }


def pressure_error(profile: dict[str, list[float]],
                   pe: list[float]) -> dict[str, float]:
    raw = [p - e for p, e in zip(profile["p"], pe)]
    mean = sum(raw) / len(raw)
    values = [v - mean for v in raw]
    return {
        "l2": math.sqrt(sum(v * v for v in values) / len(values)),
        "linf": max((abs(v) for v in values), default=0.0),
    }


def main() -> int:
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument("--cpu", default="saturnCPU",
                        help="CPU (PETSc-enabled) command or build-tree binary")
    parser.add_argument("--dim", choices=["2d", "3d"], default="3d")
    parser.add_argument("--case", choices=["poiseuille", "couette"],
                        default="poiseuille")
    parser.add_argument("--nx", type=int, default=None)
    parser.add_argument("--ny", type=int, default=None)
    parser.add_argument("--nz", type=int, default=None)
    parser.add_argument("--mu", type=float, default=1.0)
    parser.add_argument("--force-x", type=float, default=1.0,
                        help="Poiseuille body force")
    parser.add_argument("--wall-velocity-x", type=float, default=1.0,
                        help="Couette top-wall velocity")
    parser.add_argument("--cpu-threads", type=int, default=1,
                        help="Kokkos threads; PETSc direct is effectively serial")
    parser.add_argument("--tolerance", type=float, default=5.0e-3,
                        help="maximum accepted velocity L2 error")
    parser.add_argument("--work", type=Path,
                        default=ROOT / "benchmarks/results/stokes")
    args = parser.parse_args()

    try:
        cpu = resolve_cli(args.cpu)
    except BenchmarkError as error:
        print(error, file=sys.stderr)
        return 2

    dim = 2 if args.dim == "2d" else 3
    default_n = 128 if dim == 2 else 16
    nx = args.nx if args.nx is not None else default_n
    ny = args.ny if args.ny is not None else default_n
    nz = 1 if dim == 2 else (args.nz if args.nz is not None else default_n)

    work = args.work.resolve()
    work.mkdir(parents=True, exist_ok=True)
    tag = f"{args.case}_{nx}x{ny}" + ("" if dim == 2 else f"x{nz}")
    prefix = work / tag
    case = work / f"{tag}.yaml"
    write_case(case, args.case, nx, ny, nz, args.mu, args.force_x,
               args.wall_velocity_x, prefix)

    print(f"+ {cpu} {case.name}")
    try:
        run_case(cpu, case, work, cpu_environment(args.cpu_threads))
        summary = load_json(prefix.with_suffix(".json"))
        profile = load_csv(prefix.with_suffix(".csv"))
    except (BenchmarkError, FileNotFoundError) as error:
        print(error, file=sys.stderr)
        return 1

    ue, pe = analytic_profile(profile, args.case, args.mu, args.force_x,
                              args.wall_velocity_x)
    ve = [0.0] * len(ue)
    we = [0.0] * len(ue)
    u_errors = error_metrics(profile["u"], ue)
    vec_errors = vector_error(profile, ue, ve, we)
    p_errors = pressure_error(profile, pe)
    consistency = independent_consistency(profile, ue, "u_exact")
    exact = summary.get("exact", {})

    lines = [
        "saturnStokes direct solver benchmark (CPU only: no CUDA PETSc)",
        f"case        : {args.case}",
        f"mesh        : {nx} x {ny}" + ("" if dim == 2 else f" x {nz}"),
        f"mu          : {args.mu:g}",
        f"forceX      : {args.force_x:g}" if args.case == "poiseuille"
        else f"wall U      : {args.wall_velocity_x:g}",
        f"backend     : {summary.get('backend', '?')}",
        f"ranks       : {summary.get('ranks', '?')}",
        f"iterations  : {summary.get('iterations', '?')}",
        f"residual    : {summary.get('residual', '?')}",
        f"wall        : {summary['wallSeconds']:.6f} s",
        "",
        "Velocity error vs independent analytic profile",
        "  " + format_metrics("u", u_errors),
        "  " + format_metrics("vector", vec_errors),
        "",
        "Pressure error (mean removed)",
        f"  pL2={p_errors['l2']:.6e}  pLinf={p_errors['linf']:.6e}",
    ]
    if exact:
        lines += [
            "",
            "Solver JSON exact norms",
            f"  L1={exact['l1']:.6e} L2={exact['l2']:.6e} "
            f"Linf={exact['linf']:.6e}",
            f"  pL2={exact['pL2']:.6e} pLinf={exact['pLinf']:.6e}",
        ]
    lines += [
        "",
        f"Analytic-column consistency : {consistency:.3e}",
        f"Tolerance                   : velocity L2 <= {args.tolerance:.1e}",
    ]

    passed = (
        u_errors["l2"] <= args.tolerance
        and vec_errors["l2"] <= args.tolerance
        and consistency <= 1.0e-10
        and (not exact or exact.get("pL2", 0.0) <= 1.0e-6)
    )
    lines.append(("PASS" if passed else "FAIL")
                 + ": direct solve agrees with the analytic Stokes profile")

    report = "\n".join(lines)
    print()
    print(report)
    write_text(work / "report.txt", report)
    write_json(work / "summary.json", {
        "solver": "saturnStokes",
        "linearSolver": "direct",
        "dimension": dim,
        "case": args.case,
        "mesh": {"nx": nx, "ny": ny, "nz": nz},
        "parameters": {
            "mu": args.mu,
            "forceX": args.force_x,
            "wallVelocityX": args.wall_velocity_x,
        },
        "backend": summary.get("backend"),
        "ranks": summary.get("ranks"),
        "iterations": summary.get("iterations"),
        "residual": summary.get("residual"),
        "wallSeconds": summary["wallSeconds"],
        "errorVsAnalytic": {
            "u": u_errors,
            "vector": vec_errors,
            "pressure": p_errors,
        },
        "solverExact": exact,
        "analyticConsistency": consistency,
        "gpuNote": "not run: the CUDA build has no PETSc, so saturnStokes is "
                   "CPU-only until a CUDA-enabled PETSc is available",
        "tolerance": args.tolerance,
        "passed": passed,
    })
    print(f"\nreport      : {work / 'report.txt'}")
    print(f"summary     : {work / 'summary.json'}")
    return 0 if passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
