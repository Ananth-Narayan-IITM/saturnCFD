# Phase 1 status and remaining work

**Status:** mesh, configuration, CLI, the MPI partition/halo substrate, the
CPU finite-volume operator slice and the Phase 0 PETSc linear-algebra layer are
implemented. The FV Poisson matrix assembler, Burgers/heat solvers and GPU
execution verification are next. The parallel design is in
[`mpiGpuDesign.md`](mpiGpuDesign.md); the solver sequence is in
[`solverSequence.md`](solverSequence.md).

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
| non-orthogonal `laplacian` on `LocalMesh` | done, exact for a sheared linear field |
| `faceNormalVelocity`, upwind/Rusanov/MUSCL convection | done |
| MUSCL limiter boundedness | done for minmod, van Leer, superbee |
| discrete divergence theorem on a sheared mesh | done |
| simplified `rhieChowFlux` | done, linear/checkerboard tests |
| linear exactness and periodic gradient/divergence MMS | done on 1/2/4 ranks |
| orthogonal `laplacian` + periodic 2D/3D MMS | done, order 2 on 1/2/4 ranks |
| upwind/MUSCL convection and advection–diffusion MMS | done |
| `ctest --preset fv` | done |
| PETSc 3.19.6 discovery (`pkg-config` fallback) | done |
| `saturnLinAlg`, `PetscScope`, `KspSolver` | done |
| 1D/3D Poisson and all-Neumann PETSc tests | done on 1/2/4 ranks |
| `ctest --preset petsc` | done |
| GPU-labelled FV regression cases | written; CUDA build not yet verified locally |

## Pending

| area | pending item |
|---|---|
| `src/fv` | pressure Poisson/SIMPLE-PISO coupling, persistent face flux, Rhie–Chow coefficient from the momentum diagonal |
| `src/simulation` | `Solver` interface, registry, time loop, diagnostics |
| `src/solvers` | `burgers` skeleton, `addSolver(burgers)`, `saturnCFDSolver` tool |
| `src/config` | `boundary` and `fields` schemas; solver-specific option schemas |
| `src/linalg` | FV Poisson matrix assembly, block/multi-RHS, fieldsplit/Schur |
| `src/io` | CSV diagnostics, run manifest, checkpoint |
| `tests/parallel` | cross-run 1/2/4-rank equality, GPU execution on a CUDA host |
| `benchmarks` | halo/stencil strong/weak scaling harness |
| tooling | ASan/UBSan options, Valgrind/Compute Sanitizer targets |
| mesh | Gmsh 4.1 ASCII support (2.2 works now) |
| CLI | `-listSolvers`/`-dumpConfig burgers` are not meaningful until the solver skeleton lands |

Deferred deliberately: multi-GPU tuning, GPU-aware halo exchange, AMR,
two-phase flow, compressible flow.

## Recommended order

The solver-specific sequence is in [`solverSequence.md`](solverSequence.md):
PETSc Phase 0 (done), solver lifecycle, Burgers 1D→2D→3D, heat, Stokes,
Navier–Stokes. The list below is the repository build-out order.

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
   - Done: non-orthogonal Laplacian correction; exact for a sheared linear
     field.
   - Done: `faceNormalVelocity`, upwind/Rusanov/MUSCL convection with
     minmod, van Leer and superbee.
   - Done: linear exactness, periodic gradient/divergence MMS, upwind/MUSCL
     convection, advection–diffusion MMS, TVD step boundedness and the
     divergence theorem on a sheared mesh.
   - Done: simplified Rhie–Chow flux; zero for a linear pressure, suppresses
     a checkerboard pressure.
   - Next: pressure Poisson/SIMPLE-PISO with PETSc.
   - Next: persistent face flux and the momentum-diagonal Rhie–Chow
     coefficient.
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

4. **PETSc wrapper — Phase 0 done**
   - `saturnLinAlg`, `PetscScope`, `KspSolver`.
   - `pkg-config` fallback for Debian PETSc.
   - done: 1D/3D Poisson, all-Neumann nullspace on 1/2/4 ranks.
   - next: FV Poisson matrix assembly from `LocalMesh`.
   - next: block/multi-RHS and fieldsplit/Schur.

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
- **Done:** non-orthogonal Laplacian is exact for a linear field on a sheared
  mesh.
- **Done:** upwind convection and steady advection–diffusion MMS are first
  order on 1/2/4 ranks.
- **Done:** MUSCL improves the L2 order to about 1.6 (minmod/van Leer) and
  keeps a periodic step bounded with all three limiters.
- **Done:** the discrete divergence theorem holds on a sheared mesh.
- **Done:** the Rhie–Chow correction vanishes for a linear pressure and
  suppresses a checkerboard pressure.
- **Done:** PETSc 3.19.6 links through `saturnLinAlg`; 1D/3D Poisson and
  all-Neumann tests pass on 1/2/4 ranks.
- Pending: FV Poisson matrix assembly and Burgers/heat solvers.
- Pending: pressure Poisson/SIMPLE-PISO with PETSc.
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
cmake --install buildCPU --prefix "$HOME/.local"
saturnCFD -help
```
