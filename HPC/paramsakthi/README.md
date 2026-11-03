# PARAM Shakthi Heat scaling

Two scripts run the CPU-vs-GPU Heat campaign on PARAM Shakthi (PARAM Rudra),
and the same two also run on a workstation with one GPU:

| file | role |
|---|---|
| `tools/compileSaturnCFD` | checks the cluster, builds Kokkos if needed, builds `buildCPUHPC` and `buildGPUHPC` |
| `tests/runScaling` | generates the cases, runs strong/weak scaling, collects JSON, plots |

Both are Python 3 and are run from this directory.

## Quick start

```bash
cd HPC/paramsakthi
./tools/compileSaturnCFD          # check + build CPU and GPU
./tests/runScaling --smoke        # one 64^3 point per backend
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

## Terminology

| term | meaning |
|---|---|
| **node** | one machine: 2 sockets, 48 cores, 192 GB; GPU nodes add 2× A100 80 GB |
| **core** | one physical CPU pipeline; Slurm allocates cores |
| **thread** | an execution stream inside a process; OpenMP creates them, they share memory |
| **rank** | one MPI process; `mpirun -np 48` starts 48 ranks, each with its own memory |
| **task** | Slurm's word for one launched process; with MPI, 1 task = 1 rank |
| **unit** | this campaign's x-axis: **ranks** on the CPU, **GPUs** on the GPU |
| **`thr`** | OpenMP threads **per rank** (1 in the strong series, 48 in the node run) |
| **kernel** | a function launched on the GPU by one `parallel_for` |
| **warp** | 32 consecutive GPU lanes executing one instruction together |
| **coalescing** | consecutive lanes reading consecutive addresses, one memory transaction |

MPI spreads work across processes and exchanges messages (halo exchange);
OpenMP spreads work across threads that share the arrays. One A100 is driven
by one rank, so **"2 GPU units" means two ranks on two GPUs**, and "48 ranks"
means 48 processes, one core each.

Speedup is `S(n) = T(1)/T(n)` and efficiency is `S(n)/n`, where `n` is ranks on
the CPU axis and GPUs on the GPU axis. One core and one A100 are not
comparable units of cost: report the ratio with its units.

The full version, with the hardware/software ladder and the four run
configurations, is in `docs/master.md` section 8.

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

Expected wall time for the full CPU chain is about 45 minutes, dominated by
the single-core 128³ point (~16 min). GPU points take seconds to minutes. Run
the campaign in `tmux` or `nohup`.

## Workstation / single GPU

Neither script needs Slurm. When `sbatch` is absent they run in local mode:

- `compileSaturnCFD` skips the partition check and uses the current shell
  environment instead of `module` (a missing `module` is not an error), and it
  selects the Kokkos CUDA arch from `nvidia-smi` (`AMPERE80` on an A100).
- `runScaling` caps the campaign to the machine: CPU points up to the cores
  available to the process, GPU points up to the GPUs `nvidia-smi` reports,
  and it skips the GPU stage when there is none. If `mpirun` is missing it runs
  a single process at a time.
- Points run directly, one after another, with logs in `tests/scaling/logs/`.

With one GPU only the 1-GPU points exist (64³ and 128³), so there is no GPU
scaling curve on that machine — but the CPU curve up to the core count and the
1-core-vs-1-GPU comparison are still valid and use the same cases and code.

```
== local mode ==
  cores 24, GPUs 1, launcher /usr/bin/mpirun

== cpu strong ==
  heatStrong64-cpu-1u-1t      ...
```

`--dry-run` prints the exact commands without running them, so the capping can
be checked before a long run.

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

## Performance note

The 3D kernels iterate with `Iterate::Left` (`src/core/Policy.hpp`) so that the
first index, which is contiguous in the `LayoutLeft` fields, is the fastest
one. Kokkos' default is the opposite order, which walks the fields with a
`ny*nz` stride. On one core the 32³ weak case (100 steps) went from 31.6 s to
19.4 s with `l1/l2/linf` unchanged to the last digit, and the test suite stays
at 47/47. The GPU build benefits from the same change through coalesced loads.

The measured single-core cost is about 5–6 µs per cell-step for the
MUSCL + Laplacian + SSPRK3 scheme (three RHS evaluations per step, eight
kernels each). That is the number behind the campaign length, and it is the
same on the login node and on a compute node.

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
