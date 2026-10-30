# P9 — Testing protocol

**Status:** proposed (framework: doctest, ADR `0012`)

## Purpose

Make every numerical claim reproducible and every regression visible. Tests are
part of the definition of done, not an afterthought.

## Framework

**doctest** is the recommended framework (ADR `0012`): a single-header C++
framework with auto-registration, subcases, tags and fast compilation. Tests run
through CTest.

If doctest is not accepted, the fallback is Catch2 v3. The v0 custom harness is
retired.

## Test categories

| label | what it checks | when it runs |
|---|---|---|
| `[unit]` | boxes, fields, units, config, mesh helpers | every build |
| `[mesh]` | Gmsh reader, `checkMesh`, structured detection | every build |
| `[config]` | schema, YAML, SI-only units, BC/initial validation | every build |
| `[mms]` | manufactured-solution order of accuracy | nightly / release |
| `[regression]` | analytic and benchmark cases (Burgers wave, Poiseuille, Ghia) | nightly / release |
| `[parity]` | CPU vs GPU within precision floor | GPU CI |
| `[performance]` | stencil/cavity timing smoke test | manual / release |

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
tests/regression/burgers/traveling-64.yaml
tests/regression/burgers/traveling-64.expected
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
