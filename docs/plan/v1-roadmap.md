# v1 roadmap

**Status:** planning. No solver code is written yet.

The order is deliberately bottom-up: mesh and configuration first, then the
scalar solvers, then the elliptic saddle point, then incompressible flow. Each
phase ends with a validation gate; the next phase does not start until the gate
passes on CPU.

## Phase 0 — decisions and protocols

**Deliverables**

- ADRs `0001`–`0013` (see `docs/decisions/`).
- Protocol pages for CLI, units, mesh, finite volume, configuration, linear
  algebra, solver lifecycle, testing and documentation.
- Dependency decisions: Kokkos, PETSc, yaml-cpp 0.8, doctest, Doxygen/LaTeX.
- Folder layout (`docs/plan/repositoryLayout.md`) and documentation pipeline
  (`docs/plan/documentationPipeline.md`).
- Theory pages with equations, exact solutions and validation plans.

**Exit criterion**

- The substrate decisions are accepted, including SI units (`0008`).
- No open question blocks mesh, config or Burgers.

## Phase 1 — substrate

**Status:** mesh reader, `checkMesh`, SI-only validator, yaml-cpp schema, CLI,
MPI partition/halo, Kokkos `CellField`/`FaceField`, the device halo adapter,
`LocalMesh`, interpolation/gradient/divergence, non-orthogonal Laplacian,
Dirichlet/Neumann ghost fill, upwind/Rusanov convection, advection–diffusion
MMS, Doxygen pipeline and regression tests are in place. MUSCL/TVD, PETSc
wrapper and solver authoring remain. The detailed checklist is
[`phase1Status.md`](phase1Status.md).

**Deliverables**

1. **Mesh**
   - Gmsh 2.2 ASCII reader, then 4.1.
   - General `Mesh`, `StructuredMesh`, patches, geometry arrays.
   - `checkMesh`: structured detection, 2D rule, quality report, JSON output.
   - `-checkMesh` writes `.vtu` + `.json`; `-writeVTK` adds per-patch `.vtp`
     and `.pvd`.
   - Test meshes: uniform hex, stretched hex, one-cell-thick 2D, rejected
     tet/non-manifold/flipped.

2. **Configuration and units**
   - `ConfigSchema`, typed entries, defaults, units, validators.
   - yaml-cpp 0.8.x behind `ConfigSource`.
   - `src/core/Units.hpp`: dimension tags and `UnitSystem { SI }`.
   - `units.system: SI` or omitted; any non-SI input is a validation error.
   - `-dumpConfig <solver>`, `-writeTemplate <file>`, `-checkConfig`,
     `-listSolvers`, `-help`, `-completion`.
   - `cmake --install` gives the `saturnCFD` command.
   - Ten validation stages; BC and initial conditions in YAML.

3. **Parallel decomposition and halo**
   - `MeshPartition` from `MPI_Dims_create` / `MPI_Cart_create`.
   - One owner per cell; two ghost layers; 26-neighbour halo.
   - Local periodic copies before remote exchange; physical BCs after.
   - Kokkos fields — done (`CellField`, `FieldHalo`).
   - Rank-to-device mapping; one rank per GPU — pending the CUDA build.
   - Host-staged halo first; GPU-aware MPI behind the same interface.
   - Tests on 1, 2 and 4 ranks — CPU done; GPU written, not yet executed.

4. **Finite volume**
   - Kokkos `CellField` with ghosts — done.
   - `FieldHalo` device halo adapter — done.
   - `CartesianGeometry` + orthogonal Laplacian + periodic 2D/3D MMS — done.
   - `LocalMesh` structured geometry + `FaceField` — done.
   - Gradient, divergence, interpolation — done; linear exactness and
     periodic MMS pass on 1/2/4 ranks.
   - Dirichlet/Neumann boundary ghosts — done.
   - Non-orthogonal Laplacian correction — done.
   - Upwind and Rusanov convective flux — done.
   - Steady advection–diffusion MMS — done, first order.
   - MUSCL/TVD limiter and reconstruction — next.
   - All operators iterate owned cells only and read ghost values.
   - Plan: [`fvmKokkosPlan.md`](fvmKokkosPlan.md), ADR `0016`.

5. **Linear algebra**
   - PETSc wrapper: `Mat`, `Vec`, `KSP`, `PC`.
   - MPI `Mat`/`Vec` assembly; block-3 momentum; scalar pressure.
   - Null-space helper; residual diagnostics.
   - CPU PETSc first; GPU PETSc as an enabled build.

6. **I/O**
   - VTU/PVD for structured hex meshes; `-writeVTK` from `-checkMesh`.
   - CSV diagnostics in SI.
   - Run manifest (git hash, build, PETSc version, yaml-cpp version, mesh hash,
     case hash).
   - Checkpoint format (binary with metadata).

7. **Solver authoring**
   - `src/solvers/<name>/` layout; a user copy adds one
     `addSolver(<name>)` line to `src/solvers/CMakeLists.txt`.
   - `SATURN_REGISTER_SOLVER` and the solver API version.
   - `tools/saturnCFDSolver.cpp`, built as `saturnCFDSolver` (C++, no Bash).
   - A build test that copies `burgers` to `src/solvers/myBurgers`, adds the
     line, builds, lists it and generates its template.

