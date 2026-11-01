# Command cheat sheet

**Purpose:** the commands you actually need. Nothing here is required for
understanding the code; it is a memory aid. `saturnCFD -help` remains the
authoritative CLI reference.

## Host requirements

```bash
python3 tools/checkRequirements.py
python3 tools/checkRequirements.py --install
python3 tools/checkRequirements.py --install --core-only
cmake --build buildCPU --target doctor
```

`requirements.toml` lists the tools and minimum versions. Missing optional
tools (docs/editor/GPU) do not fail the check; missing core tools do.
`--install` installs PETSc, installs `nvcc` only if an NVIDIA GPU is detected,
and compiles missing Kokkos into `~/.local/kokkos-cpu`/`kokkos-cuda`.
yaml-cpp and doctest are fetched by CMake. See
[`requirements.md`](requirements.md).

## First configure

```bash
cmake --preset default
```

This creates `buildCPU/` with tests and docs enabled.

## Build

```bash
cmake --build --preset default -j
```

Incremental builds are the same command.

### What compilation does

`cmake --preset default` configures `buildCPU/` and selects:

```text
SATURN_ENABLE_MPI=ON      link MPI, define SATURN_USE_MPI, build MPI tests
SATURN_ENABLE_KOKKOS=ON   fetch pinned Kokkos 4.7.04
SATURN_BACKEND=openmp     Kokkos execution backend for the CPU build
SATURN_ENABLE_PETSC=ON    link system PETSc through saturnLinAlg
SATURN_BUILD_TESTS=ON     build unit, MPI, FV and PETSc tests
SATURN_BUILD_DOCS=ON      enable docHtml/docPdf targets
CMAKE_BUILD_TYPE=Release  optimised build
```

`CMAKE_BUILD_TYPE=Release` means an optimised build (`-O3 -DNDEBUG` for
GCC/Clang) with no debug symbols. `Debug` is unoptimised with symbols;
`RelWithDebInfo` is optimised with symbols.

Configure and build are two separate commands: `cmake --preset default`
writes `buildCPU/`, and `cmake --build --preset default -j` compiles it.

`cmake --build --preset default -j` compiles:

```text
src/**            -> buildCPU/src/libsaturnCore.a
src/linalg/**     -> buildCPU/src/libsaturnLinAlg.a
apps/main.cpp     -> buildCPU/saturnCFD
tests/*           -> buildCPU/testSaturn
tests/TestMpi*    -> buildCPU/testSaturnMPI
tests/TestFv*     -> buildCPU/testSaturnFV
tests/TestLocalMesh* -> buildCPU/testSaturnFV
tests/TestPetsc*  -> buildCPU/testSaturnPETSc
```

MPI and Kokkos are linked into `saturnCore`. Kokkos is pinned by default;
`SATURN_USE_SYSTEM_KOKKOS=ON` plus `Kokkos_ROOT` uses an installed Kokkos.
PETSc is linked through the separate `saturnLinAlg` target. Debian PETSc is
found with `pkg-config` when `find_package(PETSc)` fails. Use
`-DSATURN_ENABLE_PETSC=OFF` for a minimal build.

Optional serial-only build (MPI off):

```bash
cmake -S . -B buildCPU-serial -DSATURN_ENABLE_MPI=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build buildCPU-serial -j
```

This is a diagnostic variant, not a daily tree. Remove it when finished so the
repository keeps only the two standard directories, `buildCPU` and
`buildGPU`.

Compilation database:

```text
buildCPU/compile_commands.json    # CMake/clangd standard
```

CPU/GPU builds are separate build directories because CMake fixes the compiler
and backend at configure time:

Use **one install root** with a subdirectory per backend. Rebuilding CPU does
not overwrite the GPU install, and vice versa:

```text
~/.local/saturnCFD/
  cpu/bin/saturnCFD
  gpu/bin/saturnCFD
```

The helper builds, installs and refreshes the command links:

```bash
# both backends; CPU is the plain "saturnCFD" default
tools/compileSaturnCFD both

# same, but make GPU the plain "saturnCFD" default
tools/compileSaturnCFD both --default gpu

# only one backend; the other install stays untouched
tools/compileSaturnCFD cpu
tools/compileSaturnCFD gpu

# remove buildCPU/buildGPU after a successful install
tools/compileSaturnCFD both --clean
```

