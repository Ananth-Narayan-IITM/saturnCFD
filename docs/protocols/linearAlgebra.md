# P7 — Linear algebra protocol (PETSc)

**Status:** accepted (ADR `0003`, `0017`). Phase 0 implemented: `saturnLinAlg`,
`PetscScope`, `petscCheck`, `KspSolver`, and PETSc Poisson/nullspace tests.
The finite-volume assembler and the coupled saddle-point/fieldsplit path are the
next implementation slice; the design is fixed in
[`../theory/coupledSaddlePoint.md`](../theory/coupledSaddlePoint.md).

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
in `saturnLinAlg`, not `saturnCore`.

Discovery note: Debian PETSc 3.19.6 is found through `pkg-config petsc` when
`find_package(PETSc)` fails. See [`../guides/petsc.md`](../guides/petsc.md).

## Vector form

- Momentum: one operator with block size 3, or one operator and three right-hand
  sides. The sparsity pattern is shared by $u,v,w$.
- Pressure: one scalar Poisson operator.
- The solver reports the preconditioned residual and the true residual.

## Coupled saddle-point systems

For Stokes (and the pressure–velocity block of Navier–Stokes) the matrix is

$$
\left[\begin{array}{cc}
A & B^{T}\\
B & 0
\end{array}\right]
\left[\begin{array}{cc}
\mathbf{u}\\ p
\end{array}\right]
=
\left[\begin{array}{cc}
\mathbf{f}\\ \mathbf{g}
\end{array}\right].
$$

- Assemble one `MPIAIJ` matrix with `u,v,w,p` degrees of freedom per cell
  (`u,v,p` in 2D), using a local-to-global map that covers owned cells and one
  ghost layer.
- Use GMRES/FGMRES on the coupled system.
- Precondition with `PCFIELDSPLIT` and a Schur-complement factorization:
  approximate velocity solve with $A$, approximate Schur solve with
  $S=-BA^{-1}B^{T}$, then back-substitute the pressure coupling.
- The exact $A^{-1}$ and $S^{-1}$ are never formed; AMG/ILU supplies the
  approximate actions. GMRES handles the remaining coupling.
- The current Stokes validation core uses `preonly` + `lu` (coupled direct);
  the fieldsplit/Schur configuration is the production target and is being
  tuned for refined meshes.
- Reuse the matrix and preconditioner while the coefficients are fixed; rebuild
  or update them for each Navier–Stokes Picard/Newton iteration.
- All-Neumann pressure: attach `MatNullSpace` with the constant vector and use
  `KSPSetNullSpace`; report the mean pressure separately.
- Rhie–Chow stabilisation in the continuity block is required for the
  collocated grid; the matrix is a stabilised saddle-point system, not the raw
  central-difference block.

The derivation and options are in
[`../theory/coupledSaddlePoint.md`](../theory/coupledSaddlePoint.md).

## Recommended PETSc settings

| system | KSP | PC | notes |
|---|---|---|---|
| momentum (vector) | GMRES or BiCGStab | ILU / block Jacobi | one setup, three components |
| pressure Poisson | CG | GAMG or HYPRE BoomerAMG | symmetric positive (semi-)definite |
| coupled Stokes/NS | GMRES/FGMRES | fieldsplit/Schur | velocity GAMG/ILU, pressure GAMG/HYPRE |
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
- coupled saddle-point block with fieldsplit/Schur on 1/2/4 ranks;
- pressure nullspace and mean-pressure diagnostics;
- PETSc unavailable -> clear configuration error;
- convergence failure -> non-zero exit with the PETSc reason.

## References

- Balay et al., PETSc/TAO Users Manual.
- PETSc `KSP`, `PC`, `MatNullSpace`, `PCFIELDSPLIT` documentation.
- Benzi, Golub & Liesen (2005).
- Elman, Silvester & Wathen (2014).
- Kay, Loghin & Wathen (2002).
- Elman et al. (2006).
