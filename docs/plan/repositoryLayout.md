# Repository layout

**Status:** implemented for the current slice.

This page fixes the folder structure. The goal is that a new contributor can
tell where a file belongs without reading the build.

## Tree

```text
saturnCFD/
  CMakeLists.txt
  CMakePresets.json             # configure/build/test presets
  requirements.toml             # host tools, versions, install packages
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
    simulation/                 # Solver, ScalarSolver, registry, TimeControl
    solvers/
      CMakeLists.txt            # addSolver(saturnBurger), addSolver(saturnHeat)
      saturnBurger/             # 1D/2D/3D scalar Burgers
        SaturnBurger.hpp        # core solver
        SaturnBurger.cpp
        BurgersCase.hpp         # YAML schema/factory/registration
        BurgersCase.cpp
        README.md
      saturnHeat/               # 1D/2D/3D scalar convection–diffusion
        SaturnHeat.hpp
        SaturnHeat.cpp
        HeatCase.hpp
        HeatCase.cpp
        README.md
      saturnStokes/             # planned
      saturnIncompressible/     # planned

  apps/
    main.cpp                    # saturnCFD driver

  benchmarks/
    README.md
    benchmarkBurgers.py         # CPU vs GPU wall-clock comparison

  examples/
    README.md
    saturnBurger/
      saturnBurger1D/           # standalone traveling-wave case
      saturnBurger2D/           # diagonal wave
      saturnBurger3D/           # diagonal wave
    saturnHeat/
      saturnHeat2D/             # periodic Fourier mode
      saturnHeat3D/             # periodic Fourier mode
    saturnStokes/               # later

  tests/
    TestMain.cpp                # doctest main
    data/meshes/                # fixture meshes
    TestFv.cpp                  # fields, operators, MMS
    TestLocalMesh.cpp           # local geometry + distribution
    TestSimulation.cpp          # time loop
    TestSaturnBurger*.cpp       # 1D/2D/3D Burgers
    TestSaturnHeat.cpp          # 3D Fourier + constant field
    TestPetsc.cpp               # Poisson/nullspace
    TestParallel.cpp            # partition/halo
    regression/
      mesh/
      config/
      burgers/
      burgers2d/
      burgers3d/
      heat2d/
        fourier64.yaml
      heat3d/
        fourier32.yaml
        restartFull.yaml
        restartCheckpoint.yaml
        restartTemplate.yaml.in
    checkBurgers.cmake           # CLI + analytic-norm regression
    checkBurgersVtk.cmake        # VTK frame + PVD regression
    checkBurgersCheckpoint.py    # checkpoint/restart round trip
    checkHeat.cmake              # heat CLI + analytic-norm regression
    checkHeatCheckpoint.py       # scalar checkpoint/restart round trip

  docs/
    Doxyfile
    index.md                    # Doxygen main page
    Design-v1.md
    Reference-v0.md
    doxygen/
      doxygenMathFilter.py      # Markdown $...$ -> Doxygen \f$...\f$
    logo/
      saturnCFD.svg             # project logo
    guides/
      commands.md               # CLI and CTest presets
      gpu.md                    # CPU/GPU builds and benchmarks
      petsc.md                  # PETSc discovery, saturnLinAlg, KSP/PC
    plan/
      solverSequence.md         # PETSc -> Burgers -> heat -> Stokes -> NS
      repositoryLayout.md       # this page
    theory/
      burgers.md
      heat.md
      poisson.md
    decisions/                  # ADRs
    protocols/                  # contracts
    validation/
      README.md                 # validation index
      burgers.md
      heat.md
      scripts/
        validateBurgers.py      # convergence table + validation figure
    references.bib

  tools/
    README.md
    checkRequirements.py        # pre-flight host check and --install
    compileSaturnCFD            # build/install CPU+GPU into ~/.local/saturnCFD
    saturnCFDSolver.cpp         # C++ scaffold -> saturnCFDSolver

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

## Where a new file belongs

- **Build/install/host check** → `tools/`. This folder only supports compiling
  and installing saturnCFD.
- **Solver numerics** → `src/solvers/saturn<SolverName>/`; the case layer
  (`<Name>Case.*`) stays beside the solver.
- **Shared finite-volume operators** → `src/fv/`.
- **Validation tables/figures** → `docs/validation/`; reusable generators in
  `docs/validation/scripts/`.
- **Performance measurements** → `benchmarks/`; these are not CTest cases.
- **Documentation tooling** → `docs/doxygen/`.

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
src/solvers/saturnBurger/
  CMakeLists.txt
  SaturnBurger.hpp         # options + class
  SaturnBurger.cpp         # geometry, BCs, RHS
  README.md                # equations, algorithm, validation
```

A user copies `src/solvers/saturnBurger` to `src/solvers/saturnMyBurger`,
renames the class/target, and adds one line to `src/solvers/CMakeLists.txt`:

```cmake
add_subdirectory(saturnMyBurger)
```

Each solver target is `saturn<SolverName>`. `saturnCFDSolver saturnBurger
saturnMyBurger` will perform the copy/rename when the scaffold tool lands; until
then the manual copy works.

## Tests

- `tests/regression/burgers/traveling64.yaml` — 1D case.
- `tests/regression/burgers/vtk.yaml` — VTK field export.
- `tests/regression/burgers2d/traveling64.yaml` — 2D case.
- `tests/regression/burgers3d/traveling16.yaml` — 3D case.
- `tests/regression/heat2d/fourier64.yaml` — 2D Fourier case.
- `tests/regression/heat3d/fourier32.yaml` — 3D Fourier case.
- `tests/regression/*/restart*.yaml` — checkpoint/restart.
- `tests/checkBurgers.cmake`, `tests/checkHeat.cmake` — run the CLI and check
  the analytic norms, CSV rows and JSON summary.
- MPI test targets set `OMP_NUM_THREADS=1` to avoid oversubscription.
- Unit tests live beside the category they test, not in one giant file.

CTest has two umbrella labels:

- `solver` — FV operators plus Burgers and heat numerics;
- `saturnCFD` — CLI, config, mesh, PETSc, VTK and checkpoint.

The fine-grained labels (`unit`, `mesh`, `config`, `cli`, `mpi`, `petsc`, `fv`,
`burgers`, `heat`, `vtk`, `checkpoint`, `gpu`) remain as metadata.

## Docs

- `docs/Doxyfile` is the single Doxygen configuration.
- `docs/index.md` is the main page.
- `docs/doxygen` holds the Doxygen Markdown filter.
- `docs/logo` holds the project logo.
- `docs/decisions` is the ADR log.
- `docs/protocols` is the contract layer.
- `docs/theory` is the equation and validation record.
- `docs/guides` is user-facing procedure.
- `docs/validation` is the measured results, with generators in
  `docs/validation/scripts`.
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
