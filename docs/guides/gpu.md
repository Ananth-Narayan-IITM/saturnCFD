# CPU and GPU builds, threads and benchmarking

The case YAML is backend independent: the same `saturnBurger` case runs on the
CPU or the GPU. The backend is selected at configure time.

## The two builds

```text
buildCPU/    Kokkos OpenMP, PETSc on
buildGPU/    Kokkos CUDA, PETSc off
```

They cannot share a build directory because CMake fixes the compiler and
Kokkos backend at configure time.

## Building the GPU backend

The GPU build needs an installed **CUDA-enabled Kokkos** because the C++
compiler must be Kokkos' `nvcc_wrapper`. The OpenMP-only Kokkos install is not
enough. `tools/checkRequirements.py --install` detects an NVIDIA GPU, installs
`nvcc` from `nvidia-cuda-toolkit`, and compiles Kokkos into
`~/.local/kokkos-cuda` when it is missing. The manual build below is the
explicit alternative.

Example for a Quadro P620 (`PASCAL61`), using the Kokkos source already fetched
by the CPU build:

```bash
cmake -S buildCPU/_deps/kokkos-src -B /tmp/kokkos-cuda-build \
  -DCMAKE_INSTALL_PREFIX="$HOME/.local/kokkos-cuda" \
  -DCMAKE_BUILD_TYPE=Release \
  -DKokkos_ENABLE_CUDA=ON \
  -DKokkos_ENABLE_SERIAL=ON \
  -DKokkos_ENABLE_OPENMP=OFF \
  -DKokkos_ARCH_PASCAL61=ON \
  -DKokkos_ENABLE_CUDA_LAMBDA=ON \
  -DCMAKE_CXX_COMPILER="$PWD/buildCPU/_deps/kokkos-src/bin/nvcc_wrapper"

cmake --build /tmp/kokkos-cuda-build -j 8
cmake --install /tmp/kokkos-cuda-build

export KOKKOS_ROOT="$HOME/.local/kokkos-cuda"
cmake --preset gpu
cmake --build --preset gpu -j
```

Run the GPU-labelled tests with `ctest --preset gpu`. The solver YAML cases
also run through the GPU binary by filtering the GPU build directly:

```bash
ctest --preset gpu
ctest --test-dir buildGPU -L solver --output-on-failure
```

`KOKKOS_ROOT` is only needed when configuring/building the GPU preset; it is
not needed to run `saturnGPU`. You can put it in `~/.bashrc` if you
configure the GPU preset manually:

```bash
export KOKKOS_ROOT="$HOME/.local/kokkos-cuda"
```

The recommended installation helper does this for you and keeps both backends:

```bash
tools/compileSaturnCFD both        # CPU is the default saturnCFD
tools/compileSaturnCFD both --default gpu
```

It installs to `~/.local/saturnCFD/cpu` and `~/.local/saturnCFD/gpu` and creates
`~/.local/bin/saturnCPU` / `saturnGPU`; see
[`commands.md`](commands.md).

Use the architecture that matches the card (`Kokkos_ARCH_...`), for example
`AMPERE80`, `HOPPER90`, or `PASCAL61`.

## Running the same case

The recommended path-independent commands are created by installing the two
backends into separate prefixes and symlinking them:

```bash
saturnCPU /path/to/case.yaml
saturnGPU /path/to/case.yaml
```

See [`commands.md`](commands.md) for the install/symlink setup. The raw
build-tree commands also work:

```bash
# CPU
./buildCPU/saturnCFD examples/saturnBurger/saturnBurger2D/saturnBurger2D.yaml

# GPU
./buildGPU/saturnCFD examples/saturnBurger/saturnBurger2D/saturnBurger2D.yaml
```

Both write the same CSV/JSON, and the analytic comparison is identical. Each
run prints the active backend before the time loop:

```text
backend : CPU (Kokkos OpenMP, 24 threads)
backend : GPU (Kokkos CUDA, device 0: Quadro P620)
```

The same string is stored as `"backend"` in the JSON summary. The GPU
`saturnBurger` tests also pass:

```bash
./buildGPU/testSaturnFV --test-case="saturnBurger:*"
```

For one GPU, use one MPI rank:

```bash
mpirun -np 1 saturnGPU case.yaml
```

One GPU means **one MPI rank**:

