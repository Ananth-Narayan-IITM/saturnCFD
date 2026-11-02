#!/usr/bin/env python3
"""Generate the Heat scaling YAML cases for PARAM Shakthi.

Strong cases keep the physical end time and timestep fixed, so every rank
count performs the same number of steps. Weak cases use a fixed timestep and
100 steps, giving a constant 32^3 cells per rank.
"""

from __future__ import annotations

from pathlib import Path

HERE = Path(__file__).resolve().parent
CASES = HERE / "cases"


def heat_case(name: str, n: int, end: float, adaptive: bool,
              dt: float) -> str:
    return f"""# saturnHeat 3D scaling case: {name}
solver: saturnHeat

units:
  system: SI

mesh:
  type: box
  nx: {n}
  ny: {n}
  nz: {n}
  box:
    xmin: 0.0
    xmax: 1.0
    ymin: 0.0
    ymax: 1.0
    zmin: 0.0
    zmax: 1.0

time:
  start: 0.0
  end: {end}
  scheme: ssprk3
  adaptive: {"true" if adaptive else "false"}
  cfl: 0.3
  dtMax: {dt}
  maxSteps: 100000

heat:
  alpha: 0.01
  velocityX: 1.0
  velocityY: 0.5
  velocityZ: 0.25
  source: 0.0
  periodic: true
  initial: fourier
  amplitude: 1.0
  kx: 6.283185307179586
  ky: 6.283185307179586
  kz: 6.283185307179586

numerics:
  convection: muscl
  limiter: vanLeer

output:
  prefix: results/{name}
  csv: false
  json: true
  exact: true
  vtkEvery: 0
"""


def main() -> int:
    CASES.mkdir(parents=True, exist_ok=True)
    strong = {
        "heatStrong64": (64, 0.2, 0.001),
        "heatStrong96": (96, 0.2, 0.001),
        "heatStrong128": (128, 0.2, 0.001),
        "heatStrong256": (256, 0.2, 0.001),
    }
    for name, (n, end, dt) in strong.items():
        (CASES / f"{name}.yaml").write_text(
            heat_case(name, n, end, True, dt), encoding="utf-8")
    weak = {
        "heatWeak032": 32,
        "heatWeak064": 64,
        "heatWeak096": 96,
        "heatWeak128": 128,
    }
    for name, n in weak.items():
        (CASES / f"{name}.yaml").write_text(
            heat_case(name, n, 0.05, False, 0.0005), encoding="utf-8")
    print(f"wrote {len(strong) + len(weak)} cases under {CASES}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
