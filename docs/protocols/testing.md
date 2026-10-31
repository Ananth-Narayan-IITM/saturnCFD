# P9 — Testing protocol

**Status:** accepted (framework: doctest, ADR `0012`)

## Purpose

Make every numerical claim reproducible and every regression visible. Tests are
part of the definition of done, not an afterthought.

## Framework

**doctest** is the framework (ADR `0012`): a single-header C++ framework with
auto-registration, subcases, tags and fast compilation. Tests run through CTest.
Catch2 v3 remains the fallback if doctest becomes limiting. The v0 custom
harness is retired.

## Test categories

| label | what it checks | when it runs |
|---|---|---|
| `[unit]` | boxes, fields, units, config, mesh helpers | every build |
| `[mesh]` | Gmsh reader, `checkMesh`, structured detection | every build |
| `[config]` | schema, YAML, SI-only units, BC/initial validation | every build |
| `[fv]` | CellField, halo adapter, FVM operators | every FVM change |
| `[mms]` | manufactured-solution order of accuracy | every FVM change |
| `[linalg]` | PETSc discovery, matrix/solve wrappers | every PETSc change |
| `[petsc]` | Poisson matrix/solve, nullspace, 1/2/4 ranks | every PETSc change |
| `[mpi]` | partition, halo, FVM, PETSc, 1/2/4-rank runs | MPI CI |
| `[gpu]` | GPU parity and device indexing | GPU CI |
| `[regression]` | analytic and benchmark cases (Burgers wave, Poiseuille, Ghia) | nightly / release |
| `[parity]` | CPU vs GPU within precision floor | GPU CI |
| `[benchmark]` | halo/stencil timing, strong/weak scaling | manual / release |
| `[memcheck]` | Valgrind/ASan/UBSan runs | manual / nightly |

## Layout

```text
tests/
  TestMain.cpp            doctest main
  core/                   types, units, errors
  mesh/                   reader, checkMesh, structured detection
  config/                 YAML, schema, SI-only validation, template
  fv/                     gradient, divergence, laplacian, limiters
  linalg/                 PETSc wrapper, null space, multi-RHS
  solvers/
    burgers/
    heat/
    stokes/
    incompressible/
  regression/             case files + expected values
```

## Manufactured solutions

MMS is the primary verification method for the FVM operators. The methodology,
expected orders, rank/backend matrix and pitfalls are in
[`../theory/manufacturedSolutions.md`](../theory/manufacturedSolutions.md).

## Implemented FV tests

`tests/TestFv.cpp` checks:

- `CellField` fill and owned-cell reduction;
- Kokkos halo on two periodic ghost layers, all 26 directions, 1/2/4 ranks;
- 3D periodic Laplacian MMS: observed order 1.978 and 1.994 on the CPU OpenMP
  build;
- 2D periodic Laplacian MMS: observed order 1.994 and 1.999;
- a stored 32^3 CPU reference `L2` (`0.13435580757778084`) checked to `1e-10`;
- CUDA-only cases (`SATURN_BACKEND_CUDA`): active device, 32^3 MMS against the
  CPU reference to `1e-9`, and periodic halo roundoff.

`tests/TestLocalMesh.cpp` checks:

- `LocalMesh` cell volume/centroid, face area/normal/distance and patch tags;
- linear interpolation exact at face centroids;
- Dirichlet ghost mirror values;
- linear gradient with Dirichlet ghosts exact at every cell;
- linear divergence with Dirichlet ghosts exact at every cell;
- Neumann ghosts reproduce the exact linear gradient;
- 2D `LocalMesh` ignores the empty direction;
- periodic divergence integrates to zero over the domain;
- sheared-mesh non-orthogonal Laplacian is exact for a linear field;
- constant-field upwind/Rusanov convection has zero divergence;
- upwind convection MMS: observed order 0.960 and 0.990;
- Rusanov equals upwind for scalar advection (algebraic identity);
- MUSCL linear-field interior exactness with all three limiters;
- MUSCL convection MMS: about 1.61/1.57 (minmod), 1.58/1.52 (van Leer),
  1.15/1.42 (superbee);