The build directories are only needed for compiling and running CTest. Once a
backend is installed, `saturnCPU`/`saturnGPU` do not need them. Keeping them
makes the next incremental build fast; `--clean` removes them when you want a
minimal tree.

It creates:

```text
~/.local/bin/saturnCPU -> ~/.local/saturnCFD/cpu/bin/saturnCFD
~/.local/bin/saturnGPU -> ~/.local/saturnCFD/gpu/bin/saturnCFD
~/.local/bin/saturnCFD     -> CPU unless --default gpu
```

Make sure the command directory is on `PATH`:

```bash
echo 'export PATH="$HOME/.local/bin:$PATH"' >> "$HOME/.bashrc"
source "$HOME/.bashrc"
which saturnCPU
which saturnGPU
```

### KOKKOS_ROOT

The GPU **configure** step needs a CUDA-enabled Kokkos. The helper reads
`KOKKOS_CUDA_ROOT` (default `$HOME/.local/kokkos-cuda`) and exports
`KOKKOS_ROOT` only for that configure step. You do **not** need it at runtime.

If you configure the GPU preset manually, put this in `~/.bashrc`:

```bash
export KOKKOS_ROOT="$HOME/.local/kokkos-cuda"
```

It is ignored by the CPU preset. Build the CUDA Kokkos for the target
architecture (for example `PASCAL61` for the Quadro P620); see
[`gpu.md`](gpu.md).

### Manual equivalent

```bash
# CPU -> ~/.local/saturnCFD/cpu
cmake --preset cpu
cmake --build --preset cpu -j
cmake --install buildCPU --prefix "$HOME/.local/saturnCFD/cpu"

# GPU -> ~/.local/saturnCFD/gpu
export KOKKOS_ROOT="$HOME/.local/kokkos-cuda"
cmake --preset gpu
cmake --build --preset gpu -j
cmake --install buildGPU --prefix "$HOME/.local/saturnCFD/gpu"
```

```bash
mkdir -p "$HOME/.local/bin"
ln -sfn "$HOME/.local/saturnCFD/cpu/bin/saturnCFD" \
        "$HOME/.local/bin/saturnCPU"
ln -sfn "$HOME/.local/saturnCFD/gpu/bin/saturnCFD" \
        "$HOME/.local/bin/saturnGPU"
ln -sfn "$HOME/.local/saturnCFD/cpu/bin/saturnCFD" \
        "$HOME/.local/bin/saturnCFD"
```

## Call saturnCFD from any directory

Now the case can be anywhere and the backend is explicit:

```bash
saturnCPU /path/to/case.yaml
saturnGPU /path/to/case.yaml
saturnCFD     /path/to/case.yaml   # CPU unless installed with --default gpu
```

Each run prints the backend before the time loop:

```text
backend : CPU (Kokkos OpenMP, 24 threads)
backend : GPU (Kokkos CUDA, device 0: Quadro P620)
```

The installed binaries are independent of the build trees:

```bash
rm -rf buildCPU buildGPU
saturnCPU case.yaml      # still works
saturnGPU case.yaml      # still works
```

`which saturnCPU` / `which saturnGPU` show the symlinks. The
build-tree binaries (`buildCPU/saturnCFD`, `buildGPU/saturnCFD`) also work, but
they disappear when the build tree is cleaned, so they are not the recommended
command-line interface. If you really want them on `PATH`:

```bash
export PATH="$PWD/buildCPU:$PATH"   # temporary; buildCPU must exist
```

The same line is stored as `"backend"` in the JSON summary.

Then the run is always:

```bash
mpirun -np 4 saturnCFD case.yaml
mpirun -np 1 saturnCFD case.yaml
```

## Clean

```bash
cmake --build buildCPU --target clean   # remove compiled objects and binaries
rm -rf buildCPU                         # remove the CPU build tree
rm -rf buildCPU buildGPU               # both backend trees
```

There is no `wclean`; `cmake --build <dir> --target clean` is the per-build
equivalent. Deleting the directory is the full clean and forces a fresh
configure.

## Test

```bash
ctest --preset default       # everything, output on failure
ctest --preset cpu           # everything except GPU-labelled tests
ctest --preset solver        # FV operators, Burgers and heat, 1/2/4 ranks
ctest --preset saturnCFD     # CLI, config, mesh, PETSc, VTK, checkpoint
ctest --preset gpu           # GPU-labelled tests in buildGPU
```

