# Command cheat sheet

**Purpose:** the commands you actually need. Nothing here is required for
understanding the code; it is a memory aid. `saturnCFD -help` remains the
authoritative CLI reference.

## First configure

```bash
cmake --preset default
```

This creates `build-cpu/` with tests and docs enabled.

## Build

```bash
cmake --build --preset default -j
```

Incremental builds are the same command.

### What compilation does

`cmake --preset default` configures `build-cpu/` and selects:

```text
SATURN_ENABLE_MPI=ON      link MPI, define SATURN_USE_MPI, build MPI tests
SATURN_BUILD_TESTS=ON     build saturn_tests and saturn_mpi_tests
SATURN_BUILD_DOCS=ON      enable docHtml/docPdf targets
CMAKE_BUILD_TYPE=Release  optimised build
```

`CMAKE_BUILD_TYPE=Release` means an optimised build (`-O3 -DNDEBUG` for
GCC/Clang) with no debug symbols. `Debug` is unoptimised with symbols;
`RelWithDebInfo` is optimised with symbols.

Configure and build are two separate commands: `cmake --preset default`
writes `build-cpu/`, and `cmake --build --preset default -j` compiles it.

`cmake --build --preset default -j` compiles:

```text
src/**            -> build-cpu/src/libsaturn_core.a
apps/main.cpp     -> build-cpu/saturnCFD
tests/*           -> build-cpu/saturn_tests
tests/TestMpi*    -> build-cpu/saturn_mpi_tests
```

MPI is linked into `saturn_core` and the MPI test binary. Kokkos and PETSc are
the next layers; they are not enabled in this mesh/config/CLI/comm slice.

Optional serial-only build (MPI off):

```bash
cmake -S . -B build-cpu-serial -DSATURN_ENABLE_MPI=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build-cpu-serial -j
```

This is a diagnostic variant, not a daily tree. Remove it when finished so the
repository keeps only the two standard directories, `build-cpu` and
`build-gpu`.

Compilation database:

```text
build-cpu/compile_commands.json    # CMake/clangd standard
```

CPU/GPU builds are separate build directories because CMake fixes the compiler
and backend at configure time. Once the Kokkos FVM layer lands, the intended
commands are:

```bash
cmake --preset cpu        # build-cpu, OpenMP
cmake --build --preset cpu -j
cmake --install build-cpu --prefix "$HOME/.local"

cmake --preset gpu        # build-gpu, CUDA
cmake --build --preset gpu -j
cmake --install build-gpu --prefix "$HOME/.local"
```

They do not coexist in one build directory. Install puts one `saturnCFD` on
`PATH`; the installed binary carries the backend it was compiled with. Install
the CPU or GPU build last to choose which one `saturnCFD` runs. Do not call
`build-cpu/saturnCFD` or `build-gpu/saturnCFD` directly.

Then the run is always:

```bash
mpirun -np 4 saturnCFD case.yaml
mpirun -np 1 saturnCFD case.yaml
```

## Clean

```bash
cmake --build build-cpu --target clean   # remove compiled objects and binaries
rm -rf build-cpu                         # remove the CPU build tree
rm -rf build-cpu build-gpu               # both backend trees
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
ctest --preset mpi
```

List the available presets instead of memorising them:

```bash
ctest --list-presets
```

## Install the command

```bash
cmake --install build-cpu --prefix "$HOME/.local"
```

`~/.local/bin` must be on `PATH`. After that, use `saturnCFD`, not
`./build-cpu/saturnCFD`.

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
cmake --build build-cpu --target docHtml
cmake --build build-cpu --target docPdf
```

Outputs:

```text
build-cpu/doxygen/html/index.html
build-cpu/doxygen/latex/refman.pdf
```

The hosted site is built by the `docs` workflow.

## Format

```bash
cmake --build build-cpu --target formatCode
```

The CI check runs `clang-format --dry-run --Werror`.

## MPI and GPU

The MPI partition/halo layer is implemented. Run its tests with:

```bash
ctest --preset mpi
# equivalent:
mpirun -np 1 build-cpu/saturn_mpi_tests
mpirun -np 2 build-cpu/saturn_mpi_tests
mpirun -np 4 build-cpu/saturn_mpi_tests
```

This PC has enough cores for the current 1/2/4-rank tests; no
`--oversubscribe` is needed. Use it only when the requested ranks exceed the
available slots. To make it the default for a test build:

```bash
cmake -S . -B build-cpu -DSATURN_MPI_PREFLAGS=--oversubscribe
```

On the cluster, use `srun`/`mpirun` with the allocated tasks and one rank per
GPU.

Solver runs are Phase 2+. The intended form is:

```bash
mpirun -np 4 saturnCFD case.yaml
mpirun -np 1 saturnCFD case.yaml      # one GPU, one rank
```

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
