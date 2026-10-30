# linalg

PETSc is the linear-algebra backend. This module wraps it so solvers never call
PETSc directly.

## Responsibilities

- `LinearSolver` interface and `SolveReport`.
- PETSc `Mat`/`Vec`/`KSP`/`PC` wrappers.
- Matrix assembly from finite-volume coefficients.
- Vector momentum systems (block size 3 or multiple RHS).
- Pressure Poisson null space.

## Boundaries

- No PETSc calls in Kokkos kernels.
- No home-grown Krylov solver.
- A failed solve is an error in validation runs, not a warning.

## Tests

- `tests/linalg/` — 1D Poisson, Neumann null space, multi-RHS, PETSc error
  wrapper.

## Docs

- `docs/protocols/linearAlgebra.md`
- ADR `0003`
