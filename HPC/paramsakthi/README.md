# PARAM Shakthi Heat scaling

This directory runs the **3D heat / convection–diffusion** solver on the
PARAM Shakthi cluster to compare CPU and GPU.

The cluster document says:

| node | specification |
|---|---|
| CPU | 2× Intel Xeon Gold 6240R, 48 cores, 192 GB |
| high memory | same CPU, 768 GB |
| GPU | 2× NVIDIA A100 80 GB, 48 cores, 192 GB |
| network | InfiniBand NDR |
| scheduler | Slurm 23.11.10 |

Partitions used here:

| partition | useful limit |
|---|---|
| `small` | 1–48 cores, one CPU node |
| `medium` | 49–384 cores, 2–8 CPU nodes |
| `large` | 385–6144 cores |
| `sgpu` | 1 core / 1 GPU |
| `gpu` | 1 core / 2 GPUs up to 192 cores / 8 GPUs |

The GPU nodes have **NVIDIA A100**, not Intel GPUs. The Kokkos CUDA backend is
therefore the target.

## Why Heat first

Heat is a Kokkos scalar solver and is already GPU-enabled. Stokes is a PETSc
DMDA solver and is not yet GPU-accelerated; it will be a separate PETSc/CUDA
work package. This campaign answers:

- how much faster is one A100 than one CPU core / one CPU node for the same
  Heat problem;
- how well does the Heat solver scale on 1, 2, 4 and 8 A100s;
- how well does it scale on 1–8 CPU nodes.

## File roles

| file | role |
|---|---|
| `envCpu` | loads GCC 12.4 + OpenMPI 5.0.6 + Python, sets the CPU Kokkos path and compilers |
| `envGpu` | loads GCC 12.4 + nvhpc/CUDA 12 + HPC-X, sets the CUDA Kokkos path and `nvcc_wrapper` host compiler |
| `cpuBuild` | builds a local Kokkos OpenMP install if needed, then builds `buildCPUHPC/saturnCFD` |
| `gpuBuild` | builds a local Kokkos CUDA (SM80) install if needed, then builds `buildGPUHPC/saturnCFD` |
| `generateCases.py` | writes the Heat YAML cases into `cases/` |
| `cpuStrong` | Slurm job: one CPU strong-scaling point (`mpirun -np $SLURM_NTASKS`) |
| `cpuWeak` | Slurm job: one CPU weak-scaling point |
| `cpuThreads` | Slurm job: one rank with 48 OpenMP threads |
| `gpuStrong` | Slurm job: one GPU strong-scaling point, one rank per GPU |
| `gpuWeak` | Slurm job: one GPU weak-scaling point |
| `submitCpu` | submits the CPU series one job at a time and waits |
| `submitGpu` | submits the GPU series one job at a time and waits |
| `collectResults.py` | merges the JSON runs into `results/scaling.csv` |
| `plotScaling.py` | draws speedup, efficiency, time/step and accuracy figures |
| `diagnose` | prints modules, tool versions, binaries, Kokkos, Slurm and recent logs |

## First checks on the login node

```bash
module load nvhpc-24.5/nvhpc-hpcx-cuda12/24.5
nvcc --version
cmake --version || echo "cmake missing"
python3 -c "import numpy, matplotlib; print(numpy.__version__, matplotlib.__version__)"
```

`module avail petsc`, `module avail kokkos` and `module avail cmake` are empty
on this cluster. Kokkos and CMake are therefore built/installed locally; PETSc
is not needed for Heat.

To confirm the GPU model inside an allocation:

```bash
srun --partition=sgpu --gres=gpu:1 --ntasks=1 nvidia-smi -L
```

The node manual says 2×A100 80 GB per GPU node.

## Build on the login node

The compute nodes have no internet access. Build on a login node, where
`git`/CMake can fetch yaml-cpp, doctest and Kokkos.

```bash
cd HPC/paramsakthi
./cpuBuild
./gpuBuild
```

- `cpuBuild` uses GCC 12.4 + OpenMPI 5.0.6 and a local Kokkos OpenMP build.
- `gpuBuild` uses the `nvhpc-24.5/nvhpc-hpcx-cuda12/24.5` module and a local
  Kokkos CUDA build with `Kokkos_ARCH_AMPERE80`.
- If `cmake` is missing, install it on the login node, for example:
  `python3 -m pip install --user 'cmake>=3.24'` or use the conda base
  environment if it already provides `cmake`.
- Run the builds from `HPC/paramsakthi`; they write `buildCPUHPC` and
  `buildGPUHPC` in the repository root.
- After a successful build, capture the login-node environment for the batch
  nodes:
  ```bash
  ./writeRuntime cpu
  ./writeRuntime gpu
  ```
  The jobs source `envCpuRuntime`/`envGpuRuntime` instead of calling Lmod.
  This is needed because the compute nodes do not initialize the login-node
  `/opt/ohpc` Lmod path. Regenerate the runtime files if you change modules.
- Kokkos is cloned into `~/.cache/saturnCFD` and installed under
  `~/.local/kokkos-cpu-4.7.04` / `~/.local/kokkos-cuda-4.7.04`.
- Binaries are `buildCPUHPC/saturnCFD` and `buildGPUHPC/saturnCFD` in the
  repository root.

If a module name differs, edit `envCpu`/`envGpu`; the module list on the login
node shows `compiler/gcc/12.4`, `compiler/openmpi/5.0.6` and
`nvhpc-24.5/nvhpc-hpcx-cuda12/24.5`.

## Cases

