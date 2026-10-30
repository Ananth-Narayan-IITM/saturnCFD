# saturnCFD v1

A rewrite of saturnCFD as a structured hexahedral finite-volume solver:
scalar transport (Burgers, heat), Stokes and incompressible Navier–Stokes,
driven by YAML and validated against analytic solutions.

**Status: planning.** v1.0 has accepted substrate decisions and theory pages;
no solver code is written yet.

The validated v0 implementation is preserved on `v0-archive` (tag `v0.3.0`)
and is a reference for validation data and algorithms, not a tree to copy.

## Decisions

- [ADR log](docs/decisions/README.md) — accepted and proposed decisions.
- [v1 roadmap](docs/plan/v1-roadmap.md) — phases, validation gates and risks.
- [Design summary](docs/Design-v1.md) — the short version.

Accepted so far:

- structured hexahedral Gmsh `.msh` only; 2D is a one-cell-thick 3D mesh;
- `checkMesh` validates topology, structuredness, dimension and quality;
- collocated cell-centred variables, Rhie–Chow, SIMPLE/PISO;
- PETSc for all linear algebra; no home-grown Krylov solver;
- separate solver modules for Burgers, heat, Stokes and incompressible flow;
- YAML schema generated from solver code, parsed with **yaml-cpp 0.8**;
- SI only in v1.0; non-SI YAML input is a validation error;
- single-dash CLI, `-help`, `-checkMesh`, `-writeVtk` mesh export,
  generated solver templates with `-dumpConfig -writeTemplate`;
- user-editable solvers: copy a solver directory, edit equations, rebuild;
- no AMR in v1.0; AMR/two-phase is a later milestone.

## Dependencies

- C++20 compiler, CMake >= 3.24
- Kokkos
- PETSc
- yaml-cpp 0.8.x
- doctest (test framework)

## Equations and validation

- [Theory index](docs/theory/README.md)
- [Finite-volume operators](docs/theory/finiteVolume.md)
- [Burgers](docs/theory/burgers.md)
- [Heat / convection–diffusion](docs/theory/heat.md)
- [Stokes](docs/theory/stokes.md)
- [Incompressible Navier–Stokes](docs/theory/incompressible.md)
- [Units](docs/theory/units.md)
- [References](docs/references.bib)

## Command line

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
collection and a JSON sidecar. Open the `.pvd` in ParaView. The full option
list is in [`docs/protocols/cli.md`](docs/protocols/cli.md).

## Planned layout

```text
src/
  core/         types, errors, logging, timer, units, physical properties
  config/       YAML schema, parser, validation
  mesh/         Mesh, GmshReader, CheckMesh, StructuredMesh
  fv/           gradient, divergence, laplacian, interpolation,
                convection fluxes, Rhie–Chow, boundary conditions
  linalg/       PETSc wrappers, matrix assembly
  io/           VTU/PVD, CSV diagnostics, checkpoint, manifest
  solvers/
    burgers/
    heat/
    stokes/
    incompressible/
  simulation/   time loop, solver registry
tools/
  newSolver.cpp C++ scaffold, built as saturnNewSolver
apps/main.cpp
```

## Writing a solver

```bash
saturnNewSolver burgers myBurgers
# add: add_subdirectory(myBurgers)   to src/solvers/CMakeLists.txt
# edit src/solvers/myBurgers/createFields.cpp and equations.cpp
cmake -S . -B build
cmake --build build -j
saturnCFD -listSolvers
saturnCFD -dumpConfig myBurgers -writeTemplate myBurgers.yaml
```

The solver directory is self-contained; the mesh, finite-volume, PETSc and I/O
layers are not edited. See
[`docs/guides/writingASolver.md`](docs/guides/writingASolver.md) and ADR
`0011`.

## Inspecting v0

```bash
git switch v0-archive
git worktree add ../saturnCFD-v0 v0-archive
```

## License

Apache-2.0. See [LICENSE](LICENSE).
