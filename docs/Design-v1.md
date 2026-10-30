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
| config | typed schema, generated `-dumpConfig`, strict `-checkConfig` |
| units | SI only in v1.0; `units.system: SI` or omitted; non-SI is an error |
| CLI | single-dash camelCase options; `-help`, `-checkMesh`, `-writeVtk` |
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

## 4a. Command line and mesh visualisation

```bash
saturnCFD case.yaml
saturnCFD -checkMesh mesh.msh
saturnCFD -checkMesh mesh.msh -report report.json
saturnCFD -checkMesh mesh.msh -writeVtk output/mesh
saturnCFD -checkConfig case.yaml
saturnCFD -dumpConfig burgers
saturnCFD -dumpConfig burgers -writeTemplate burgers.yaml
saturnCFD -help
saturnCFD -help checkMesh
```

`-writeVtk` writes a volume `.vtu`, one boundary `.vtp` per patch, a `.pvd`
collection and a JSON sidecar; open the `.pvd` in ParaView. The full option
list is in `docs/protocols/cli.md`.

## 5. Directory layout

```text
src/
  core/         types, errors, logging, timer, units, physical properties
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
- non-SI unit systems (v1.0 accepts SI only; other systems are rejected);
- a home-grown Krylov solver.

## 7. Next actions

1. Write the remaining protocol specs P1 (core), P8 (I/O) and P9 (verification).
2. Verify the references and equation signs in the theory pages.
3. Implement `checkMesh`, the YAML schema, the units binder and `-help` first,
   because every solver depends on them.
4. Implement Burgers and validate against the traveling wave before touching
   Stokes.
5. Add mesh VTK export early so every validation case can be inspected in
   ParaView.
