# 0012 — Test framework: doctest (proposed)

- **Status:** proposed
- **Date:** 2026-10-31
- **Owner:** saturnCFD v1
- **Supersedes:** none
- **Protocol:** `docs/protocols/testing.md` (to write)
- **Tests:** the test suite itself

## Context

The project needs unit tests, manufactured-solution tests, regression tests and
CPU/GPU parity checks. The v0 custom harness (`tests/TestHarness.hpp`) works but
lacks sections, tags, rich assertions and a standard runner. A real framework
should be small, compile quickly with Kokkos/CUDA and integrate with CTest.

## What doctest is

**doctest** is a single-header C++ testing framework. A test looks like:

```cpp
#include <doctest/doctest.h>

TEST_CASE("box intersect") {
  Box a{{0,0,0}, {4,4,4}};
  Box b{{3,3,3}, {8,8,8}};
  CHECK(a.intersects(b));
  CHECK(a.intersect(b).numPts() == 8);
}
```

It auto-registers tests, runs them from `main`, and reports failures with file
and line. It is header-only, has no external dependency and compiles very
quickly compared with Catch2 or GoogleTest. It supports subcases, tags,
decorators, exception checks and custom reporters.

## Decision (recommended)

Adopt **doctest** as the v1 test framework:

- vendor the single header under `external/doctest/` or fetch a pinned release;
- one `tests/TestMain.cpp` defines `DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN`;
- tests are registered with CTest;
- each module has a test target, and the solver tests are separate;
- tests run on the configured Kokkos backend; CPU/GPU parity is a separate
  labeled suite.

## Alternatives considered

| framework | advantages | why not chosen |
|---|---|---|
| doctest | single header, very fast compile, auto-registration, minimal | fewer advanced features than Catch2 |
| Catch2 v3 | rich matchers, generators, sections, mature | extra compiled library and slower builds with CUDA |
| GoogleTest | industry standard, good reporting | heavier dependency and build; overkill for this project |
| v0 custom harness | no dependency, already written | no sections/tags, weak diagnostics, reinvents a solved problem |

## Consequences

- **Dependency:** one header; trivial to vendor.
- **Build:** a `tests` target and CTest labels; fast incremental test builds.
- **GPU:** the same tests run on the selected backend; GPU-only tests are
  labeled and skipped when unavailable.
- **Migration:** port the v0 box/halo/config tests to doctest; keep the analytic
  cases.
- **Risk:** doctest is less feature-rich than Catch2; if generators or complex
  matchers become necessary, the migration is mechanical.

## Validation / exit criteria

- `ctest` runs all tests and reports failures with file/line.
- A deliberately failing test returns non-zero.
- Tests can be filtered by tag (`[unit]`, `[mms]`, `[regression]`, `[gpu]`).
- The test binary builds on CPU serial/OpenMP and, when available, CUDA.
- Porting one v0 test to doctest is a one-file change.

## References

- doctest documentation: https://github.com/doctest/doctest
- Catch2 documentation: https://github.com/catchorg/Catch2
