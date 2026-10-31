# saturnCFD v1

A rewrite of saturnCFD as a structured hexahedral finite-volume solver:
scalar transport (Burgers, heat), Stokes and incompressible Navier–Stokes,
driven by YAML and validated against analytic solutions.

**Status: Phase 1 in progress.** Mesh reading, `checkMesh`, the SI-only
validator, yaml-cpp schema validation, CLI help, VTK mesh export, the MPI
partition/halo substrate, Kokkos `CellField`/`FaceField`, the device halo
adapter, structured `LocalMesh`, interpolation/gradient/divergence,
non-orthogonal Laplacian, Dirichlet/Neumann ghost fill, upwind/Rusanov/MUSCL
convection, advection–diffusion and divergence-theorem tests, a simplified
Rhie–Chow flux, and the explicit solver/time loop (forward Euler, SSPRK2/3,
diagnostics, registry) are in place. PETSc 3.19.6 is linked through the
separate `saturnLinAlg` library; the 1D/3D Poisson and all-Neumann tests pass
on 1/2/4 ranks. The FV Poisson matrix assembler, implicit time schemes,
Burgers and heat solvers are next. Solver physics starts in Phase 2.

MPI and GPU are **first-class from the first FVM layer**, not a later port.
The decomposition, ghost-cell and one-GPU benchmark plan is in
[`docs/plan/mpiGpuDesign.md`](docs/plan/mpiGpuDesign.md).

The validated v0 implementation is preserved on `v0-archive` (tag `v0.3.0`)
and is a reference for validation data and algorithms, not a tree to copy.

## Host requirements

Check the host before a clean build:

```bash
python3 tools/checkRequirements.py
python3 tools/checkRequirements.py --install   # apt packages when missing
cmake --build buildCPU --target doctor         # same check through CMake
```

`requirements.toml` lists core, docs, editor and GPU tools with minimum
versions. Kokkos, yaml-cpp and doctest are fetched and compiled by CMake.
Details: [`docs/guides/requirements.md`](docs/guides/requirements.md).

## Build and test

```bash
cmake --preset default
cmake --build --preset default -j
ctest --preset regression          # the common case
ctest --preset mpi                 # 1/2/4-rank halo and FVM tests
ctest --preset fv                  # fields, halo, LocalMesh, operators, MMS
ctest --preset petsc               # PETSc Poisson/nullspace on 1/2/4 ranks
ctest --preset default             # all tests, output on failure
```

Equivalent without presets:

```bash
cmake -S . -B buildCPU -DCMAKE_BUILD_TYPE=Release
cmake --build buildCPU -j
ctest --test-dir buildCPU --output-on-failure
```

Install the binary so the command is simply `saturnCFD`:

```bash
cmake --install buildCPU --prefix "$HOME/.local"
# ~/.local/bin must be on PATH
```

yaml-cpp, doctest and Kokkos are fetched automatically when not installed.
Kokkos is pinned to 4.7.04 and uses `SATURN_BACKEND=openmp` by default.
`SATURN_ENABLE_MPI=ON` is the default: it links MPI, defines
`SATURN_USE_MPI`, and builds the MPI partition/halo and FVM tests. The app is
still a serial mesh/config front end; the FVM operators live in
`testSaturnFV`. Use `-DSATURN_ENABLE_MPI=OFF` for a serial build.

PETSc 3.19.6 is enabled in the CPU/default preset and linked through
`saturnLinAlg`. Debian's PETSc is found through `pkg-config` when
`find_package(PETSc)` fails. Use `-DSATURN_ENABLE_PETSC=OFF` for a minimal
mesh/config build. Setup and solver choices are in
[`docs/guides/petsc.md`](docs/guides/petsc.md).

Library targets are uniformly camelCase:

```text
saturnCore         mesh, config, comm, FVM
saturnLinAlg       PETSc wrappers and matrix assembly
saturnSimulation   Solver, registry, TimeControl, explicit time loop
```

