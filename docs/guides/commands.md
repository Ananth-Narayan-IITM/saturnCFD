# Command cheat sheet

**Purpose:** the commands you actually need. Nothing here is required for
understanding the code; it is a memory aid. `saturnCFD -help` remains the
authoritative CLI reference.

## First configure

```bash
cmake --preset default
```

This creates `build/` with tests and docs enabled.

## Build

```bash
cmake --build --preset default -j
```

Incremental builds are the same command.

## Test

```bash
ctest --preset default       # all tests, output on failure
ctest --preset regression    # regression only
ctest --preset unit
ctest --preset mesh
ctest --preset config
ctest --preset cli
```

List the available presets instead of memorising them:

```bash
ctest --list-presets
```

## Install the command

```bash
cmake --install build --prefix "$HOME/.local"
```

`~/.local/bin` must be on `PATH`. After that, use `saturnCFD`, not
`./build/saturnCFD`.

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
cmake --build build --target docHtml
cmake --build build --target docPdf
```

Outputs:

```text
build/doxygen/html/index.html
build/doxygen/latex/refman.pdf
```

The hosted site is built by the `docs` workflow.

## Format

```bash
cmake --build build --target formatCode
```

The CI check runs `clang-format --dry-run --Werror`.

## MPI and GPU

The MPI partition/halo layer is implemented. Run its tests with:

```bash
ctest --preset mpi
# equivalent:
mpirun --oversubscribe -np 1 build/saturn_mpi_tests
mpirun --oversubscribe -np 2 build/saturn_mpi_tests
mpirun --oversubscribe -np 4 build/saturn_mpi_tests
```

`--oversubscribe` is for a workstation with fewer slots than ranks. On the
cluster, use `srun`/`mpirun` with the allocated tasks and one rank per GPU.

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