```bash
mpirun -np 1 saturnGPU case.yaml
```

Two GPUs, one rank per GPU:

```bash
CUDA_VISIBLE_DEVICES=0,1 mpirun -np 2 saturnGPU case.yaml
```

Four GPUs:

```bash
CUDA_VISIBLE_DEVICES=0,1,2,3 mpirun -np 4 saturnGPU case.yaml
```

On SLURM, ask for one GPU per task:

```bash
srun --ntasks=2 --gpus-per-task=1 saturnGPU case.yaml
```

Each rank selects its GPU by local rank. Do **not** use `mpirun -np 4` on a
single GPU; that puts four ranks on one device and is not a multi-GPU run.
`mpirun` is not CPU-only; each rank's compiled Kokkos backend decides whether
it uses a CPU or a GPU.

## CPU/GPU benchmark

`benchmarks/benchmarkBurgers.py` generates one 2D case, runs it through both
binaries and reads `wallSeconds` from each JSON summary:

```bash
python3 benchmarks/benchmarkBurgers.py \
  --cpu saturnCPU \
  --gpu saturnGPU \
  --nx 256 --ny 256 --t-end 0.2
```

Measured on this machine (Quadro P620, 2 GB, 24-core CPU, CPU with 12 threads):

```text
grid       : 256 x 256
t_end      : 0.2 s
CPU threads: 12
CPU wall   : 33.75 s
GPU wall   : 16.74 s
speedup    : 2.02 x
```

The GPU is a low-power Quadro P620 with only 2 GB, and the display uses about
1.3 GB. A 512 x 512 run ran out of memory. Use 256 x 256 or 384 x 384 on this
card; larger GPUs can take denser meshes.

The current halo and Dirichlet boundary fill are host-staged, so the GPU
speedup is limited by copies between host and device. The device kernels
themselves are GPU-parallel; a device boundary/halo path is the next
optimization for bigger gains.

## What `OMP_NUM_THREADS` means

`OMP_NUM_THREADS` is the number of **CPU threads per process** for the Kokkos
OpenMP backend. It has nothing to do with the number of GPU threads.

Total CPU threads used is

```text
MPI ranks x OMP_NUM_THREADS
```

Keep that product at or below the number of physical cores. Four MPI ranks with
12 threads each on a 24-core node means 48 threads competing for 24 cores; that
is why `fv_np4` was slow.

### Explicit control

For a CPU run:

```bash
saturnCPU -threads 1 case.yaml       # one CPU thread
saturnCPU -threads 12 case.yaml      # twelve CPU threads
```

or use the environment variable:

```bash
OMP_NUM_THREADS=1 saturnCPU case.yaml
```

For MPI + OpenMP:

```bash
mpirun -np 4 saturnCPU -threads 6 case.yaml
# total = 4 ranks x 6 threads = 24 CPU threads
```

`-threads` and `OMP_NUM_THREADS` both mean "CPU threads per rank/process".
`-threads` is the CLI override; the environment variable is the standard one.

### Automatic behavior

For a **CPU** build:

- one rank and no `OMP_NUM_THREADS`: Kokkos chooses its default (often all
  available cores);
- more than one rank and no `OMP_NUM_THREADS`: `saturnCFD` sets
  `OMP_NUM_THREADS = max(1, cores/ranks)`;
- `-threads N` or an exported `OMP_NUM_THREADS=N` always wins.

For a **GPU** build, `OMP_NUM_THREADS` does not control the GPU. Use one MPI
rank per GPU and leave `OMP_NUM_THREADS` alone.

Rules of thumb:

| situation | starting point |
|---|---|
| pure MPI on CPU | `-threads 1`, one rank per physical core |
| hybrid MPI+OpenMP | `ranks * threads = physical cores` |
| small 1D cases | fewer threads often faster; kernel overhead dominates |
| dense 2D/3D cases | more threads per rank; benchmark |
| one GPU | one MPI rank; no `OMP_NUM_THREADS` tuning |
| several GPUs | one MPI rank per GPU |

Useful pinning:

```bash
export OMP_PROC_BIND=spread
export OMP_PLACES=threads
mpirun --bind-to core -np 4 saturnCPU -threads 1 case.yaml
```

The CTest MPI targets set `OMP_NUM_THREADS=1` so tests stay fast and
reproducible. An explicit user value always wins.
