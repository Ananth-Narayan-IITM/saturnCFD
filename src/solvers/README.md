# solvers

One directory per PDE. A solver is self-contained: lifecycle, options, state,
equations and diagnostics.

```text
src/solvers/
  CMakeLists.txt
  burgers/
  heat/
  stokes/
  incompressible/
```

## Adding a solver

```bash
saturnCFDSolver burgers myBurgers
# addSolver(myBurgers) to src/solvers/CMakeLists.txt
```

Then edit `createFields.cpp`, `equations.cpp` and `MyBurgersOptions.cpp` and
rebuild. See `docs/guides/writingASolver.md`.

## Boundaries

- A solver may use `mesh`, `fv`, `linalg`, `io` and `simulation`.
- A solver may not edit them for its own equation.
- No runtime plugin in v1.0: solvers are static libraries linked into
  `saturnCFD`.

## Tests

- `tests/solvers/<name>/` — unit and analytic tests.
- `tests/regression/<name>/` — accepted cases.

## Docs

- `docs/guides/writingASolver.md`
- ADR `0011`