8. **Documentation and formatting**
   - `docs/Doxyfile` HTML + LaTeX/PDF.
   - `tools/doxygenMathFilter.py` for `$...$` -> `\f$...\f$`.
   - `docs.yml` deploys HTML to GitHub Pages; PDF is a workflow artifact.
   - `.clang-format` and the `formatCode` CMake target; `format.yml` CI check.

9. **Parallel testing and profiling**
   - MPI test main with global failure reduction.
   - 1/2/4-rank halo, ownership and equality tests.
   - `benchmarks/haloStencil` strong/weak scaling.
   - ASan/UBSan CMake option; Valgrind and compute-sanitizer targets.
   - `docs/plan/parallelTesting.md` and `docs/guides/profiling.md`.

**Exit criterion**

- `checkMesh` passes on all accepted test meshes and rejects all bad ones.
- `-checkConfig` catches every category of error in the tests.
- yaml-cpp parse errors report line/column.
- Non-SI `units:` input is rejected; SI or absent is accepted.
- `-dumpConfig -writeTemplate` produces a template that passes `-checkConfig`.
- A copied `burgers` solver builds as `src/solvers/myBurgers` after one CMake
  line and appears in `-listSolvers`.
- `doxygen docs/Doxyfile` builds HTML; the LaTeX `make` produces `refman.pdf`.
- The Pages workflow deploys; `clang-format --dry-run --Werror` passes.
- Halo exchange matches an analytic periodic field on 1 and 2 ranks.
- 1-rank and 2-rank FVM results agree; CPU and GPU agree within precision.
- `mpirun -np 1` GPU runs the halo/stencil benchmark.
- A 1D Poisson problem and a manufactured diffusion problem solve through
  MPI PETSc to the expected order.

## Phase 2 — Burgers

**Deliverables**

- `src/solvers/burgers`: scalar first, vector second.
- Rusanov flux, MUSCL/TVD, SSP-RK2/3.
- Explicit diffusion; implicit option later.
- Exact traveling-wave validation and inviscid shock validation.
- `docs/validation/burgers.md` with tables and observations.

**Exit criterion**

- Traveling-wave error decreases under refinement; observed order close to the
  scheme order in smooth regions.
- Shock position and conservation are correct.
- Limiter comparison is documented.

## Phase 3 — heat

**Deliverables**

- `src/solvers/heat`: prescribed velocity, scalar transport.
- Steady 1D boundary layer, Gaussian pulse, 2D/3D manufactured solution.
- Central vs upwind vs TVD comparison.
- `docs/validation/heat.md`.

**Exit criterion**

- Boundary-layer profile matches the analytic solution.
- Manufactured solution shows the expected order.
- Cell-Péclet oscillation threshold is documented.

## Phase 4 — Stokes

**Deliverables**

- `src/solvers/stokes`: SIMPLE, vector momentum, pressure correction.
- Rhie–Chow face flux.
- Poiseuille, Couette, cavity at $Re\to0$, 2D/3D MMS.
- PETSc null-space handling for all-Neumann pressure.
- `docs/validation/stokes.md`.

**Exit criterion**

- Poiseuille and Couette match to machine precision on a uniform mesh.
- Cavity at $Re\to0$ matches the reference.
- Divergence norm is below tolerance; no checkerboard.

## Phase 5 — incompressible Navier–Stokes

**Deliverables**

- `src/solvers/incompressible`: PISO, backward Euler first, Crank–Nicolson
  later.
- Ghia cavity at $Re=100$, then $Re=1000$.
- Taylor–Green vortex.
- `docs/validation/incompressible.md`.

**Exit criterion**

- Cavity RMS and maximum deviation are stated and at least as good as v0.
- Divergence norm and pressure field are clean.
- Taylor–Green decay rate and convergence order are correct.

## Phase 6 — after v1.0

- Multi-GPU performance tuning and GPU-aware halo exchange.
- AMR, then two-phase flow.
- Full units parser.
- Unstructured mesh support, if needed.

## Validation matrix

| phase | analytic | manufactured | benchmark | conservation |
|---|---|---|---|---|
| mesh | — | — | — | — |
| Burgers | traveling wave | 2D/3D MMS | shock | $\int u$ |
| heat | boundary layer | 2D/3D MMS | Gaussian pulse | energy |
| Stokes | Poiseuille/Couette | 2D/3D MMS | cavity $Re\to0$ | mass |
| incompressible | — | — | Ghia, Taylor–Green | mass, energy |

## Risks

| risk | mitigation |
|---|---|
| `.msh` files are not truly structured | `checkMesh` classifier rejects them early |
| PETSc installation friction | document `PETSC_DIR`/`PETSC_ARCH`; provide a minimal build script |
| Rhie–Chow sign/interpolation error | derive once in `finiteVolume.md`; checkerboard regression test |
| pressure null space | PETSc `MatNullSpace`; mean-pressure diagnostic |
| validation drift | theory page + validation page + test for every solver |
| MPI halo bugs | 1-rank vs 2-rank and analytic periodic tests before any solver |
| one GPU is not an MPI+GPU scaling benchmark | report GPU single-rank separately; multi-GPU scaling deferred |
| PETSc GPU build friction | `LinearSolver` interface; host PETSc first, GPU backend enabled separately |
| scope creep (AMR, compressible) | explicitly out of v1.0; revisit only after Phase 5 |

## Definition of done per solver

- API documented and camelCase.
- Theory page with equations and citations.
- Unit + manufactured-solution tests.
- Validation case recorded with numbers.
- Observations written, including failures.
- `-dumpConfig` and `-checkConfig` work for its case files.
