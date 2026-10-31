# PETSc setup and `saturnLinAlg`

**Status:** Phase 0 implemented. PETSc is linked, `saturnLinAlg` exists, and the
Poisson smoke tests pass on 1, 2 and 4 ranks.

## Installed version

The development machine has Debian's real-scalar PETSc:

```text
PETSc 3.19.6
/usr/lib/petscdir/petsc3.19/x86_64-linux-gnu-real
pkg-config --modversion petsc -> 3.19.6
```

PETSc is real-valued, not complex. It is built with Open MPI, matching the
project's MPI. The version is recorded at configure time.

## CMake discovery

`find_package(PETSc)` is tried first. Debian does not always install a CMake
config in a location CMake searches, so the build falls back to:

```cmake
pkg_check_modules(PETSC REQUIRED IMPORTED_TARGET petsc)
```

and links `PkgConfig::PETSC`. The configure output reports the origin:

```text
PETSc      : ON (pkg-config 3.19.6)
```

`SATURN_ENABLE_PETSC=OFF` gives a minimal mesh/config/CLI build without PETSc.
The CPU/default preset enables PETSc; the GPU preset keeps it off until a GPU
PETSc build is configured.

## Library

The target is **`saturnLinAlg`** (alias `saturn::LinAlg`). It is separate from
`saturnCore`:

```text
saturnCore      mesh, config, comm, FVM
saturnLinAlg     PETSc wrappers, matrix assembly, solvers
solvers          link both
```

This keeps the CLI and mesh/config tests independent of PETSc.

Current contents:

- `PetscScope` — initializes PETSc once and finalizes it on scope exit.
- `petscCheck` / `SATURN_PETSC_CHECK` — converts a PETSc error code into a
  C++ exception.
- `KspSolver` — RAII `KSP`/`PC` wrapper with iteration and residual reporting.

Planned contents:

- `LinearSystem` / matrix assembly from FV face coefficients.
- `Poisson` assembler with Dirichlet, Neumann and all-Neumann nullspace.
- Block/multi-RHS momentum systems.
- PETSc fieldsplit/Schur support for Stokes.

## Initialization order

```text
MPI_Init_thread
PetscInitialize        (PetscScope)
Kokkos::initialize
... run ...
Kokkos::finalize
PetscFinalize          (PetscScope destructor)
MPI_Finalize
```

PETSc must not be initialized inside a Kokkos kernel, and no PETSc call is
allowed in a kernel. The test main and future solver mains follow this order.

## Tests

```bash
ctest --preset petsc
```

The tests are `testSaturnPETSc`, run under `mpirun -np 1/2/4`:

- 1D Poisson with Dirichlet boundaries: observed order `1.98, 1.99`.
- 3D Poisson with CG + GAMG: L2 error about `2.1e-3` at `12^3`.
- All-Neumann 1D matrix: constant nullspace attached and tested with
  `MatNullSpaceTest`, CG + GAMG converges.

These use PETSc `DMDA` only as a linear-algebra smoke test. The production FV
matrix assembler is the next phase; it will use the same `KspSolver` and the
cell-local FVM coefficients.

## KSP/PC choices

| system | KSP | PC |
|---|---|---|
| diffusion / heat | CG | GAMG, fallback ICC/ILU on one rank |
| nonsymmetric convection-dominated | GMRES | ILU/AMG |
| pressure Poisson | CG | GAMG or HYPRE BoomerAMG |
| all-Neumann pressure | CG | GAMG + `MatNullSpace` |
| Stokes saddle point | GMRES | fieldsplit/Schur |

PETSc's parallel ILU is not available for all matrix types. On MPI AIJ,
`PCILU` can fail with “Could not locate a solver type for factorization type
ILU and matrix type mpiaij”. Use `bjacobi` (with local ILU), `gamg`, or
`jacobi` instead. The 1D smoke test uses CG + Jacobi for this reason.

## Pitfalls

- **Real vs complex:** this PETSc is real. Do not use complex-only KSP/PC
  paths.
- **MPI mismatch:** PETSc must use the same MPI as the project (Open MPI here).
- **`PetscInt` width:** use `PetscInt` for PETSc indices, not `int`, when
  interfacing.
- **Nullspace:** attach it to the matrix with `MatSetNullSpace`; do not pin a
  reference cell.
- **Residual reporting:** report the true preconditioned residual from PETSc,
  not a hand-computed estimate.
- **No hidden solver:** do not add a home-grown Krylov method.

## Next

1. FV Poisson matrix assembly from `LocalMesh` face coefficients.
2. Dirichlet/Neumann matrix rows and RHS, preserving symmetry.
3. Burgers/heat implicit diffusion through `saturnLinAlg`.
4. Stokes fieldsplit/Schur.
