# Phase 1 status and remaining work

**Status:** mesh, configuration and CLI foundation are implemented. The
MPI/Kokkos finite-volume layer, PETSc wrapper and solver authoring are next.
The parallel design is in [`mpiGpuDesign.md`](mpiGpuDesign.md).

## Done

| area | status |
|---|---|
| CMake build, presets, install target | done |
| `core/` types and SI-only units | done |
| yaml-cpp parsing and typed schema | done |
| SI-only validation; non-SI rejected | done |
| Gmsh 2.2 ASCII reader | done |
| structured-mesh detection and 1D/2D/3D rule | done |
| `checkMesh` text + JSON report | done |
| default `.vtu` + `.json` output | done |
| optional `-writeVTK` `.vtp`/`.pvd` | done |
| CLI help topics, completion, install | done |
| doctest suite and regression cases | done |
| Doxygen HTML/PDF and Pages workflow | done |
| clang-format target and CI check | done |

## Pending

| area | pending item |
|---|---|
| `src/comm` | MeshPartition, 26-neighbour halo, MPI+Kokkos runtime |
| `src/fv` | ghosted cell/face fields, gradient, divergence, Laplacian, interpolation, convective fluxes, boundary application |
| `src/simulation` | `Solver` interface, registry, time loop, diagnostics |
| `src/solvers` | `burgers` skeleton, `addSolver(burgers)`, `saturnCFDSolver` tool |
| `src/config` | `boundary` and `fields` schemas; solver-specific option schemas |
| `src/linalg` | MPI PETSc wrapper, matrix assembly, null-space handling |
| `src/io` | CSV diagnostics, run manifest, checkpoint |
| mesh | Gmsh 4.1 ASCII support (2.2 works now) |
| CLI | `-listSolvers`/`-dumpConfig burgers` are not meaningful until the solver skeleton lands |

Deferred deliberately: multi-GPU tuning, GPU-aware halo exchange, AMR,
two-phase flow, compressible flow.

## Recommended order

1. **Parallel decomposition and halo**
   - `MeshPartition` from `MPI_Dims_create`/`MPI_Cart_create`.
   - one owner per cell, two ghost layers, 26-neighbour exchange.
   - periodic local copies before remote exchange.
   - Kokkos pack/unpack; rank-to-device mapping.
   - tests on 1 and 2 ranks; analytic periodic field.

2. **Finite-volume layer**
   - ghosted `CellField`, `FaceField`, `VectorCellField`.
   - gradient, divergence, interpolation, non-orthogonal Laplacian.
   - upwind/Rusanov/MUSCL convection.
   - boundary-condition application.
   - tests: constant/linear fields, divergence theorem, manufactured Poisson,
     limiter behaviour.

3. **Solver lifecycle and skeleton**
   - `Solver` interface and registry.
   - `Simulation` time loop.
   - `src/solvers/burgers` skeleton with options, `createFields`, `equations`,
     diagnostics.
   - `addSolver(burgers)`.
   - `saturnCFDSolver burgers myBurgers` (C++ tool).
   - `-listSolvers` and `-dumpConfig burgers` become real.
   - `boundary`/`fields` YAML schema and validation.

4. **PETSc wrapper**
   - `LinearSolver` interface and MPI PETSc backend.
   - matrix assembly and null-space handling.
   - tests: 1D Poisson, Neumann zero-mean, multi-RHS.

5. **I/O completion**
   - CSV diagnostics, run manifest, checkpoint.
   - checkpoint/restart test.

Then Phase 2: Burgers physics and the `traveling64` analytic regression.

## Exit criteria for the next increment

- Halo exchange matches an analytic periodic field on 1 and 2 ranks.
- 1-rank and 2-rank FVM results agree; CPU and GPU agree within precision.
- `mpirun -np 1` GPU runs the halo/stencil benchmark.
- `tests/fv` passes on a structured hex mesh.
- A manufactured Poisson solve shows the expected order.
- `saturnCFD -listSolvers` lists `burgers`.
- `saturnCFD -dumpConfig burgers -writeTemplate burgers` produces a
  template that passes `-checkConfig`.
- `saturnCFDSolver burgers myBurgers` creates a buildable copy and prints
  `addSolver(myBurgers)`.
- Adding that one line to `src/solvers/CMakeLists.txt` and rebuilding makes
  `-listSolvers` show `myBurgers`.

## Commands

See [`../guides/commands.md`](../guides/commands.md). The short versions are:

```bash
cmake --preset default
cmake --build --preset default -j
ctest --preset regression
cmake --install build --prefix "$HOME/.local"
saturnCFD -help
```