- periodic step advection stays within the initial bounds with all limiters;
- steady advection–diffusion MMS: observed order 0.847 and 0.936;
- discrete divergence theorem on a sheared mesh: 3.5 vs 3.5;
- Rhie–Chow correction vanishes for a linear pressure and is nonzero for a
  checkerboard pressure;
- periodic 3D gradient MMS: observed order 1.967 and 1.992;
- periodic 3D divergence MMS: observed order 1.967 and 1.992;
- all cases run on 1, 2 and 4 ranks.

The same binary is run under `mpirun -np 1/2/4`; a failure on any rank fails the
CTest test. A CUDA build adds the `gpu` label and `ctest --preset gpu` runs the
device tests. The CUDA build is written but not yet executed on this machine
because the CUDA toolkit is not installed.

## PETSc tests

`tests/TestPetsc.cpp` currently checks:

- 1D Poisson with Dirichlet boundaries: order 1.98 and 1.99;
- 3D Poisson with CG + GAMG: L2 error about 2.1e-3 at 12^3;
- all-Neumann 1D matrix: constant nullspace attached and verified with
  `MatNullSpaceTest`, CG + GAMG converges;
- all cases run on 1, 2 and 4 ranks.

The PETSc tests use `DMDA` only as a linear-algebra smoke test. The production
finite-volume matrix assembler is the next phase. See
[`../guides/petsc.md`](../guides/petsc.md) and
[`../theory/poisson.md`](../theory/poisson.md).

## Regression policy

The stored-expectation format, tolerances and update rules are in
[`docs/validation/regressionTests.md`](../validation/regressionTests.md).

## Presets

CTest does not read default command-line flags from `CMakeLists.txt`. The
supported shortcut is `CMakePresets.json`:

```bash
ctest --preset default       # all tests, output on failure
ctest --preset regression    # regression label
ctest --preset unit
ctest --preset mesh
ctest --preset config
ctest --preset cli
ctest --preset mpi           # partition/halo + FVM on 1/2/4 ranks
ctest --preset fv            # CellField, halo and MMS
ctest --preset petsc         # PETSc Poisson/nullspace on 1/2/4 ranks
ctest --preset gpu           # GPU-labelled tests in build-gpu
ctest --preset benchmark     # benchmark smoke tests
```

`ctest --test-dir build-cpu -L regression --output-on-failure` still works.

## Rules

1. Every public function with numerical behaviour has at least one unit or
   manufactured test.
2. Every solver has an analytic or manufactured validation case before a
   benchmark case.
3. Tolerances are explicit and depend on precision (double/float).
4. A failing test is never removed; it is fixed or documented as a known
   failure with a link.
5. GPU tests are skipped, not silently passed, when no GPU is available.
6. `ctest --output-on-failure` is the single entry point.
7. Tests do not depend on the current working directory; paths come from
   CMake/CTest or a fixture.
8. No test writes into the source tree; outputs go to the build tree or a temp
   directory.

## Unit test example

```cpp
#include <doctest/doctest.h>
#include "core/Units.hpp"

TEST_CASE("units: SI only") {
  CHECK(parseUnitSystem("SI") == UnitSystem::SI);
  CHECK_THROWS(parseUnitSystem("CGS"));
  CHECK_NOTHROW(validateUnitsBlock(""));
  CHECK_THROWS(validateUnitsBlock("system: CGS"));
}
```

## Regression example

The Burgers traveling wave is stored as a case plus expected error norms:

```text
tests/regression/burgers/traveling64.yaml
tests/regression/burgers/traveling64.expected
```

The test runs the case, reads the diagnostics, and checks the norms against the
expected values within a precision-dependent tolerance. The expected file
records the git hash and the date; updating it requires a reason.

## CI

- CPU serial and OpenMP: configure, build, `ctest`.
- CUDA: build and `ctest` on a GPU runner; skip if unavailable.
- Sanitizers: ASan/UBSan on the CPU unit tests.
- `compute-sanitizer` on a small GPU case when a GPU runner exists.

## Exit criteria

- `ctest --output-on-failure` is green on CPU serial and OpenMP.
- Every test has a label.
- A deliberately broken unit/config/mesh test fails with file and line.
- A GPU test is skipped cleanly when no device is present.
- Regression expected files are versioned and reviewed.
