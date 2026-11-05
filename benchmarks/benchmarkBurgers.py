#!/usr/bin/env python3
"""Benchmark the 2D scalar Burgers case on CPU and GPU.

Besides wall-clock time, the script verifies that both builds are solving the
same problem:

- it recomputes the diagonal travelling wave independently of the solver;
- it reports L1/L2/Linf errors of the CPU and GPU profiles against that
  analytic solution;
- it compares the CPU and GPU profiles point by point.

The case keeps VTK output disabled.  CSV output is enabled because the final
profile is needed for the error report; it is written once, after the time
loop, so it does not affect the measured solver time.

Example (from the repository root):

    python3 benchmarks/benchmarkBurgers.py \
        --cpu saturnCPU --gpu saturnGPU \
        --nx 256 --ny 256 --t-end 0.2
"""

from __future__ import annotations

import argparse
import math
import os
import sys
from pathlib import Path
from textwrap import dedent

from benchmarkCommon import (
    DEFAULT_AGREEMENT,
    DEFAULT_TOLERANCE,
    BenchmarkError,
    check_same_grid,
    cpu_environment,
    difference_metrics,
    error_metrics,
    format_error_values,
    independent_consistency,
    load_csv,
    load_json,
    resolve_cli,
    run_case,
    write_json,
    write_text,
)

ROOT = Path(__file__).resolve().parents[1]


def write_case(path: Path, nx: int, ny: int, t_end: float, nu: float,
               prefix: Path, dt_max: float, cfl: float) -> None:
    path.write_text(
        dedent(
            f"""\
            solver: saturnBurger

            units:
              system: SI

            mesh:
              type: box
              nx: {nx}
              ny: {ny}
              nz: 1
              box:
                xmin: -1.0
                xmax: 1.0
                ymin: -1.0
                ymax: 1.0
                zmin: 0.0
                zmax: 1.0

            time:
              start: 0.0
              end: {t_end}
              scheme: ssprk3
              adaptive: true
              cfl: {cfl}
              dtMax: {dt_max}
              maxSteps: 1000000

            burgers:
              nu: {nu}
              velocity: 1.0
              x0: 0.0
              y0: 0.0
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
              vtkEvery: 0
            """
        )
    )


def exact_solution(x: list[float], y: list[float], t: float, speed: float,
                   nu: float, x0: float, y0: float) -> list[float]:
    """Independent 2D diagonal travelling-wave reference."""
    return [
        0.5 * speed
        * (1.0 - math.tanh(speed * (xx + yy - x0 - y0 - speed * t)
                          / (4.0 * nu)))
        for xx, yy in zip(x, y)
    ]


