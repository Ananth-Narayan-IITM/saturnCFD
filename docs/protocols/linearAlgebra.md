# P7 — Linear algebra protocol (PETSc)

**Status:** accepted (ADR `0003`, `0017`). Phase 0 implemented: `saturnLinAlg`,
`PetscScope`, `petscCheck`, `KspSolver`, and PETSc Poisson/nullspace tests.
Finite-volume matrix assembly is next.

## Purpose

Wrap PETSc so the solvers see a small, stable interface and never call PETSc
directly from a kernel or a solver's physics code.

## Interface

```cpp
struct SolveReport {
  int iterations = 0;
  Real residual = 0;
  bool converged = false;
  double seconds = 0;
};

class LinearSolver {
public:
  virtual ~LinearSolver() = default;
  virtual SolveReport solve(const SparseMatrix& A,
                            const Vector& b,
                            Vector& x) = 0;
};
```

`SparseMatrix` is assembled from finite-volume coefficients and then handed to
PETSc as a `Mat` (AIJ/MPIAIJ). `Vector` wraps a `Vec`.

Phase 0 provides `saturn::KspSolver` as the first concrete wrapper. The
`LinearSolver` interface above is the target for the FV assembler. PETSc lives
in `saturnLinAlg`, not `saturn_core`.

Discovery note: Debian PETSc 3.19.6 is found through `pkg-config petsc` when
`find_package(PETSc)` fails. See [`../guides/petsc.md`](../guides/petsc.md).

## Vector form

- Momentum: one operator with block size 3, or one operator and three right-hand
  sides. The sparsity pattern is shared by $u,v,w$.
- Pressure: one scalar Poisson operator.
- The solver reports the preconditioned residual and the true residual.

## Recommended PETSc settings

| system | KSP | PC | notes |
|---|---|---|---|
| momentum (vector) | GMRES or BiCGStab | ILU / block Jacobi | one setup, three components |
| pressure Poisson | CG | GAMG or HYPRE BoomerAMG | symmetric positive (semi-)definite |
| small validation | preonly | LU / MUMPS / SuperLU | direct, for reference |

- Neumann pressure: create `MatNullSpace` with the constant vector and attach
  it with `KSPSetNullSpace`; do not guess a reference cell.
- Set tolerances explicitly; never rely on PETSc defaults in a validation run.
- Record `KSPGetIterationNumber`, `KSPGetResidualNorm`, reason and wall time in
  the diagnostics.

## Error handling

A `petscCheck(PetscErrorCode, const char* what)` helper converts any non-zero
code into a host-side exception carrying the PETSc message. PETSc calls are
host-only. A failed solve is an error, not a warning, in validation runs.

## Invariants

- Matrix rows and columns follow the cell ordering of the field.
- The pressure matrix has the correct null space for all-Neumann BCs.
- The same matrix can be reused across time steps when the coefficients do not
  change; factorisation/preconditioner reuse is explicit.
- No allocation inside the Krylov iteration.

## Tests

- 1D Poisson vs analytic;
- Neumann zero-mean solution;
- vector momentum with three RHS vs three scalar solves;
- PETSc unavailable -> clear configuration error;
- convergence failure -> non-zero exit with the PETSc reason.

## References

- Balay et al., PETSc/TAO Users Manual.
- PETSc `KSP`, `PC`, `MatNullSpace` documentation.