For clangd, the standard database is `buildCPU/compile_commands.json` (or
`buildGPU/compile_commands.json`). Link it at the repository root with:

```bash
cmake --build buildCPU --target compileDb
```

See `saturnCFD -help clangd` and
[`docs/guides/commands.md`](docs/guides/commands.md).

CPU and GPU are separate build directories. The GPU build needs an installed,
CUDA-enabled Kokkos because Kokkos' `nvcc_wrapper` must be the C++ compiler at
configure time:

```bash
export KOKKOS_ROOT=/path/to/cuda-kokkos
cmake --preset gpu
cmake --build --preset gpu -j
ctest --preset gpu
```

Build the installed Kokkos for the target architecture (for example
`PASCAL61` for the Quadro P620). This machine has the GPU but no CUDA toolkit,
so the CUDA build is written but not yet run locally.

```bash
saturnCFD -version
saturnCFD -listSolvers
saturnCFD -checkMesh tests/data/meshes/box2x2x2.msh
saturnCFD -checkMesh tests/data/meshes/box2x2x2.msh -report results/mesh
saturnCFD -checkMesh tests/data/meshes/box2x2x2.msh -writeVTK results/mesh
saturnCFD -dumpConfig common -writeTemplate common
saturnCFD -checkConfig common.yaml
saturnCFD -help checkMesh
saturnCFD -help build
saturnCFD -help mpi
saturnCFD -help gpu
saturnCFD -help clean
saturnCFD -help test
```

`CMAKE_BUILD_TYPE=Release` is an optimised, no-debug-symbols build. The
compilation database is `buildCPU/compile_commands.json`. Clean with
`cmake --build buildCPU --target clean` or by removing the build directory.

Enable completion once per shell:

```bash
source <(saturnCFD -completion bash)     # bash
source <(saturnCFD -completion zsh)      # zsh
saturnCFD -completion fish | source      # fish
```

After that, `saturnCFD -dump<Tab>` completes `-dumpConfig`, and
`saturnCFD -checkMesh tests/<Tab>` completes files and directories.

## Decisions

- [ADR log](docs/decisions/README.md) — accepted and proposed decisions.
- [v1 roadmap](docs/plan/v1-roadmap.md) — phases, validation gates and risks.
- [Repository layout](docs/plan/repositoryLayout.md) — folders and naming.
- [MPI and GPU design](docs/plan/mpiGpuDesign.md) — decomposition, ghosts,
  one-GPU benchmark.
- [Parallel testing](docs/plan/parallelTesting.md) — MPI/GPU correctness and
  scaling tests.
- [Profiling and sanitizing](docs/guides/profiling.md) — Valgrind, sanitizers,
  perf, Nsight, compute-sanitizer.
- [FVM/Kokkos plan](docs/plan/fvmKokkosPlan.md) — fields, device geometry,
  kernels and tests.
- [Phase 1 status](docs/plan/phase1Status.md) — what is done and what remains.
- [Command cheat sheet](docs/guides/commands.md) — build, test, install, help.
- [Documentation pipeline](docs/plan/documentationPipeline.md) — Doxygen,
  GitHub Pages, PDF and clang-format.
- [Design summary](docs/Design-v1.md) — the short version.

Accepted so far:

- structured hexahedral Gmsh `.msh` only; 2D is a one-cell-thick 3D mesh;
- `checkMesh` validates topology, structuredness, dimension and quality;
- collocated cell-centred variables, Rhie–Chow, SIMPLE/PISO;
- PETSc for all linear algebra; no home-grown Krylov solver;
- separate solver modules for Burgers, heat, Stokes and incompressible flow;
- YAML schema generated from solver code, parsed with **yaml-cpp 0.8**;
- SI only in v1.0; non-SI YAML input is a validation error;
- single-dash CLI, `-help`, `-checkMesh` (default `.vtu` + `.json`),
  optional `-writeVTK` patches, generated templates with
  `-dumpConfig -writeTemplate`, shell completion;