def main() -> int:
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument("--cpu", default="saturnCPU",
                        help="CPU command or build-tree binary")
    parser.add_argument("--gpu", default="saturnGPU",
                        help="GPU command or build-tree binary")
    parser.add_argument("--nx", type=int, default=256)
    parser.add_argument("--ny", type=int, default=256)
    parser.add_argument("--nu", type=float, default=0.02)
    parser.add_argument("--t-end", type=float, default=0.2)
    parser.add_argument("--dt-max", type=float, default=0.005)
    parser.add_argument("--cfl", type=float, default=0.3)
    parser.add_argument("--cpu-threads", type=int,
                        default=min(12, os.cpu_count() or 1))
    parser.add_argument("--tolerance", type=float, default=DEFAULT_TOLERANCE,
                        help="maximum accepted L2 error against analytic")
    parser.add_argument("--agreement", type=float, default=DEFAULT_AGREEMENT,
                        help="maximum accepted CPU/GPU relative L2 difference")
    parser.add_argument("--work", type=Path,
                        default=ROOT / "benchmarks/results/burgers")
    args = parser.parse_args()

    try:
        cpu = resolve_cli(args.cpu)
        gpu = resolve_cli(args.gpu)
    except BenchmarkError as error:
        print(error, file=sys.stderr)
        return 2

    work = args.work.resolve()
    work.mkdir(parents=True, exist_ok=True)

    speed = 1.0
    x0 = 0.0
    y0 = 0.0
    results: dict[str, dict] = {}
    profiles: dict[str, dict[str, list[float]]] = {}
    analytic_by_backend: dict[str, list[float]] = {}

    for name, cli, env in [
        ("cpu", cpu, cpu_environment(args.cpu_threads)),
        ("gpu", gpu, os.environ.copy()),
    ]:
        prefix = work / f"{name}_{args.nx}x{args.ny}"
        case = work / f"{name}_{args.nx}x{args.ny}.yaml"
        write_case(case, args.nx, args.ny, args.t_end, args.nu, prefix,
                   args.dt_max, args.cfl)
        print(f"+ {cli} {case.name}")
        try:
            run_case(cli, case, work, env)
            summary = load_json(prefix.with_suffix(".json"))
            profile = load_csv(prefix.with_suffix(".csv"))
        except (BenchmarkError, FileNotFoundError) as error:
            print(error, file=sys.stderr)
            return 1
        results[name] = summary
        profiles[name] = profile
        analytic_by_backend[name] = exact_solution(
            profile["x"], profile["y"], args.t_end, speed, args.nu, x0, y0)
        print(
            f"  {name}: steps={summary['steps']} "
            f"wall={summary['wallSeconds']:.6f} s "
            f"backend={summary.get('backend', '?')}"
        )

    cpu_profile = profiles["cpu"]
    gpu_profile = profiles["gpu"]
    try:
        check_same_grid(cpu_profile, gpu_profile)
    except BenchmarkError as error:
        print(error, file=sys.stderr)
        return 1

    errors = {
        name: error_metrics(profiles[name]["u_numerical"],
                            analytic_by_backend[name])
        for name in ("cpu", "gpu")
    }
    consistency = {
        name: independent_consistency(profiles[name], analytic_by_backend[name])
        for name in ("cpu", "gpu")
    }
    agreement = difference_metrics(cpu_profile["u_numerical"],
                                    gpu_profile["u_numerical"])

    cpu_time = results["cpu"]["wallSeconds"]
    gpu_time = results["gpu"]["wallSeconds"]

    lines = [
        "saturnBurgers CPU/GPU benchmark",
        f"grid        : {args.nx} x {args.ny}",
        f"t_end       : {args.t_end:g} s",
        f"nu          : {args.nu:g}",
        f"U, x0, y0   : {speed:g}, {x0:g}, {y0:g}",
        f"CPU threads : {args.cpu_threads}",
        "",
        f"{'backend':<10s} {'steps':>7s} {'wall [s]':>12s}  error vs analytic",
        f"{'cpu':<10s} {results['cpu']['steps']:>7d} {cpu_time:>12.6f}  "
        + format_error_values(errors["cpu"]),
        f"{'gpu':<10s} {results['gpu']['steps']:>7d} {gpu_time:>12.6f}  "
        + format_error_values(errors["gpu"]),
        "",
        "CPU vs GPU profile difference",
        f"  max |u_cpu - u_gpu| : {agreement['maxAbs']:.6e}",
        f"  RMS difference      : {agreement['rms']:.6e}",
        f"  relative L2         : {agreement['relativeL2']:.6e}",
        f"  grid match          : yes",
        "",
        "Analytic-column consistency (solver u_exact vs independent reference)",
        f"  cpu                 : {consistency['cpu']:.3e}",
        f"  gpu                 : {consistency['gpu']:.3e}",
        "",
        "Solver JSON exact norms",
    ]
    for name in ("cpu", "gpu"):
        exact = results[name].get("exact")
        if exact:
            lines.append(
                f"  {name}: L1={exact['l1']:.6e} L2={exact['l2']:.6e} "
                f"Linf={exact['linf']:.6e}"
            )
        else:
            lines.append(f"  {name}: not reported")
    lines += [
        "",
        f"CPU wall / GPU wall : {cpu_time / gpu_time:.2f} x",
        f"Tolerance           : L2 <= {args.tolerance:.1e}, "
        f"CPU/GPU relative L2 <= {args.agreement:.1e}",
    ]

    passed = (
        errors["cpu"]["l2"] <= args.tolerance
        and errors["gpu"]["l2"] <= args.tolerance
        and agreement["relativeL2"] <= args.agreement
        and consistency["cpu"] <= 1.0e-10
        and consistency["gpu"] <= 1.0e-10
    )
    lines.append(("PASS" if passed else "FAIL") + ": CPU and GPU agree with "
                 "the analytic solution and with each other")

    report = "\n".join(lines)
    print()
    print(report)
    write_text(work / "report.txt", report)
    write_json(work / "summary.json", {
        "solver": "saturnBurger",
        "grid": {"nx": args.nx, "ny": args.ny},
        "parameters": {
            "t_end": args.t_end,
            "nu": args.nu,
            "velocity": speed,
            "x0": x0,
            "y0": y0,
            "dt_max": args.dt_max,
            "cfl": args.cfl,
        },
        "backends": {
            name: {
                "command": cpu if name == "cpu" else gpu,
                "backend": results[name].get("backend"),
                "steps": results[name]["steps"],
                "wallSeconds": results[name]["wallSeconds"],
                "errorVsAnalytic": errors[name],
                "solverExact": results[name].get("exact"),
                "analyticConsistency": consistency[name],
            }
            for name in ("cpu", "gpu")
        },
        "cpuVsGpu": agreement,
        "tolerance": args.tolerance,
        "agreementTolerance": args.agreement,
        "passed": passed,
    })
    print(f"\nreport      : {work / 'report.txt'}")
    print(f"summary     : {work / 'summary.json'}")
    return 0 if passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
