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
| `[benchmark]` | halo/stencil timing, strong/weak scaling | manual / release (planned) |
| `[memcheck]` | Valgrind/ASan/UBSan runs | manual / nightly (planned) |

Two umbrella labels partition the suite and drive the main presets:

| umbrella | contains | preset |
|---|---|---|
| `[solver]` | FV operators, Burgers and heat numerics | `ctest --preset solver` |
| `[saturnCFD]` | CLI, config, mesh, PETSc, VTK, checkpoint | `ctest --preset saturnCFD` |

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
    saturnBurger/
    saturnHeat/
    saturnStokes/          planned
    saturnIncompressible/  planned
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
- all cases run on 1, 2 and 4 ranks;
- `fv: distributed LocalMesh` checks rank-0 extraction against direct local
  extraction on every rank.

The same binary is run under `mpirun -np 1/2/4`; a failure on any rank fails the
CTest test. A CUDA build adds the `gpu` label and `ctest --preset gpu` runs the
device tests. The CUDA build is verified on the local Quadro P620; see
[`../guides/gpu.md`](../guides/gpu.md).

## Burgers solver tests

`tests/TestSaturnBurger.cpp` checks:

- viscous traveling wave, N=32/64/128/256: L2 9.015e-3 -> 2.284e-3 ->
  5.291e-4 -> 1.236e-4, observed orders 1.98/2.11/2.10;
- constant field preserved exactly;
- periodic inviscid shock: bounds [-0.9954, 0.9954], integral 1.5e-16;
- all cases under 1/2/4 ranks, with bitwise-identical N=256 norms;
- `fv: Burgers constant flux` separately checks the $-0.5/+0.5$ face sign
  and zero divergence;
- `burgers_vtk` checks `output.vtkEvery`: `.vtu` frames and a `.pvd`
  collection;
- `burgers_3d_traveling16` checks the 3D YAML and norms;
- `burgers_checkpoint` checks checkpoint/restart against an uninterrupted run.

`tests/TestSaturnBurger2D.cpp` checks the 2D diagonal plane wave on
$[-1,1]^2$: $L^2=9.909\times10^{-3}$ at $32^2$ and
$2.555\times10^{-3}$ at $64^2$, observed order 1.96.
`tests/TestSaturnBurger3D.cpp` checks a 3D x-plane wave and the 3D diagonal
example. The 3D coarse-grid orders are lower than 2; dense cluster runs are
needed for the asymptotic value.

The stable YAML regressions are `tests/regression/burgers/traveling64.yaml`
and `tests/regression/burgers2d/traveling64.yaml`; they use
`tests/checkBurgers.cmake`. `ctest --preset solver` runs the Burgers and heat
numerics on 1/2/4 ranks. The user-facing examples
`examples/saturnBurger/saturnBurger1D/` and
`examples/saturnBurger/saturnBurger2D/` are schema-checked by CTest and have
their own independent `compareAnalytic.py` scripts.

VTK output and checkpoint/restart belong to the `saturnCFD` preset:
`ctest --preset saturnCFD`.

MPI test targets set `OMP_NUM_THREADS=1`. Without it, four ranks each request
the Kokkos default 12 threads on a 24-core node, and `fv_np4` took about 370 s
instead of 2-3 s.

The shock-speed table and dense 3D cluster validation are next. See
[`../validation/burgers.md`](../validation/burgers.md),
[`../theory/burgers.md`](../theory/burgers.md) and
[`../../src/solvers/saturnBurger/README.md`](../../src/solvers/saturnBurger/README.md).

## Heat solver tests

`tests/TestSaturnHeat.cpp` checks the periodic Fourier mode
`T = A sin(k.x - (a.k)t) exp(-alpha |k|^2 t)`:

- 3D, `alpha = 0.01`, `a = (1, 0.5, 0.25)`, `k = 2*pi*(1,1,1)`, `t = 0.05`:
  L2 1.397e-2 -> 5.491e-3 -> 2.812e-3 at 16^3/24^3/32^3, observed orders
  2.30 and 2.33;
- a constant field is preserved exactly (Linf equal to the initial value);
- the same binary runs under `mpirun -np 1/2/4`, covering periodic halos and
  the rank-0 mesh distribution;
- `heat_checkpoint` checks the generic scalar checkpoint/restart path: the
  resumed run matches the uninterrupted run to roundoff.

