# PARAM Shakthi Heat scaling

Two scripts run the CPU-vs-GPU Heat campaign on PARAM Shakthi (PARAM Rudra):

| file | role |
|---|---|
| `tools/compileSaturnCFD` | checks the cluster, builds Kokkos if needed, builds `buildCPUHPC` and `buildGPUHPC` |
| `tests/runScaling` | generates the cases, runs strong/weak scaling, collects JSON, plots |

Both are Python 3 and are run from this directory.

## Quick start

```bash
cd HPC/paramsakthi
./tools/compileSaturnCFD          # check + build CPU and GPU
./tests/runScaling --dry-run      # print the jobs
./tests/runScaling                # run everything (tmux/nohup, takes ~1 h CPU)
```

One backend, one stage:

```bash
./tools/compileSaturnCFD cpu
./tests/runScaling cpu --stage strong
./tests/runScaling gpu --stage weak
./tests/runScaling --plot-only    # re-plot without resubmitting
```

## Cluster facts

| node | specification |
|---|---|
| CPU | 2× Xeon Gold 6240R, 48 cores, 192 GB |
| GPU | 2× NVIDIA A100 80 GB PCIe, 48 cores, 192 GB |
| scheduler | Slurm 23.11.10 |

| partition | limit |
|---|---|
| `small` | 1 node, 1–48 cores |
| `medium` | 2–8 nodes, 49–384 cores |
| `sgpu` | 1 GPU |
| `gpu` | 2–8 GPUs, 2 per node |

Modules: `compiler/gcc/12.4`, `compiler/openmpi/5.0.6`,
`nvhpc-24.5/nvhpc-hpcx-cuda12/24.5`, `python/3.12`. There is no
PETSc/Kokkos/CMake module; Kokkos is built locally by `compileSaturnCFD` and
PETSc is not needed for Heat.

## tools/compileSaturnCFD

It runs `module purge` + the backend's modules in a login shell, then checks
`cmake` (>= 3.24), `git`, `gcc`, `mpicxx` (and `nvcc` for GPU), the source
tree, and the Slurm partitions. If anything is missing it stops with the
reason; nothing is built.

Then, per backend:

1. build and install Kokkos `4.7.04` under `~/.local/kokkos-<kind>-4.7.04`
   if the install is missing (`Kokkos_ARCH_NATIVE` for CPU,
   `Kokkos_ARCH_AMPERE80` for GPU);
2. configure `buildCPUHPC` (`SATURN_BACKEND=openmp`) or `buildGPUHPC`
   (`SATURN_BACKEND=cuda`, `cmake/toolchains/cuda-kokkos.cmake`);
3. write `tools/envCpu.json` / `tools/envGpu.json`.

`--check` stops after the checks. The JSON files hold the login-node `PATH`,
`LD_LIBRARY_PATH` and compiler variables. They matter because the compute
nodes have **no Lmod**: the batch jobs cannot call `module`, so `runScaling`
hands them this environment through `sbatch --export=ALL`. Re-run
`compileSaturnCFD` after changing modules.

The GPU build needs GCC 12.4 as the `nvcc_wrapper` host compiler and CMake
`3.24`; `CMAKE_CXX_EXTENSIONS=OFF` keeps the test at `-std=c++20` instead of
`gnu++2a`, which `nvcc` rejects.

## tests/runScaling

Cases are written to `tests/scaling/cases`. Every point is one `sbatch
--wait` job, so the queue limits are respected and no two points overlap.
Results go to `tests/scaling/strong` and `tests/scaling/weak`.

| stage | backend | points |
|---|---|---|
| strong | CPU | 64³ 1 core; 128³ with 1, 2, 4, 8, 16, 24, 48, 96, 192, 384 ranks; 128³ 1 rank × 48 threads; 256³ with 48, 192, 384 ranks |
| strong | GPU | 64³ 1 A100; 128³ with 1, 2, 4, 8 A100; 256³ 8 A100 |
| weak | CPU | 32³/rank at 1, 8, 27, 64 ranks (32³, 64³, 96³, 128³) |
| weak | GPU | 32³/GPU at 1 and 8 A100 (32³, 64³) |

Strong cases run 100 adaptive steps (`end 0.1`, `dtMax 1e-3`); weak cases run
100 fixed steps (`dt 5e-4`). The GPU weak series has only 1 and 8 GPUs because
only those give an integer cube with 32³ cells per GPU.

Expected wall time for the full CPU chain is about one hour, dominated by the
single-core 128³ point (~26 min at ~2 s/step, measured from the 64³ smoke
test). GPU points take seconds to minutes. Run the campaign in `tmux` or
`nohup`.

## Outputs

```text
tests/scaling/
  strong/  <tag>.json  strong.csv  speedup.png  efficiency.png
           timePerStep.png  cpuVsGpu.png  accuracy.png
  weak/    <tag>.json  weak.csv    efficiency.png
           timePerStep.png  accuracy.png
  cases/   run-<tag>.yaml
  logs/    <tag>-<jobid>.out
```

`<tag>` is `<case>-<backend>-<units>u-<threads>t`. The JSON is the solver
output; the CSV merges it (`backend, case, units, threads, steps, wall, spt,
cells, l2`). All figures are PNG at 150 dpi.

If a job fails, `runScaling` prints the Slurm log tail and stops, so the
campaign never continues on a broken environment.

## Fairness

- CPU strong scaling uses one thread per MPI rank; the 48-thread run is the
  node-level comparison.
- GPU runs use one MPI rank per GPU, 4 host cores per rank, and pin each rank
  to its own device with `SLURM_LOCALID % GPUs/node`.
- Both backends run the same YAML, mesh, end time and timestep.
- Heat is explicit, so time per step is the main metric; total time also
  includes start-up and I/O.

## Troubleshooting

- `mpirun: command not found` or a stale Lmod error in a log: the job did not
  inherit the captured environment. Re-run `./tools/compileSaturnCFD`, then
  `./tests/runScaling`.
- `UCP API version is incompatible ... actual 1.16.0` comes from mixing the
  module OpenMPI with the system UCX. It is only a warning in the captured
  environment; keep using `compileSaturnCFD` so the module libraries stay
  first on `LD_LIBRARY_PATH`.
- GPU job fails with "does not report a GPU backend": the binary fell back to
  the CPU. Check `nvcc --version` and rebuild with
  `./tools/compileSaturnCFD gpu`.
- `cmake` too old: `python3 -m pip install --user 'cmake>=3.24'`.

## Not in this campaign

Stokes GPU scaling. `saturnStokes` is a PETSc DMDA solver and the GPU preset
disables PETSc, so a meaningful GPU Stokes benchmark needs a PETSc CUDA build
plus a GPU-capable preconditioner, or a matrix-free GPU Stokes solver.
