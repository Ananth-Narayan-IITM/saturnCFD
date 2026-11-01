#!/usr/bin/env python3
"""Compare CPU and GPU wall-clock time for the 2D scalar Burgers case.

This runs the same generated case through two builds of ``saturnCFD`` and
reads the ``wallSeconds`` field from each JSON summary.

Example (from the repository root):

    python3 benchmarks/benchmarkBurgers.py \
        --cpu saturnCPU --gpu saturnGPU \
        --nx 256 --ny 256 --t-end 0.2

The GPU executable must be a CUDA build (``cmake --preset gpu`` with a
CUDA-enabled Kokkos). The case keeps only the time loop and the solver; CSV,
analytic comparison and VTK output are disabled, so the numbers measure the
solver rather than file I/O.
"""

from __future__ import annotations

import argparse
import json
import os
import shutil
import subprocess
import sys
from pathlib import Path
from textwrap import dedent

ROOT = Path(__file__).resolve().parents[1]


def write_case(path: Path, nx: int, ny: int, t_end: float, nu: float,
               prefix: Path) -> None:
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
              cfl: 0.3
              dtMax: 0.005
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
              csv: false
              json: true
              exact: false
              vtkEvery: 0
            """
        )
    )


def run(cli: str, case: Path, cwd: Path, env: dict[str, str]) -> dict:
    result = subprocess.run(
        [cli, case.name],
        cwd=cwd,
        env=env,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    if result.returncode != 0:
        raise RuntimeError(
            f"{cli} failed ({result.returncode})\n{result.stdout}\n{result.stderr}"
        )
    return result


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cpu", default="saturnCPU",
                        help="CPU command or build-tree binary")
    parser.add_argument("--gpu", default="saturnGPU",
                        help="GPU command or build-tree binary")
    parser.add_argument("--nx", type=int, default=256)
    parser.add_argument("--ny", type=int, default=256)
    parser.add_argument("--nu", type=float, default=0.02)
    parser.add_argument("--t-end", type=float, default=0.2)
    parser.add_argument("--cpu-threads", type=int,
                        default=min(12, os.cpu_count() or 1))
    parser.add_argument("--work", type=Path,
                        default=ROOT / "results/burgers/benchmark")
    args = parser.parse_args()

    def resolve_cli(value: str) -> str:
        path = Path(value)
        if path.exists():
            return str(path.resolve())
        found = shutil.which(value)
        if found:
            return found
        raise SystemExit(
            f"missing executable: {value}\n"
            "install the backends with tools/compileSaturnCFD, or pass a path"
        )

    cpu = resolve_cli(args.cpu)
    gpu = resolve_cli(args.gpu)

    work = args.work.resolve()
    work.mkdir(parents=True, exist_ok=True)

    results = {}
    for name, cli, env in [
        ("cpu", cpu, {**os.environ, "OMP_NUM_THREADS": str(args.cpu_threads),
                      "OMP_PROC_BIND": "spread"}),
        ("gpu", gpu, os.environ.copy()),
    ]:
        prefix = work / f"{name}_{args.nx}x{args.ny}"
        case = work / f"{name}_{args.nx}x{args.ny}.yaml"
        write_case(case, args.nx, args.ny, args.t_end, args.nu, prefix)
        print(f"+ {cli} {case.name}")
        try:
            run(cli, case, work, env)
        except RuntimeError as error:
            print(error, file=sys.stderr)
            return 1
        summary = json.loads(prefix.with_suffix(".json").read_text())
        results[name] = summary
        print(
            f"  {name}: steps={summary['steps']} "
            f"wall={summary['wallSeconds']:.6f} s"
        )

    cpu_time = results["cpu"]["wallSeconds"]
    gpu_time = results["gpu"]["wallSeconds"]
    print()
    print(f"grid       : {args.nx} x {args.ny}")
    print(f"t_end      : {args.t_end:g} s")
    print(f"CPU threads: {args.cpu_threads}")
    print(f"CPU wall   : {cpu_time:.6f} s")
    print(f"GPU wall   : {gpu_time:.6f} s")
    print(f"speedup    : {cpu_time / gpu_time:.2f} x")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