The stable YAML regressions are `tests/regression/heat2d/fourier64.yaml` and
`tests/regression/heat3d/fourier32.yaml`, checked by `tests/checkHeat.cmake`.
`ctest --preset solver` runs the heat slice; in an MPI build it runs the 3D case
on 1/2/4 ranks. The user-facing examples
`examples/saturnHeat/saturnHeat2D/` and `saturnHeat3D/` are schema-checked by
CTest and ship independent `compareAnalytic.py` scripts.

## Stokes solver tests

`testSaturnStokes` checks the coupled FV system under the `solver` label:

- **Poiseuille 2D:** 16$^2$ and 32$^2$; $L^2$ 4.88e-4 -> 1.22e-4, second order.
- **Couette 2D:** 24$^2$; linear profile to roundoff.
- **Poiseuille 3D:** 16$^2\times4$ and 24$^2\times4$; extruded channel.
- **Couette 3D:** 12$^2\times4$; linear profile to roundoff.
- **Pressure gauge:** mean-removed pressure error at roundoff.
- **Examples:** `examples/saturnStokes/saturnStokes2D` and `saturnStokes3D`
  each ship independent `compareAnalytic.py` scripts that validate both
  Poiseuille and Couette, write a combined `summary.txt`, and draw 2D fields
  or 3D velocity-profile surfaces (numerical, analytic, error).
  `output.vtkEvery: 1` writes one `.vtu` with
  `u,v,w,p`.

The coupled direct PETSc solve is used for the validation core. The
linear-algebra fieldsplit/Schur path is tested separately by
`petsc: coupled saddle-point fieldsplit solves a 2-field block` on 1/2/4
ranks; production tuning is a follow-up. 2D Stokes tests run on 1/2/4 ranks;
3D tests run serially, with a message on higher rank counts. See
[`../validation/stokes.md`](../validation/stokes.md).

## PETSc tests

`tests/TestPetsc.cpp` currently checks:

- 1D Poisson with Dirichlet boundaries: order 1.98 and 1.99;
- 3D Poisson with CG + GAMG: L2 error about 2.1e-3 at 12^3;
- all-Neumann 1D matrix: constant nullspace attached and verified with
  `MatNullSpaceTest`, CG + GAMG converges;
- coupled $u,p$ saddle-point block with `PCFIELDSPLIT` and a Schur
  preconditioner: GMRES converges in one iteration on 1/2/4 ranks;
- all cases run on 1, 2 and 4 ranks.

The PETSc tests use `DMDA` only as a linear-algebra smoke test. The production
finite-volume matrix assembler and the coupled saddle-point/fieldsplit test are
the next phase; the latter verifies the Schur block solve and the pressure
nullspace before `saturnStokes` is wired in. See
[`../guides/petsc.md`](../guides/petsc.md) and
[`../theory/poisson.md`](../theory/poisson.md).

## Time-loop tests

`tests/TestSimulation.cpp` checks:

- constant-field preservation with forward Euler, SSPRK2 and SSPRK3;
- cosine diffusion decay against the analytic L2;
- bounded sine advection with SSPRK3;
- `TimeControl` selecting the convective or diffusive stability limit;
- solver registry add/lookup/duplicate/unknown-name behaviour;
- a clear error for an implicit scheme without an implicit solver.

These run inside `testSaturnFV` under 1/2/4 ranks. The scheme derivations are
in [`../theory/timeIntegration.md`](../theory/timeIntegration.md).

## Regression policy

The stored-expectation format, tolerances and update rules are in
[`docs/validation/regressionTests.md`](../validation/regressionTests.md).

## Presets

CTest does not read default command-line flags from `CMakeLists.txt`. The
supported shortcut is `CMakePresets.json`:

```bash
ctest --preset default       # everything, output on failure
ctest --preset cpu           # everything except GPU-labelled tests
ctest --preset solver        # FV operators, Burgers and heat, 1/2/4 ranks
ctest --preset saturnCFD     # CLI, config, mesh, PETSc, VTK, checkpoint
ctest --preset gpu           # GPU-labelled tests in buildGPU
```

Each test also keeps its fine-grained labels, so a direct label filter still
works:

```bash
ctest --test-dir buildCPU -L burgers --output-on-failure
ctest --test-dir buildCPU -L saturnCFD --output-on-failure
```

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

The Burgers traveling wave is stored as a YAML case and checked by a CMake
script:

```text
tests/regression/burgers/traveling64.yaml
tests/checkBurgers.cmake
```

The script runs the real `saturnCFD` CLI, reads the JSON summary and the CSV
profile, and checks the analytic norms and row count. The CTest slice is
`ctest --preset solver`. A change to the accepted norm is a deliberate code
change, not a test-run side effect.

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
