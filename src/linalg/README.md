# linalg

**Library target:** `saturnLinAlg` (alias `saturn::LinAlg`).

PETSc is the linear-algebra backend. This module wraps it so solvers never call
PETSc directly.

## Implemented

- `PetscScope` — initializes PETSc once after MPI and finalizes it on scope
  exit.
- `petscCheck` / `SATURN_PETSC_CHECK` — converts a PETSc error code into a C++
  exception with the failing expression and source location.
- `KspSolver` — RAII `KSP`/`PC` wrapper with iteration count and residual
  norm.
- CMake discovery: `find_package(PETSc)` with a `pkg-config` fallback for
  Debian.
- `testSaturnPETSc` — 1D Poisson order, 3D Poisson with CG + GAMG, an
  all-Neumann nullspace test, and a coupled $u,p$ saddle-point block with
  `PCFIELDSPLIT`/Schur on 1/2/4 ranks.

The Phase 0 tests use PETSc `DMDA` only as a smoke test. The coupled block test
validates the fieldsplit/Schur setup; the production finite-volume matrix
assembler is the next slice.

## Next

- `LinearSystem` and matrix assembly from `LocalMesh` face coefficients.
- Poisson assembler with Dirichlet/Neumann rows and RHS.
- All-Neumann pressure nullspace.
- Block/multi-RHS momentum systems.
- Coupled `u,v,w,p` saddle-point matrix for `saturnStokes`.
- Apply the validated `PCFIELDSPLIT`/Schur path to the FV matrix:
  velocity block GAMG/ILU, pressure block GAMG/HYPRE, `MatNullSpace` for
  all-Neumann pressure.
- True residual and iteration diagnostics.

The coupled saddle-point design, matrix layout and PETSc options are described
in [`docs/theory/coupledSaddlePoint.md`](../../docs/theory/coupledSaddlePoint.md).

## Boundaries

- No PETSc calls in Kokkos kernels.
- No home-grown Krylov solver.
- A failed solve is an error in validation runs, not a warning.
- `saturnCore` does not link PETSc; only `saturnLinAlg` and solvers do.

## Tests

```bash
ctest --preset saturnCFD
```

The tests run under `mpirun -np 1/2/4`. See
[`docs/guides/petsc.md`](../../docs/guides/petsc.md) and
[`docs/theory/poisson.md`](../../docs/theory/poisson.md).

## Docs

- `docs/guides/petsc.md`
- `docs/theory/poisson.md`
- `docs/protocols/linearAlgebra.md`
- ADR `0003`, `0017`
