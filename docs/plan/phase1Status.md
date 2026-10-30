# Phase 1 status and remaining work

**Status:** mesh, configuration, CLI, the MPI partition/halo substrate and the
CPU finite-volume operator slice are implemented. PETSc wrapper, solver
authoring and the GPU execution verification are next. The parallel design is
in [`mpiGpuDesign.md`](mpiGpuDesign.md).

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
| `src/comm` MeshPartition, 26-neighbour halo | done |
| 1/2/4-rank analytic halo tests | done |
| `ctest --preset mpi` | done |
| Kokkos dependency, `cpu`/`gpu` presets | done |
| `src/fv` `CellField`, `FaceField`, `CartesianGeometry` | done |
| `LocalMesh` structured cell/face geometry | done |
| `FieldHalo` device pack / MPI / unpack | done |
| `fillBoundaryGhosts` Dirichlet/Neumann | done |
| `interpolate`, `gradient`, `divergence` | done |
| linear exactness and periodic gradient/divergence MMS | done on 1/2/4 ranks |
| orthogonal `laplacian` + periodic 2D/3D MMS | done, order 2 on 1/2/4 ranks |
| `ctest --preset fv` | done |
| GPU-labelled FV regression cases | written; CUDA build not yet verified locally |

## Pending

| area | pending item |
|---|---|
| `src/fv` | non-orthogonal Laplacian correction, convective fluxes, divergence theorem on non-uniform meshes, persistent face flux |
| `src/simulation` | `Solver` interface, registry, time loop, diagnostics |
| `src/solvers` | `burgers` skeleton, `addSolver(burgers)`, `saturnCFDSolver` tool |
| `src/config` | `boundary` and `fields` schemas; solver-specific option schemas |
| `src/linalg` | MPI PETSc wrapper, matrix assembly, null-space handling |
| `src/io` | CSV diagnostics, run manifest, checkpoint |
| `tests/parallel` | cross-run 1/2/4-rank equality, GPU execution on a CUDA host |
| `benchmarks` | halo/stencil strong/weak scaling harness |
| tooling | ASan/UBSan options, Valgrind/Compute Sanitizer targets |
| mesh | Gmsh 4.1 ASCII support (2.2 works now) |
| CLI | `-listSolvers`/`-dumpConfig burgers` are not meaningful until the solver skeleton lands |

Deferred deliberately: multi-GPU tuning, GPU-aware halo exchange, AMR,
two-phase flow, compressible flow.

## Recommended order

1. **Parallel decomposition and halo — host MPI slice done**
   - `MeshPartition` from `MPI_Dims_create`/`MPI_Cart_create`.
   - one owner per cell, two ghost layers, 26-neighbour exchange.
   - periodic local copies before remote exchange.
   - host-staged pack/unpack and `MPI_Isend`/`Irecv`.
   - tests on 1, 2 and 4 ranks; analytic periodic field.
   - MPI test main with global failure reduction.
   - Done: Kokkos `CellField` adapter and host-staged device halo.
   - Remaining: CUDA build on a machine with the toolkit, rank-to-device
     mapping, halo/stencil benchmark, ASan/UBSan and compute-sanitizer.

2. **Finite-volume layer — first slice done**
   - Done: Kokkos `CellField` with local ghost storage and components.
   - Done: `CartesianGeometry` device geometry (uniform block).
   - Done: `FieldHalo` device pack/unpack, local periodic copies.
   - Done: orthogonal `laplacian`; periodic 2D/3D MMS at order 2 on 1/2/4
     ranks.
   - Done: GPU-labelled regression cases (active CUDA space, 32^3 MMS against
     the CPU reference, halo roundoff).
   - Done: `LocalMesh` structured geometry, `FaceField`, `interpolate`,
     `gradient`, `divergence`, Dirichlet/Neumann ghost fill.
   - Done: linear exactness and periodic gradient/divergence MMS on 1/2/4
     ranks; 2D empty-direction check.
   - Next: non-orthogonal Laplacian correction, divergence theorem on a
     non-uniform mesh.
   - Next: persistent face flux for pressure; upwind/Rusanov/MUSCL
     convection; advection–diffusion MMS; limiter behaviour.
   - Remaining: CUDA execution on a host with the toolkit.
   - MMS methodology: [`../theory/manufacturedSolutions.md`](../theory/manufacturedSolutions.md).
   - plan: [`fvmKokkosPlan.md`](fvmKokkosPlan.md), ADR `0016`.

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

- **Done:** Kokkos halo matches an analytic periodic field on 1, 2 and 4
  ranks, including two ghost layers and all 26 directions.
- **Done:** periodic 2D/3D manufactured Laplacian shows order 2 on 1, 2 and 4
  ranks, and the 32^3 reference norm is stored.
- **Done:** `LocalMesh` interpolation/gradient/divergence are exact for a
  linear field with Dirichlet ghosts, and periodic gradient/divergence MMS is
  second-order on 1/2/4 ranks.
- **Done:** Neumann ghosts reproduce the exact linear gradient.
- 1-rank and 2-rank *cross-run* results agree; a CUDA build reproduces the CPU
  reference within precision.
- `mpirun -np 1` GPU runs the halo/stencil benchmark.
- `tests/fv` passes on a structured hex mesh with non-uniform geometry.
- Dirichlet and Neumann *Laplacian* MMS isolate the physical-boundary order.
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
cmake --install build-cpu --prefix "$HOME/.local"
saturnCFD -help
```
