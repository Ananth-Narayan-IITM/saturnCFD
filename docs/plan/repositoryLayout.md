# Repository layout

**Status:** planning. No solver code is written yet.

This page fixes the folder structure before Phase 1 starts. The goal is that a
new contributor can tell where a file belongs without reading the build.

## Tree

```text
saturnCFD/
  CMakeLists.txt
  CMakePresets.json             # configure/buildCPU/test presets
  cmake/
    SaturnDependencies.cmake    # Kokkos, PETSc, yaml-cpp, doctest
    SaturnOptions.cmake         # backend, precision, warnings
    SaturnSolvers.cmake         # addSolver(name)
    SaturnDocs.cmake            # docHtml, docPdf, format

  src/
    core/                       # types, units, errors, logging, timer
    config/                     # yaml-cpp source, schema, binding, validation
    mesh/                       # Mesh, GmshReader, CheckMesh, StructuredMesh
    comm/                       # MeshPartition, HaloExchange, MPI runtime
    fv/                         # fields, gradient, divergence, laplacian,
                                # interpolation, fluxes, Rhie-Chow, BCs
    linalg/                     # saturnLinAlg: Petsc scope, KSP wrapper,
                                # Poisson matrix assembly
    io/                         # VTU/PVD, CSV, checkpoint, run manifest
    simulation/                 # Solver, registry, TimeControl, SSP-RK loop
    solvers/
      CMakeLists.txt            # addSolver(burgers), addSolver(heat), ...
      burgers/
      heat/
      stokes/
      incompressible/

  apps/
    main.cpp                    # saturnCFD driver
  benchmarks/
    haloStencil.cpp             # MPI halo + stencil benchmark
    diffusion.cpp               # FVM benchmark

  tools/
    saturnCFDSolver.cpp         # C++ scaffold -> saturnCFDSolver
    doxygenMathFilter.py        # Markdown $...$ -> Doxygen \f$...\f$

  tests/
    TestMain.cpp                # doctest main
    data/meshes/                # fixture meshes
    core/
    config/
    mesh/
    comm/
    fv/
    linalg/
    solvers/
    regression/
      mesh/
        box2x2x2.expected
        thin2d.expected
        tetRejected.expected
      config/
        siOnly.yaml
        cgsRejected.yaml
        unknownKey.yaml
      burgers/
        traveling64.yaml
        traveling64.expected

  docs/
    Doxyfile
    index.md                    # Doxygen main page
    Design-v1.md
    Reference-v0.md
    guides/
      petsc.md                  # PETSc discovery, saturnLinAlg, KSP/PC
    plan/
      solverSequence.md         # PETSc -> Burgers -> heat -> Stokes -> NS
    theory/
      poisson.md                # FV Poisson matrix and nullspace
    decisions/                  # ADRs
    protocols/                  # contracts
    theory/                     # equations and derivations
    guides/                     # writing a solver
    validation/                 # lab notebook
    plan/                       # this folder
    references.bib

  examples/
    burgers/
      traveling64.yaml
    heat/
    stokes/
    incompressible/

  external/                     # pinned dependencies or FetchContent cache
  .github/workflows/
    docs.yml                    # Doxygen HTML -> GitHub Pages, PDF artifact
    format.yml                  # clang-format check
  .clang-format
  .clang-tidy
  .gitignore
  README.md
  LICENSE
```

## Module READMEs

Every module directory has a `README.md` stating:

- its responsibilities;
- its boundaries (what it must not do);
- its tests;
- the ADR/protocol/theory pages it implements.

That keeps the first question — "where does this belong?" — answerable without
reading the build.

## Naming

- New case and test files use `camelCase`: `traveling64.yaml`,
  `burgersTemplate.yaml`, `meshReport.json`.
- Avoid hyphens and underscores in new case/test names.
- C++ functions `camelCase`; types `PascalCase`; members `mPascalCase`;
  constants `kPascalCase`.
- CMake helper functions are camelCase: `addSolver(forcedBurgers)`.
- Tool executables are one word or camelCase: `saturnCFD`, `saturnCFDSolver`.
- Standard names (`CMakeLists.txt`, `README.md`, `.clang-format`) and ADR
  number prefixes are exceptions.

## Solver directories

```text
src/solvers/burgers/
  CMakeLists.txt
  BurgersSolver.hpp
  BurgersSolver.cpp        # lifecycle + registration
  BurgersCase.hpp          # typed SI options
  BurgersOptions.cpp       # registerOptions()
  createFields.cpp         # state, initial conditions, BCs
  equations.cpp            # PDE/flux/source/time step
  diagnostics.cpp
  README.md
  cases/traveling64.yaml
```

A user copies `src/solvers/burgers` to `src/solvers/myBurgers` and adds one line
to `src/solvers/CMakeLists.txt`:

```cmake
addSolver(myBurgers)
```

`addSolver(name)` adds the directory, creates the static library and links it
into `saturnCFD`. `saturnCFDSolver burgers myBurgers` performs the copy, rename
and prints that line. No Bash script.

## Tests

- `tests/regression/burgers/traveling64.yaml` — case.
- `tests/regression/burgers/traveling64.expected` — expected norms, git hash and
  date.
- Unit tests live beside the category they test, not in one giant file.
- CTest labels: `[unit]`, `[mesh]`, `[config]`, `[mms]`, `[regression]`,
  `[parity]`, `[performance]`.

## Docs

- `docs/Doxyfile` is the single Doxygen configuration.
- `docs/index.md` is the main page.
- `docs/decisions` is the ADR log.
- `docs/protocols` is the contract layer.
- `docs/theory` is the equation and validation record.
- `docs/guides` is user-facing procedure.
- `docs/validation` is the measured results.
- `docs/plan` is planning, including this page.

## Dependencies

- Kokkos — CPU/GPU portability.
- MPI — decomposition and halo exchange.
- PETSc — MPI linear algebra.
- yaml-cpp 0.8.x — YAML parsing.
- doctest — tests.
- Doxygen + LaTeX — HTML and PDF docs.
- clang-format — formatting.

Dependency acquisition is pinned by `cmake/SaturnDependencies.cmake`. No
hand-written YAML parser and no Bash-only tooling.
