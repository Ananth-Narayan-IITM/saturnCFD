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
  BuiltinSolvers.cpp       # registers every built-in case-driven solver
  saturnBurger/
    CMakeLists.txt
    SaturnBurger.hpp
    SaturnBurger.cpp
    BurgersCase.hpp
    BurgersCase.cpp
    TravelingWave.hpp
    Register.hpp
    Register.cpp
    README.md
```

Each solver directory has its own `README.md` explaining the equations, data
layout, algorithm, validation and limitations.

## Adding a solver

```bash
cp -r src/solvers/saturnBurger src/solvers/saturnMyBurger
# rename the class/target and add one line to src/solvers/CMakeLists.txt:
#   addSolver(saturnMyBurger)
# then add its registration call to BuiltinSolvers.cpp
```

The `saturnCFDSolver` scaffold tool will automate the copy/rename. Until then
the manual copy works. See `docs/guides/writingASolver.md`.

## Boundaries

- A solver may use `mesh`, `fv`, `linalg`, `io` and `simulation`.
- A solver may not edit them for its own equation.
- No runtime plugin in v1.0: solvers are static libraries linked into
  `saturnCFD`.

## Tests

- `tests/TestSaturnBurger.cpp` — unit and analytic tests, run by
  `testSaturnFV` on 1/2/4 ranks.
- `tests/regression/burgers/traveling64.yaml` — YAML CLI regression.
- `ctest --preset burgers` — YAML case on 1/2/4 ranks.
- `python3 tools/validateBurgers.py` — convergence table and validation plot.

## Docs

- `docs/guides/writingASolver.md`
- `docs/theory/burgers.md`
- `src/solvers/saturnBurger/README.md`
- ADR `0011`