The fine-grained labels still work directly, for example:

```bash
ctest --test-dir buildCPU -L burgers --output-on-failure
ctest --test-dir buildCPU -L saturnCFD --output-on-failure
```

The MPI test targets (`mpi_np*`, `fv_np*`, `petsc_np*`) set
`OMP_NUM_THREADS=1`. Without this, every rank asks Kokkos for 12 threads and
four ranks oversubscribe a 24-core node; `fv_np4` took about 370 s instead of
2-3 s. If you run a test binary by hand, use:

```bash
OMP_NUM_THREADS=1 mpirun -np 4 ./buildCPU/testSaturnFV
```

Direct FVM runs:

```bash
./buildCPU/testSaturnFV
mpirun -np 2 buildCPU/testSaturnFV
mpirun -np 4 buildCPU/testSaturnFV
./buildCPU/testSaturnFV --test-case="simulation:*"
./buildCPU/testSaturnFV --test-case="saturnBurger:*"
```

Direct PETSc runs:

```bash
./buildCPU/testSaturnPETSc
mpirun -np 2 buildCPU/testSaturnPETSc
mpirun -np 4 buildCPU/testSaturnPETSc
```

The MMS tests print the observed orders; a failure on any rank fails the run.
A CUDA build compiles the `gpu` cases and `ctest --preset gpu` runs them.
PETSc setup and KSP/PC choices are in [`petsc.md`](petsc.md).

List the available presets instead of memorising them:

```bash
ctest --list-presets
```

## Install only one backend

```bash
tools/compileSaturnCFD cpu
# or: tools/compileSaturnCFD gpu
```

That leaves the other backend install untouched. The plain `saturnCFD` is CPU
by default (pass `--default gpu` to change it).

```bash
ln -sfn "$HOME/.local/saturnCFD/cpu/bin/saturnCFD" \
        "$HOME/.local/bin/saturnCFD"
# or the gpu path
```

Then run from anywhere:

```bash
saturnCFD /path/to/case.yaml
saturnCPU /path/to/case.yaml
saturnGPU /path/to/case.yaml
```

## Run

```bash
saturnCFD -version
saturnCFD -listSolvers
saturnCFD -checkMesh tests/data/meshes/box2x2x2.msh
saturnCFD -checkMesh tests/data/meshes/box2x2x2.msh -report results/mesh
saturnCFD -checkMesh tests/data/meshes/box2x2x2.msh -writeVTK results/mesh
saturnCFD -dumpConfig common -writeTemplate common
saturnCFD -checkConfig common.yaml
```

`-writeTemplate common` writes `common.yaml`; `.yaml` is appended when the
argument has no extension.

Default `-checkMesh` output:

```text
results/<mesh-stem>.vtu
results/<mesh-stem>.json
```

`-writeVTK` adds `.vtp` patches and a `.pvd`.

### Solver field output

A solver case writes according to its `output` block:

```yaml
output:
  prefix: results/burgers/run
  csv: true        # final-time profile
  json: true       # norms and wall time
  exact: true      # analytic comparison where available
  vtkEvery: 10     # u field every 10 steps; 0 = off
```

With `vtkEvery: 10`:

```text
results/burgers/run_000000.vtu   # initial state
results/burgers/run_000010.vtu   # every 10 accepted steps
...
results/burgers/run_<final>.vtu  # final state
results/burgers/run.pvd          # ParaView collection
```

Open the `.pvd` in ParaView to animate the sequence. VTK field output is
currently available for `saturnBurger`.

### Checkpoint and restart

```yaml
output:
  checkpointEvery: 100        # 0 = off

restart:
  file: results/burgers/run_000500.chk
```

Checkpoints are written at step 0, every N steps and the final state. A restart
case resumes at the checkpoint step/time and continues to its `time.end`. The
solver name, dimension and mesh must match. `ctest --preset saturnCFD` verifies
that a restarted run matches an uninterrupted reference run.

## Help and completion

```bash
saturnCFD -help
saturnCFD -help checkMesh
saturnCFD -help units
saturnCFD -help config
saturnCFD -help completion
saturnCFD -help petsc
saturnCFD -help clangd

source <(saturnCFD -completion bash)     # bash
source <(saturnCFD -completion zsh)      # zsh
saturnCFD -completion fish | source      # fish
```

