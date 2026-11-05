# benchmarks

Standalone performance and correctness measurements. These scripts are not part
of CTest: they take minutes, need a GPU build for the GPU half, and produce
timing/error tables rather than pass/fail unit checks.

All three scripts run the same case through the solver, read the final profile
and JSON summary, recompute the analytic solution independently, and report:

- wall time (and CPU/GPU speedup where both backends exist);
- L1/L2/Linf error of each backend against the analytic solution;
- CPU-vs-GPU profile difference for the scalar solvers;
- the solver's own `u_exact` column against the independent reference.

Generated files go under `benchmarks/results/<solver>/` by default. Each run
writes the case YAML, per-backend `.csv`/`.json`, a human-readable `report.txt`
and a machine-readable `summary.json`.

## Scripts

| script | solver | backends | notes |
|---|---|---|---|
| `benchmarkBurgers.py` | `saturnBurger` 2D | CPU + GPU | diagonal travelling wave |
| `benchmarkHeat.py` | `saturnHeat` 2D | CPU + GPU | periodic Fourier mode |
| `benchmarkStokes.py` | `saturnStokes` 2D/3D | CPU only | direct LU; no CUDA PETSc yet |

`benchmarkCommon.py` contains the shared CSV/JSON readers and error helpers.

## Burgers CPU vs GPU

Generates one 2D scalar Burgers case, runs it through two builds, and reads the
final profile plus the JSON summary. VTK output is disabled. CSV output is
enabled only because the final profile is needed for the error report; it is
written once after the time loop, so it does not affect the measured solver
time.

From the repository root:

```bash
python3 benchmarks/benchmarkBurgers.py \
    --cpu saturnCPU --gpu saturnGPU \
    --nx 256 --ny 256 --t-end 0.2
```

The report contains, for example:

```text
backend      steps     wall [s]  error vs analytic
cpu            525    32.466719  L1=...  L2=...  Linf=...
gpu            525    16.329617  L1=...  L2=...  Linf=...

CPU vs GPU profile difference
  max |u_cpu - u_gpu| : ...
  relative L2         : ...
```

Use `--tolerance` for the accepted analytic L2 error and `--agreement` for the
accepted CPU/GPU relative L2 difference.

## Heat CPU vs GPU

The heat case is the periodic Fourier mode from
`examples/saturnHeat/saturnHeat2D`:

```bash
python3 benchmarks/benchmarkHeat.py \
    --cpu saturnCPU --gpu saturnGPU \
    --nx 256 --ny 256 --t-end 0.2
```

The analytic reference is recomputed from the case parameters:

$$
T(x,y,t)=A\sin(k_x x+k_y y-(a_x k_x+a_y k_y)t)
\exp\!\left[-\alpha(k_x^2+k_y^2)t\right].
$$

Use `--alpha`, `--velocity-x`, `--velocity-y`, `--kx`, `--ky` and
`--amplitude` to change the case.

## Stokes direct solver (CPU only)

The CUDA build keeps PETSc off (`cmake --preset gpu` sets
`SATURN_ENABLE_PETSC=OFF`), so `saturnStokes` is only available in the CPU
build. The benchmark therefore uses the CPU direct LU solver and validates it
against the analytic Poiseuille or Couette profile:

```bash
# 3D Poiseuille, 16^3
python3 benchmarks/benchmarkStokes.py --cpu saturnCPU --dim 3d

# 2D Couette, 128^2
python3 benchmarks/benchmarkStokes.py --cpu saturnCPU --dim 2d --case couette
```

Use `--nx`, `--ny`, `--nz` to change the mesh. The script reports iterations,
residual, wall time, velocity and pressure errors, and the solver JSON norms.
When a CUDA-enabled PETSc build becomes available, pass that binary with
`--cpu`; the script is backend-agnostic.

## GPU build

The GPU executable must be a CUDA build (`cmake --preset gpu` with a
CUDA-enabled Kokkos). See [`../docs/guides/gpu.md`](../docs/guides/gpu.md).
SaturnStokes is not built there until PETSc is available for CUDA.
