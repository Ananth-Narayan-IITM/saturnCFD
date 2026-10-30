# saturnCFD v1 — design

**Status: decisions frozen for the substrate; physics planning in progress.**

This page is the short version. The binding records are in
[`docs/decisions/`](decisions/README.md); the equations are in
[`docs/theory/`](theory/README.md); the implementation sequence is in
[`docs/plan/v1-roadmap.md`](plan/v1-roadmap.md).

---

## 1. Mission

A single codebase that:

1. solves scalar transport (Burgers, heat) and incompressible flow (Stokes,
   Navier–Stokes) on a structured hexahedral mesh imported from Gmsh `.msh`;
2. is dimensionally consistent and validated against analytic solutions;
3. is driven entirely by YAML, with generated templates and strict validation;
4. is performance-portable through Kokkos, with CPU first and GPU later;
5. remains readable enough that one timestep can be followed from the driver to
   the last kernel.

## 2. Accepted architecture

| area | decision |
|---|---|
| mesh | structured hexahedral `.msh` only; 1D/2D are degenerate 3D meshes; `checkMesh` mandatory |
| state | collocated cell-centred velocity vector and dynamic pressure |
| coupling | Rhie–Chow face flux; SIMPLE for steady, PISO for transient |
| linear algebra | PETSc `Mat`/`Vec`/`KSP`/`PC`; no home-grown Krylov |
| solvers | separate modules for Burgers, heat, Stokes, incompressible |
| selection | `Solver` interface + registry; `solver:` in YAML |
| time loop | `Simulation` owns time, step, output, checkpoints and manifest |
| config | typed schema, generated `--dump-config`, strict `--check-config` |
| units | SI internally from the start; full units parser later |
| AMR | out of v1.0; revisit after incompressible NS is stable, for two-phase flow |

## 3. Why collocated

Basilisk's default incompressible solver is collocated/centred (with an
optional MAC module); Nektar++ and NekRS use collocated nodal points with a
lower-order pressure space; OpenFOAM is collocated FV with Rhie–Chow. A
staggered MAC grid gives exact discrete continuity and no checkerboard, but it
splits the velocity components onto different meshes and fights the vector
form, the `.msh` import and the 2D/3D rule. The checkerboard risk is handled
explicitly by Rhie–Chow, and the pressure–velocity coupling is validated before
Navier–Stokes.

## 4. Solver sequence

| stage | solver | main lesson | first validation |
|---|---|---|---|
| 1 | Burgers (scalar, then vector) | convection, shocks, limiters, diffusion, time integration | viscous traveling wave; inviscid shock |
| 2 | heat / convection–diffusion | scalar transport, Péclet effects, boundary layers | 1D boundary layer; 2D/3D MMS |
| 3 | Stokes | vector elliptic operator, pressure coupling, PETSc | Poiseuille, Couette, cavity at \(Re\to0\) |
| 4 | incompressible Navier–Stokes | nonlinear convection + pressure projection | Ghia cavity \(Re=100/1000\); Taylor–Green |

Compressible flow is out of v1.0. Two-phase flow and AMR are later.

## 5. Directory layout

```text
src/
  core/         types, errors, logging, timer, physical properties
  config/       YAML schema, parser, validation
  mesh/         Mesh, GmshReader, CheckMesh, StructuredMesh
  fv/           gradient, divergence, laplacian, interpolation,
                convection fluxes, Rhie–Chow, boundary conditions
  linalg/       PETSc wrappers, matrix assembly, linear-solver interface
  io/           VTU/PVD, CSV diagnostics, checkpoint, run manifest
  solvers/
    burgers/
    heat/
    stokes/
    incompressible/
  simulation/   time loop, solver registry, factory
apps/main.cpp
```

v0's `BoxArray`/`MultiFab` are not the v1 core: v1 is a structured
finite-volume mesh with cell/face fields, not a patch-based AMR container.

## 6. What is deliberately not in v1.0

- unstructured meshes;
- AMR, regridding, two-phase flow;
- compressible flow;
- GPU kernels (design for them, implement later);
- a full units library (SI is enforced by convention and documented);
- a home-grown Krylov solver.

## 7. Next actions

1. Accept or amend ADR `0008` (units).
2. Write the protocol specs for mesh, config, finite volume, linear algebra and
   solver lifecycle.
3. Write the theory pages (already drafted) and verify the references.
4. Implement `checkMesh` and the YAML schema/validation first, because every
   solver depends on them.
5. Implement Burgers and validate against the traveling wave before touching
   Stokes.