```bash
python3 generateCases.py
```

generates:

| file | mesh | time control | purpose |
|---|---|---|---|
| `heatStrong64.yaml` | 64³ | adaptive, 200 steps | one CPU core vs one GPU |
| `heatStrong96.yaml` | 96³ | adaptive | intermediate |
| `heatStrong128.yaml` | 128³ | adaptive, 200 steps | CPU/GPU node comparison |
| `heatStrong256.yaml` | 256³ | adaptive | dense GPU scaling |
| `heatWeak032.yaml` | 32³ | fixed 100 steps | 1 rank |
| `heatWeak064.yaml` | 64³ | fixed 100 steps | 8 ranks (32³/rank) |
| `heatWeak096.yaml` | 96³ | fixed 100 steps | 27 ranks |
| `heatWeak128.yaml` | 128³ | fixed 100 steps | 64 ranks |

The strong cases keep the physical end time and the timestep fixed, so every
rank count performs the same number of steps. The weak cases use a fixed
timestep and 100 steps, so work per rank is constant.

## Submit

```bash
./submitCpu     # small/medium strong + weak + one 48-thread run
./submitGpu     # 1 GPU on sgpu, 2/4/8 GPUs on gpu
```

### What the submit scripts do

`submitCpu` and `submitGpu` do **not** run the solver themselves. They:

1. check that the matching binary and case file exist;
2. submit one `sbatch` job with the right partition, nodes, tasks, GPUs and
   `CASE`;
3. wait until `squeue` no longer lists the job;
4. read the final state with `sacct` and stop immediately on
   `FAILED`/`CANCELLED`/`TIMEOUT`;
5. repeat for the next point.

This is why the script stays in the foreground: it waits for the first job,
then submits the second, and so on. Run it in `tmux`/`nohup`, not in a login
shell you intend to close.

Point list (`./submitCpu`):

| point | case | ranks × threads |
|---|---|---|
| smoke test | `heatStrong64` | 1 × 1 |
| one node, threads | `heatStrong128` | 1 × 48 |
| strong 128³ | `heatStrong128` | 1, 2, 4, 8, 16, 24, 48 ranks × 1 |
| strong 128³, multi-node | `heatStrong128` | 96 (2 nodes), 192 (4), 384 (8) |
| weak | `heatWeak032/064/096/128` | 1, 8, 27, 64 ranks |
| dense 256³ | `heatStrong256` | 48, 192, 384 ranks |

Point list (`./submitGpu`):

| point | case | GPUs |
|---|---|---|
| smoke test | `heatStrong64` | 1 |
| one A100 | `heatStrong128` | 1 |
| strong 128³ | `heatStrong128` | 2, 4, 8 |
| dense 256³ | `heatStrong256` | 8 |
| weak, 32³/GPU | `heatWeak032`, `heatWeak064` | 1, 8 |

The weak GPU series has only 1 and 8 GPUs because only those give an integer
cube with exactly 32³ cells per GPU; strong scaling carries the 2/4/8 GPU
curve.

Preview the chain without submitting anything:

```bash
DRY_RUN=1 ./submitCpu
DRY_RUN=1 ./submitGpu
```

A successful job prints `state=COMPLETED` and `log=logs/...`. A failed job
prints `state=FAILED`, the log path and its last 40 lines, then the chain stops.

Watch progress:

```bash
squeue -u "$USER"
tail -f logs/heatCpuStrong-*.out
ls -lt results/cpu results/gpu
```

If a job fails, the submit script stops; inspect the log named in the error
and run `./diagnose cpu` or `./diagnose gpu`.

`WAIT=0 ./submitCpu` queues all points without waiting, but then the queue
limits and cross-job timing noise apply. The default `WAIT=1` is the clean
mode.

The job scripts write YAML copies with a unique `output.prefix` under
`results/`. Each job stores its JSON and log.

Slurm limits: at most 3 running small jobs and 4 running medium jobs per user;
1 running GPU job. `submitCpu` and `submitGpu` submit one job at a time and
wait for it to finish, so the timings are clean and the queue limits are
respected. Run them inside `tmux` or `nohup`:

```bash
tmux new -s heatscale
cd HPC/paramsakthi
./submitCpu
# then, in another window/session:
./submitGpu
```

Set `WAIT=0` only if you want to queue everything at once.

## Collect and plot

```bash
python3 collectResults.py
python3 plotScaling.py
```

Outputs:

```text
results/scaling.csv
results/figures/cpuStrong.png / .pdf
results/figures/gpuStrong.png / .pdf
results/figures/cpuVsGpu.png / .pdf
results/figures/accuracy.png / .pdf
```

The plots show speedup, parallel efficiency, time per step, cells×steps/s and
the analytic error. The CSV keeps the raw measurements.

## Fairness notes

- CPU strong scaling uses 1 thread per MPI rank; the 48-thread run is a
  separate node-level comparison.
- GPU runs use one MPI rank per GPU, with 4 host cores per rank.
- CPU and GPU runs use the same YAML, mesh, end time and timestep.
- The GPU number is one A100; CPU numbers are one core, 48 cores (one node)
  and multi-node.
- The Heat solver is explicit, so time per step is the clean metric; total
  time includes I/O and startup.

## What is not in this campaign

- Stokes GPU scaling. `saturnStokes` is a PETSc DMDA solver and the GPU preset
  disables PETSc. A GPU Stokes benchmark needs a PETSc CUDA build and a
  GPU-capable preconditioner, or a matrix-free GPU Stokes solver.
- PETSc installation. Not needed for Heat.
