#!/usr/bin/env python3
"""Benchmark the steady ``saturnStokes`` direct solve on CPU and, if available,
GPU.

The direct path is PETSc ``preonly`` + LU.  PETSc decides where the
factorisation runs, so a GPU direct solve needs a CUDA-enabled PETSc build with
a GPU-capable LU (for example cuSOLVER through PETSc).  The repository's CUDA
build currently keeps PETSc off, which means ``saturnGPU`` does not register
``saturnStokes``.  This script detects that case and reports the GPU point as
*skipped* instead of failing; when a CUDA-PETSc binary exists, pass it with
``--gpu`` and the same benchmark runs on both backends.

Example (from the repository root):

    python3 benchmarks/benchmarkStokes.py --cpu saturnCPU --gpu saturnGPU --dim 3d

Use ``--no-gpu`` for a CPU-only run.
"""

from __future__ import annotations

import argparse
import math
import os
import subprocess
import sys
from pathlib import Path

from benchmarkCommon import (
    DEFAULT_AGREEMENT,
    BenchmarkError,
    check_same_grid,
    cpu_environment,
    difference_metrics,
    error_metrics,
    format_error_values,
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


def vector_difference(a: dict[str, list[float]],
                      b: dict[str, list[float]]) -> dict[str, float]:
    magnitudes = [
        math.sqrt((au - bu) ** 2 + (av - bv) ** 2 + (aw - bw) ** 2)
        for au, av, aw, bu, bv, bw in zip(a["u"], a["v"], a["w"],
                                          b["u"], b["v"], b["w"])
    ]
    ref2 = sum(u * u + v * v + w * w
               for u, v, w in zip(a["u"], a["v"], a["w"]))
    diff2 = sum(m * m for m in magnitudes)
    return {
        "maxAbs": max(magnitudes, default=0.0),
        "rms": math.sqrt(diff2 / len(magnitudes)),
        "relativeL2": math.sqrt(diff2 / ref2) if ref2 > 0 else 0.0,
    }


def supports_stokes(cli: str) -> tuple[bool, str]:
    """Return whether a binary registers saturnStokes and, if not, why."""
    try:
        result = subprocess.run(
            [cli, "-listSolvers"],
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )
    except OSError as error:
        return False, f"cannot run {cli}: {error}"
    if result.returncode != 0:
        return False, f"{cli} -listSolvers failed ({result.returncode})"
    names = {line.strip() for line in result.stdout.splitlines()
             if line.strip()}
    if "saturnStokes" not in names:
        return False, (
            f"{cli} does not register saturnStokes "
            "(the CUDA build has PETSc off)"
        )
    return True, ""


def evaluate(profile: dict[str, list[float]], case: str, mu: float,
             force_x: float, wall_velocity_x: float) -> dict:
    ue, pe = analytic_profile(profile, case, mu, force_x, wall_velocity_x)
    ve = [0.0] * len(ue)
    we = [0.0] * len(ue)
    return {
        "u": error_metrics(profile["u"], ue),
        "vector": vector_error(profile, ue, ve, we),
        "pressure": pressure_error(profile, pe),
        "analyticConsistency": independent_consistency(profile, ue,
                                                       "u_exact"),
    }


def main() -> int:
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument("--cpu", default="saturnCPU",
                        help="CPU (PETSc-enabled) command or build-tree binary")
    parser.add_argument("--gpu", default="saturnGPU",
                        help="GPU command; skipped if it does not register "
                             "saturnStokes")
    parser.add_argument("--no-gpu", action="store_true",
                        help="run the CPU backend only")
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
    parser.add_argument("--agreement", type=float, default=DEFAULT_AGREEMENT,
                        help="maximum accepted CPU/GPU relative velocity "
                             "difference")
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

    gpu: str | None = None
    gpu_skip = ""
    if not args.no_gpu and args.gpu:
        try:
            candidate = resolve_cli(args.gpu)
        except BenchmarkError as error:
            gpu_skip = str(error)
        else:
            available, reason = supports_stokes(candidate)
            if available:
                gpu = candidate
            else:
                gpu_skip = reason

    work = args.work.resolve()
    work.mkdir(parents=True, exist_ok=True)
    tag = f"{args.case}_{nx}x{ny}" + ("" if dim == 2 else f"x{nz}")

    def run_backend(name: str, cli: str) -> tuple[dict, dict]:
        prefix = work / (tag if name == "cpu" else f"{tag}_{name}")
        case = work / (f"{tag}.yaml" if name == "cpu" else f"{tag}_{name}.yaml")
        write_case(case, args.case, nx, ny, nz, args.mu, args.force_x,
                   args.wall_velocity_x, prefix)
        env = (cpu_environment(args.cpu_threads) if name == "cpu"
               else os.environ.copy())
        print(f"+ {cli} {case.name}")
        try:
            run_case(cli, case, work, env)
            summary = load_json(prefix.with_suffix(".json"))
            profile = load_csv(prefix.with_suffix(".csv"))
        except (BenchmarkError, FileNotFoundError) as error:
            raise BenchmarkError(f"{name} backend failed: {error}") from error
        return summary, profile

    try:
        cpu_summary, cpu_profile = run_backend("cpu", cpu)
        cpu_eval = evaluate(cpu_profile, args.case, args.mu, args.force_x,
                            args.wall_velocity_x)
        gpu_summary = None
        gpu_profile = None
        gpu_eval = None
        if gpu is not None:
            gpu_summary, gpu_profile = run_backend("gpu", gpu)
            gpu_eval = evaluate(gpu_profile, args.case, args.mu, args.force_x,
                                args.wall_velocity_x)
    except BenchmarkError as error:
        print(error, file=sys.stderr)
        return 1

    agreement = None
    if gpu_profile is not None:
        try:
            check_same_grid(cpu_profile, gpu_profile)
        except BenchmarkError as error:
            print(error, file=sys.stderr)
            return 1
        agreement = {
            "u": difference_metrics(cpu_profile["u"], gpu_profile["u"]),
            "vector": vector_difference(cpu_profile, gpu_profile),
        }
        print(f"  cpu: wall={cpu_summary['wallSeconds']:.6f} s")
        print(f"  gpu: wall={gpu_summary['wallSeconds']:.6f} s")
    else:
        print(f"  cpu: wall={cpu_summary['wallSeconds']:.6f} s")
        print(f"  gpu: skipped — {gpu_skip}")

    lines = [
        "saturnStokes direct solver benchmark",
        f"case        : {args.case}",
        f"mesh        : {nx} x {ny}" + ("" if dim == 2 else f" x {nz}"),
        f"mu          : {args.mu:g}",
        f"forceX      : {args.force_x:g}" if args.case == "poiseuille"
        else f"wall U      : {args.wall_velocity_x:g}",
        f"linearSolver: direct (PETSc preonly + LU)",
        "",
        f"{'backend':<10s} {'wall [s]':>12s} {'its':>5s}  error vs analytic",
        f"{'cpu':<10s} {cpu_summary['wallSeconds']:>12.6f} "
        f"{cpu_summary.get('iterations', '?'):>5}  "
        + format_error_values(cpu_eval["u"]),
    ]
    if gpu_summary is not None:
        lines.append(
            f"{'gpu':<10s} {gpu_summary['wallSeconds']:>12.6f} "
            f"{gpu_summary.get('iterations', '?'):>5}  "
            + format_error_values(gpu_eval["u"])
        )
    else:
        lines.append(f"{'gpu':<10s} {'skipped':>12s}     -  no saturnStokes")
        lines += ["", f"GPU note    : {gpu_skip}",
                  "              A GPU direct solve needs a CUDA-enabled "
                  "PETSc with a GPU-capable LU."]

    lines += [
        "",
        "Velocity vector error vs analytic",
        "  " + format_metrics("cpu", cpu_eval["vector"]),
    ]
    if gpu_eval is not None:
        lines.append("  " + format_metrics("gpu", gpu_eval["vector"]))

    lines += [
        "",
        "Pressure error (mean removed)",
        f"  cpu pL2={cpu_eval['pressure']['l2']:.6e}  "
        f"pLinf={cpu_eval['pressure']['linf']:.6e}",
    ]
    if gpu_eval is not None:
        lines.append(
            f"  gpu pL2={gpu_eval['pressure']['l2']:.6e}  "
            f"pLinf={gpu_eval['pressure']['linf']:.6e}"
        )

    if agreement is not None:
        lines += [
            "",
            "CPU vs GPU solution difference",
            f"  max |u_cpu - u_gpu|     : {agreement['u']['maxAbs']:.6e}",
            f"  u relative L2           : "
            f"{agreement['u']['relativeL2']:.6e}",
            f"  velocity relative L2    : "
            f"{agreement['vector']['relativeL2']:.6e}",
        ]

    exact = cpu_summary.get("exact", {})
    if exact:
        lines += [
            "",
            "Solver JSON exact norms (CPU)",
            f"  L1={exact['l1']:.6e} L2={exact['l2']:.6e} "
            f"Linf={exact['linf']:.6e}",
            f"  pL2={exact['pL2']:.6e} pLinf={exact['pLinf']:.6e}",
        ]
    lines += [
        "",
        f"Analytic-column consistency (CPU) : "
        f"{cpu_eval['analyticConsistency']:.3e}",
        f"Tolerance                         : velocity L2 <= "
        f"{args.tolerance:.1e}",
    ]
    if agreement is not None:
        lines.append(
            f"CPU/GPU agreement                 : relative velocity L2 <= "
            f"{args.agreement:.1e}"
        )

    passed = (
        cpu_eval["u"]["l2"] <= args.tolerance
        and cpu_eval["vector"]["l2"] <= args.tolerance
        and cpu_eval["analyticConsistency"] <= 1.0e-10
        and (not exact or exact.get("pL2", 0.0) <= 1.0e-6)
    )
    if gpu_eval is not None:
        passed = (
            passed
            and gpu_eval["u"]["l2"] <= args.tolerance
            and gpu_eval["vector"]["l2"] <= args.tolerance
            and gpu_eval["analyticConsistency"] <= 1.0e-10
            and agreement["vector"]["relativeL2"] <= args.agreement
        )
    lines.append(("PASS" if passed else "FAIL")
                 + ": direct solve agrees with the analytic Stokes profile")

    report = "\n".join(lines)
    print()
    print(report)
    write_text(work / "report.txt", report)

    backends = {
        "cpu": {
            "command": cpu,
            "status": "ran",
            "backend": cpu_summary.get("backend"),
            "ranks": cpu_summary.get("ranks"),
            "iterations": cpu_summary.get("iterations"),
            "residual": cpu_summary.get("residual"),
            "wallSeconds": cpu_summary["wallSeconds"],
            "errorVsAnalytic": cpu_eval,
            "solverExact": exact,
        },
    }
    if gpu_summary is not None:
        backends["gpu"] = {
            "command": gpu,
            "status": "ran",
            "backend": gpu_summary.get("backend"),
            "ranks": gpu_summary.get("ranks"),
            "iterations": gpu_summary.get("iterations"),
            "residual": gpu_summary.get("residual"),
            "wallSeconds": gpu_summary["wallSeconds"],
            "errorVsAnalytic": gpu_eval,
            "solverExact": gpu_summary.get("exact", {}),
        }
    else:
        backends["gpu"] = {
            "command": args.gpu,
            "status": "skipped",
            "reason": gpu_skip,
        }

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
        "backends": backends,
        "cpuVsGpu": agreement,
        "gpuNote": (
            "A GPU direct solve needs a CUDA-enabled PETSc with a GPU-capable "
            "LU. The current CUDA build has PETSc off, so saturnGPU does not "
            "register saturnStokes and the GPU point is skipped."
        ),
        "tolerance": args.tolerance,
        "agreementTolerance": args.agreement,
        "passed": passed,
    })
    print(f"\nreport      : {work / 'report.txt'}")
    print(f"summary     : {work / 'summary.json'}")
    return 0 if passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