After completion is sourced:

```text
saturnCFD -dump<Tab>            -> -dumpConfig
saturnCFD -checkMesh tests<Tab> -> tests/
```

## Documentation

```bash
cmake --build buildCPU --target docHtml
cmake --build buildCPU --target docPdf
```

Outputs:

```text
buildCPU/doxygen/html/index.html
buildCPU/doxygen/latex/refman.pdf
```

The hosted site is built by the `docs` workflow. `TIMESTAMP = YES` in
`docs/Doxyfile` makes the HTML footer show the generation date.

## clangd

```bash
cmake --build buildCPU --target compileDb   # root symlink
clangd --compile-commands-dir=buildCPU
clangd --check=src/fv/CellField.hpp
```

The database is `buildCPU/compile_commands.json` (or
`buildGPU/compile_commands.json`). There is no `compileCommands.json`.
See `saturnCFD -help clangd`.

## Format

```bash
cmake --build buildCPU --target formatCode
```

The CI check runs `clang-format --dry-run --Werror`.

## MPI and GPU

The MPI partition/halo layer is implemented. Run its tests with:

```bash
ctest --preset default
# equivalent:
mpirun -np 1 buildCPU/testSaturnMPI
mpirun -np 2 buildCPU/testSaturnMPI
mpirun -np 4 buildCPU/testSaturnMPI
```

This PC has enough cores for the current 1/2/4-rank tests; no
`--oversubscribe` is needed. Use it only when the requested ranks exceed the
available slots. To make it the default for a test build:

```bash
cmake -S . -B buildCPU -DSATURN_MPI_PREFLAGS=--oversubscribe
```

On the cluster, use `srun`/`mpirun` with the allocated tasks and one rank per
GPU.

The 1D Burgers solver is registered. Run the YAML case directly:

```bash
./buildCPU/saturnCFD -listSolvers
./buildCPU/saturnCFD -dumpConfig saturnBurger
./buildCPU/saturnCFD -checkConfig tests/regression/burgers/traveling64.yaml
./buildCPU/saturnCFD tests/regression/burgers/traveling64.yaml
mpirun -np 4 ./buildCPU/saturnCFD tests/regression/burgers/traveling64.yaml
python3 docs/validation/scripts/validateBurgers.py     # table + validation figure

# standalone user examples + independent analytic comparison
cd examples/saturnBurger/saturnBurger1D
saturnCFD saturnBurger.yaml
python3 compareAnalytic.py

cd ../saturnBurger2D
saturnCFD saturnBurger2D.yaml
python3 compareAnalytic.py

cd ../saturnBurger3D
saturnCFD saturnBurger3D.yaml
python3 compareAnalytic.py
```

The same form runs any registered solver:

```bash
mpirun -np 4 saturnCPU -threads 1 case.yaml
mpirun -np 1 saturnGPU case.yaml      # one GPU, one rank
mpirun -np 2 saturnGPU case.yaml      # two GPUs, one rank each
```

With more than one CPU rank, the CLI sets
`OMP_NUM_THREADS = max(1, cores/ranks)` when it is not already set, so 4 ranks
on a 24-core node use 6 OpenMP threads each instead of oversubscribing.
Override it explicitly for hybrid runs:

```bash
mpirun -np 4 saturnCPU -threads 6 case.yaml
OMP_NUM_THREADS=1 mpirun -np 4 saturnCPU case.yaml
```

Every run prints its simulation wall-clock time and stores it as
`wallSeconds` in the JSON summary.

CPU and GPU backends are both built and verified; the single-GPU benchmark,
thread guidance and multi-GPU commands are in [`gpu.md`](gpu.md). The
decomposition plan is in
[`../plan/mpiGpuDesign.md`](../plan/mpiGpuDesign.md). The benchmark and
Valgrind/Compute Sanitizer commands are in [`profiling.md`](profiling.md) and
[`../plan/parallelTesting.md`](../plan/parallelTesting.md).

## What not to memorise

- Test labels: use `ctest --list-presets`.
- CLI options: use `saturnCFD -help` and `saturnCFD -help <topic>`.
- Output paths: `-checkMesh` prints them; the default is
  `results/<mesh-stem>`.
- Completion: source it once per shell.
