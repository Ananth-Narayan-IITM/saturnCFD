# 0017 — PETSc discovery and `saturnLinAlg`

- **Status:** accepted
- **Date:** 2026-10-31
- **Owner:** saturnCFD v1
- **Supersedes:** none
- **Protocol:** `docs/protocols/linearAlgebra.md`
- **Tests:** `tests/TestPetsc.cpp`, `tests/TestPetscMain.cpp`

## Context

PETSc 3.19.6 is installed through Debian (`petsc-dev`). It is a real-scalar
build using Open MPI. Debian does not install a CMake config in a location that
a plain `find_package(PETSc)` finds; `pkg-config --cflags --libs petsc` works.

`saturn_core` contains the mesh, configuration, communication and FVM layers.
Linking PETSc into it would force every CLI and mesh test to load PETSc and
would make the dependency compulsory for the front end.

The solver implementation sequence was agreed as PETSc linear algebra →
Burgers → heat → Stokes → Navier–Stokes.

## Decision

1. PETSc is found by `find_package(PETSc QUIET)` first, with a
   `pkg_check_modules(PETSC REQUIRED IMPORTED_TARGET petsc)` fallback.
2. PETSc is wrapped in a separate static library target **`saturnLinAlg`**
   (alias `saturn::LinAlg`). `saturn_core` does not link PETSc.
3. The first library contents are `PetscScope`, `petscCheck` and
   `KspSolver`; matrix assembly and Poisson systems follow.
4. `SATURN_ENABLE_PETSC=ON` is the default and is set in the CPU/default
   preset. `-DSATURN_ENABLE_PETSC=OFF` gives a minimal build. The GPU preset
   keeps PETSc off until a GPU PETSc build is configured.
5. PETSc is initialized after MPI and before Kokkos, and finalized in reverse
   order.
6. The first tests use PETSc `DMDA` only as a smoke test for PETSc/MPI/KSP.
   They are not the production finite-volume matrix assembler.

## Alternatives considered

| alternative | why not chosen |
|---|---|
| Link PETSc into `saturn_core` | forces PETSc on every CLI/mesh test and binary |
| Home-grown Krylov solver | duplicates a solved problem; violates ADR `0003` |
| `find_package(PETSc)` only | fails on Debian's packaging layout |
| Require `PETSC_DIR`/`PETSC_ARCH` manually | fragile on Debian; pkg-config is standard |
| Start with the FV matrix assembler | larger step before proving PETSc/MPI works |

## Consequences

- **Dependency:** PETSc is available in the default CPU build; the minimal
  build can disable it.
- **Code:** `saturnLinAlg` owns all PETSc calls; solvers link it.
- **Testing:** `saturn_petsc_tests` runs on 1/2/4 ranks; `ctest --preset
  petsc` is the entry point.
- **Risk:** Debian's PETSc and Open MPI must stay ABI-compatible. Record the
  PETSc version and configured precision in the run manifest.

## Validation / exit criteria

- `SATURN_ENABLE_PETSC=ON` configures through the pkg-config fallback.
- 1D Poisson with Dirichlet BCs is second order.
- 3D Poisson solves with CG + GAMG on 1/2/4 ranks.
- All-Neumann nullspace is attached and accepted by `MatNullSpaceTest`.
- `ctest --preset petsc` passes.

## References

- ADR `0003` — PETSc, not a home-grown Krylov solver.
- PETSc/TAO Users Manual.
- `docs/guides/petsc.md`, `docs/theory/poisson.md`.
