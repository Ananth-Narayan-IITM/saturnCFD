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
tools (docs/editor/GPU) do not fail the check; missing core tools do. Kokkos,
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

```bash
cmake --preset cpu        # buildCPU, Kokkos OpenMP
cmake --build --preset cpu -j
cmake --install buildCPU --prefix "$HOME/.local"

export KOKKOS_ROOT=/path/to/cuda-kokkos
cmake --preset gpu        # buildGPU, CUDA, system Kokkos + nvcc_wrapper
cmake --build --preset gpu -j
cmake --install buildGPU --prefix "$HOME/.local"
```

The GPU preset needs an installed CUDA-enabled Kokkos because the compiler must
be Kokkos' `nvcc_wrapper` before the project is configured. Build that Kokkos
for the target architecture (for example `PASCAL61` for the Quadro P620).

They do not coexist in one build directory. Install puts one `saturnCFD` on
`PATH`; the installed binary carries the backend it was compiled with. Install
the CPU or GPU build last to choose which one `saturnCFD` runs. Do not call
`buildCPU/saturnCFD` or `buildGPU/saturnCFD` directly.

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
ctest --preset default       # all tests, output on failure
ctest --preset regression    # regression only
ctest --preset unit
ctest --preset mesh
ctest --preset config
ctest --preset cli
ctest --preset mpi           # partition/halo + FVM, 1/2/4 ranks
ctest --preset fv            # fields, LocalMesh, operators, MMS, Burgers
ctest --preset burgers       # YAML Burgers validation on 1/2/4 ranks
ctest --preset petsc         # PETSc Poisson/nullspace on 1/2/4 ranks
ctest --preset gpu           # GPU-labelled tests in buildGPU
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

## Install the command

```bash
cmake --install buildCPU --prefix "$HOME/.local"
```

`~/.local/bin` must be on `PATH`. After that, use `saturnCFD`, not
`./buildCPU/saturnCFD`.

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
ctest --preset mpi
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
python3 tools/validateBurgers.py     # table + validation figure

# standalone user example + independent analytic comparison
cd examples/saturnBurger
saturnCFD saturnBurger.yaml
python3 compareAnalytic.py
```

The same form runs any registered solver:

```bash
mpirun -np 4 saturnCFD case.yaml
mpirun -np 1 saturnCFD case.yaml      # one GPU, one rank
```

With MPI, the CLI sets `OMP_NUM_THREADS = max(1, cores/ranks)` when it is not
already set, so 4 ranks on a 24-core node use 6 OpenMP threads each instead of
oversubscribing. Override it explicitly for hybrid runs:

```bash
OMP_NUM_THREADS=1 mpirun -np 4 saturnCFD case.yaml
```

Every run prints its simulation wall-clock time and stores it as
`wallSeconds` in the JSON summary.

Kokkos/GPU execution and the one-GPU benchmark land with the FVM layer. The
decomposition, ghost-cell and benchmark plan is in
[`../plan/mpiGpuDesign.md`](../plan/mpiGpuDesign.md). The benchmark and
Valgrind/Compute Sanitizer commands are in [`profiling.md`](profiling.md) and
[`../plan/parallelTesting.md`](../plan/parallelTesting.md).

## What not to memorise

- Test labels: use `ctest --list-presets`.
- CLI options: use `saturnCFD -help` and `saturnCFD -help <topic>`.
- Output paths: `-checkMesh` prints them; the default is
  `results/<mesh-stem>`.
- Completion: source it once per shell.
