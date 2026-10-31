# solvers

One directory per PDE. A solver is self-contained: lifecycle, options, state,
equations and diagnostics.

## Naming convention

Every solver library is named

```text
saturn<SolverName>
```

Examples:

```text
saturnBurger
saturnHeat
saturnStokes
saturnIncompressible
```

The first implemented solver is `saturnBurger` for 1D scalar Burgers.

```text
src/solvers/
  CMakeLists.txt
  saturnBurger/
    SaturnBurger.hpp
    SaturnBurger.cpp
    CMakeLists.txt
    README.md
```

Each solver directory has its own `README.md` explaining the equations, data
layout, algorithm, validation and limitations.

## Adding a solver

```bash
saturnCFDSolver saturnBurger myBurger
# addSolver(myBurger) to src/solvers/CMakeLists.txt
```

Then edit `createFields.cpp`, `equations.cpp` and the options file and rebuild.
See `docs/guides/writingASolver.md`.

## Boundaries

- A solver may use `mesh`, `fv`, `linalg`, `io` and `simulation`.
- A solver may not edit them for its own equation.
- No runtime plugin in v1.0: solvers are static libraries linked into
  `saturnCFD`.

## Tests

- `tests/solvers/<name>/` — unit and analytic tests.
- `tests/regression/<name>/` — accepted cases.
- `saturnBurger` is currently tested in `testSaturnFV`
  (`tests/TestSaturnBurger.cpp`).

## Docs

- `docs/guides/writingASolver.md`
- `docs/theory/burgers.md`
- `src/solvers/saturnBurger/README.md`
- ADR `0011`
