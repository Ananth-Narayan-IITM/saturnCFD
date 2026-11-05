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
| `benchmarkStokes.py` | `saturnStokes` 2D/3D | CPU + optional GPU | direct LU; GPU needs CUDA PETSc |

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

## Stokes direct solver (CPU, and GPU when PETSc supports it)

The direct path is PETSc `preonly` + LU. PETSc decides where the factorisation
runs, so a GPU direct solve needs a **CUDA-enabled PETSc build with a
GPU-capable LU** (for example cuSOLVER through PETSc). The repository's CUDA
build currently keeps PETSc off (`cmake --preset gpu` sets
`SATURN_ENABLE_PETSC=OFF`), so `saturnGPU` does not register `saturnStokes`.

The benchmark runs the CPU direct solver and attempts the GPU binary by
default. If the GPU binary has no `saturnStokes`, the GPU point is reported as
**skipped** and the CPU result still passes/fails on its own. When a CUDA-PETSc
build exists, the same command runs both backends and compares them:

```bash
# CPU + GPU (GPU is skipped automatically with the current CUDA build)
python3 benchmarks/benchmarkStokes.py --cpu saturnCPU --gpu saturnGPU --dim 3d

# CPU only
python3 benchmarks/benchmarkStokes.py --cpu saturnCPU --dim 2d --case couette --no-gpu
```

Use `--nx`, `--ny`, `--nz` to change the mesh. The report includes iterations,
residual, wall time, velocity and pressure errors, and the CPU/GPU solution
difference when both backends run.

## GPU build

The GPU executable must be a CUDA build (`cmake --preset gpu` with a
CUDA-enabled Kokkos). See [`../docs/guides/gpu.md`](../docs/guides/gpu.md).
SaturnStokes is not built there until PETSc is available for CUDA.
