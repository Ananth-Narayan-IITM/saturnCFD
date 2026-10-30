# 0003 — Linear algebra: PETSc, not a home-grown Krylov solver

- **Status:** accepted
- **Date:** 2026-10-30
- **Owner:** saturnCFD v1
- **Supersedes:** none
- **Protocol:** `docs/protocols/linearAlgebra.md`
- **Tests:** `tests/linalg/...`

## Context

The solvers need sparse linear systems for diffusion, pressure and the
vector momentum equation. Writing and debugging a Krylov solver, preconditioner
and MPI parallelisation is a separate project and would distract from the CFD
coursework. PETSc is a mature, well-documented library with Krylov methods,
preconditioners, sparse direct solvers, null-space handling and MPI support.

## Decision

**PETSc is the linear-algebra backend for v1.0.** No home-grown Krylov solver.

- Assemble sparse matrices from the finite-volume operators and solve with
  PETSc `KSP`/`PC`.
- Use `Mat`/`Vec` with `MPI_COMM_WORLD`; serial runs are the one-rank case.
- Use block size 3 for the vector momentum operator, or a multi-RHS solve, so
  the three velocity components share one sparsity pattern and one
  preconditioner setup.
- Use a scalar matrix for the pressure-correction Poisson equation.
- Recommended defaults: `GMRES`/`BiCGStab` with `ILU` for momentum; `CG` with
  `GAMG` (or `HYPRE` BoomerAMG) for pressure. Direct solvers (`MUMPS`,
  `SuperLU_DIST`) are acceptable for small validation meshes.
- Handle the Neumann pressure null space with `MatNullSpaceCreate` and
  `KSPSetNullSpace`, not by guessing a reference cell.
- Wrap PETSc calls in a `petscCheck` helper that converts an error code into a
  host-side exception with the PETSc message. No PETSc call is allowed inside a
  Kokkos kernel.
- PETSc version: pin a minimum (3.20 at the time of writing) and record the
  exact version in the run manifest.
- CPU double precision first. MPI and GPU backends are configuration choices
  behind the same `LinearSolver` protocol.

## Alternatives considered

| alternative | why not chosen |
|---|---|
| Eigen | excellent for serial prototypes, but no MPI/AMG story and more assembly work for large 3D |
| Home-grown CG/BiCGStab | educational but duplicates a solved problem; slows the CFD work |
| Trilinos | capable but heavier; PETSc is sufficient and more common in CFD |
| hypre directly | strong AMG, but no full Krylov/assembly layer; PETSc can call it |
| Ginkgo | GPU-first, but CPU/MPI coursework path is better served by PETSc |

## Consequences

- **Dependency:** PETSc must be installed or built; CMake needs
  `find_package(PETSc)` and `PETSC_DIR`/`PETSC_ARCH`.
- **Code:** a `LinearSolver` interface wrapping `KSP`; matrix assembly from FV
  operators; a `petscCheck` error wrapper.
- **Performance:** good defaults immediately; AMG for pressure; block/multi-RHS
  for the vector momentum solve.
- **Portability:** MPI comes almost for free; GPU acceleration depends on the
  PETSc build and is a later milestone.
- **Risk:** PETSc's `PetscInt` width and real precision must match the build;
  document the configured values.

## Validation / exit criteria

- A 1D Poisson problem solved with PETSc matches the analytic solution.
- The pressure-correction solve reports the true preconditioned residual.
- A Neumann problem with a constant right-hand side returns a zero-mean
  solution.
- The vector momentum solve with three right-hand sides matches three
  independent scalar solves.

## References

- Balay et al., *PETSc/TAO Users Manual*, Argonne National Laboratory.
- Rhie & Chow (1983); Issa (1986) for the pressure-correction systems.
