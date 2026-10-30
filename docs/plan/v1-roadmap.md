# v1 roadmap

**Status:** planning. No solver code is written yet.

The order is deliberately bottom-up: mesh and configuration first, then the
scalar solvers, then the elliptic saddle point, then incompressible flow. Each
phase ends with a validation gate; the next phase does not start until the gate
passes on CPU.

## Phase 0 — decisions and protocols

**Deliverables**

- ADRs `0001`–`0009` (see `docs/decisions/`).
- Protocol pages for mesh, finite volume, configuration, linear algebra,
  solver lifecycle and documentation.
- Theory pages with equations, exact solutions and validation plans.

**Exit criterion**

- The substrate decisions are accepted; the units decision (`0008`) is either
  accepted or explicitly deferred.
- No open question blocks mesh, config or Burgers.

## Phase 1 — substrate

**Deliverables**

1. **Mesh**
   - Gmsh 2.2 ASCII reader, then 4.1.
   - General `Mesh`, `StructuredMesh`, patches, geometry arrays.
   - `checkMesh`: structured detection, 2D rule, quality report, JSON output.
   - Test meshes: uniform hex, stretched hex, one-cell-thick 2D, rejected
     tet/non-manifold/flipped.

2. **Configuration**
   - `ConfigSchema`, typed entries, defaults, units, validators.
   - `--dump-config <solver>`, `--check-config`, `--list-solvers`.
   - Nine validation stages; BC and initial conditions in YAML.

3. **Finite volume**
   - `CellField`, `FaceField`, `VectorCellField`.
   - Gradient, divergence, interpolation, non-orthogonal Laplacian.
   - Convective flux: upwind, Rusanov, MUSCL + limiter.
   - Boundary-condition application.

4. **Linear algebra**
   - PETSc wrapper: `Mat`, `Vec`, `KSP`, `PC`.
   - Matrix assembly interface; block-3 momentum; scalar pressure.
   - Null-space helper; residual diagnostics.

5. **I/O**
   - VTU/PVD for structured hex meshes.
   - CSV diagnostics.
   - Run manifest (git hash, build, PETSc version, mesh hash, case hash).
   - Checkpoint format (binary with metadata).

**Exit criterion**

- `checkMesh` passes on all accepted test meshes and rejects all bad ones.
- `--check-config` catches every category of error in the tests.
- A 1D Poisson problem and a manufactured diffusion problem solve through
  PETSc to the expected order.

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
- Poiseuille, Couette, cavity at \(Re\to0\), 2D/3D MMS.
- PETSc null-space handling for all-Neumann pressure.
- `docs/validation/stokes.md`.

**Exit criterion**

- Poiseuille and Couette match to machine precision on a uniform mesh.
- Cavity at \(Re\to0\) matches the reference.
- Divergence norm is below tolerance; no checkerboard.

## Phase 5 — incompressible Navier–Stokes

**Deliverables**

- `src/solvers/incompressible`: PISO, backward Euler first, Crank–Nicolson
  later.
- Ghia cavity at \(Re=100\), then \(Re=1000\).
- Taylor–Green vortex.
- `docs/validation/incompressible.md`.

**Exit criterion**

- Cavity RMS and maximum deviation are stated and at least as good as v0.
- Divergence norm and pressure field are clean.
- Taylor–Green decay rate and convergence order are correct.

## Phase 6 — after v1.0

- MPI decomposition (PETSc already provides the linear-algebra path).
- GPU backend (Kokkos and a PETSc GPU build, or a specialised solver).
- AMR, then two-phase flow.
- Full units parser.
- Unstructured mesh support, if needed.

## Validation matrix

| phase | analytic | manufactured | benchmark | conservation |
|---|---|---|---|---|
| mesh | — | — | — | — |
| Burgers | traveling wave | 2D/3D MMS | shock | \(\int u\) |
| heat | boundary layer | 2D/3D MMS | Gaussian pulse | energy |
| Stokes | Poiseuille/Couette | 2D/3D MMS | cavity \(Re\to0\) | mass |
| incompressible | — | — | Ghia, Taylor–Green | mass, energy |

## Risks

| risk | mitigation |
|---|---|
| `.msh` files are not truly structured | `checkMesh` classifier rejects them early |
| PETSc installation friction | document `PETSC_DIR`/`PETSC_ARCH`; provide a minimal build script |
| Rhie–Chow sign/interpolation error | derive once in `finiteVolume.md`; checkerboard regression test |
| pressure null space | PETSc `MatNullSpace`; mean-pressure diagnostic |
| validation drift | theory page + validation page + test for every solver |
| scope creep (AMR, GPU, compressible) | explicitly out of v1.0; revisit only after Phase 5 |

## Definition of done per solver

- API documented and camelCase.
- Theory page with equations and citations.
- Unit + manufactured-solution tests.
- Validation case recorded with numbers.
- Observations written, including failures.
- `--dump-config` and `--check-config` work for its case files.