- user-editable solvers: copy a solver directory, edit equations, rebuild;
- no AMR in v1.0; AMR/two-phase is a later milestone.

## Dependencies

- C++20 compiler, CMake >= 3.24
- Kokkos (CPU/GPU)
- MPI (Open MPI / MPICH)
- PETSc (MPI build)
- yaml-cpp 0.8.x
- doctest (test framework)
- Doxygen + LaTeX (documentation)
- clang-format (style)

## Documentation

- HTML (GitHub Pages): https://ananth-narayan-iitm.github.io/saturnCFD/
- Source: [`docs/`](docs/), built with Doxygen.
- PDF: produced by the `docs` workflow on manual dispatch.
- Presentation: [`docs/presentation/saturnCFD.pdf`](docs/presentation/saturnCFD.pdf)
  (Beamer source in the same directory).
- Math: Markdown uses `$...$` and `$$...$$`; the Doxygen filter converts them.

```bash
mkdir -p buildCPU
doxygen docs/Doxyfile
# HTML: buildCPU/doxygen/html/index.html
# PDF:  make -C buildCPU/doxygen/latex
```

The docs workflow publishes the HTML to the `gh-pages` branch. Enable Pages
once in repository settings:

**Settings → Pages → Build and deployment → Deploy from a branch →
Branch: `gh-pages` / (root)` → Save.**

Direct link: https://github.com/Ananth-Narayan-IITM/saturnCFD/settings/pages

The workflow also needs write access to push `gh-pages`:
**Settings → Actions → General → Workflow permissions → Read and write
permissions**.

If Pages is not enabled, the site returns 404 even though the `gh-pages` branch
exists; the built HTML is also uploaded as the `saturnCFD-docs-html` workflow
artifact. The workflow intentionally does not call the Pages create-site API,
because a workflow token is not allowed to create a Pages site
(`Resource not accessible by integration`).

## Equations and validation

- [Theory index](docs/theory/README.md)
- [Finite-volume operators](docs/theory/finiteVolume.md)
- [Burgers](docs/theory/burgers.md)
- [Heat / convection–diffusion](docs/theory/heat.md)
- [Stokes](docs/theory/stokes.md)
- [Incompressible Navier–Stokes](docs/theory/incompressible.md)
- [Units](docs/theory/units.md)
- [Manufactured solutions](docs/theory/manufacturedSolutions.md)
- [References](docs/references.bib)

## Command line

```bash
saturnCFD case.yaml
saturnCFD -checkMesh mesh.msh
saturnCFD -checkMesh mesh.msh -report results/mesh
saturnCFD -checkMesh mesh.msh -writeVTK results/mesh
saturnCFD -checkConfig case.yaml
saturnCFD -dumpConfig common
saturnCFD -dumpConfig common -writeTemplate common
saturnCFD -help
saturnCFD -help checkMesh
saturnCFD -completion bash
```

`-checkMesh` always writes `<prefix>.vtu` and `<prefix>.json`. The default
prefix is `results/<mesh-stem>`; `-report results/mesh` writes
`results/mesh.json` (the extension is optional). `-writeVTK` additionally
writes one boundary `.vtp` per patch and a `.pvd`; open the `.pvd` in ParaView.
The full option list is in [`docs/protocols/cli.md`](docs/protocols/cli.md).

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
  saturnCFDSolver.cpp C++ scaffold, built as saturnCFDSolver
apps/main.cpp
```

## Writing a solver

> `saturnCFDSolver` and the built-in `burgers` directory arrive in Phase 2.
> The naming and the `addSolver(name)` build path are fixed now.

```bash
saturnCFDSolver burgers myBurgers
# add: addSolver(myBurgers)   to src/solvers/CMakeLists.txt
# edit src/solvers/myBurgers/createFields.cpp and equations.cpp
cmake -S . -B buildCPU
cmake --build buildCPU -j
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
