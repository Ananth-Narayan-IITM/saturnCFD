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

Implemented solvers are `saturnBurger` (1D/2D/3D scalar Burgers),
`saturnHeat` (1D/2D/3D scalar convection–diffusion) and `saturnStokes`
(2D/3D steady coupled Stokes with PETSc).

```text
src/solvers/
  CMakeLists.txt
  BuiltinSolvers.cpp       # registers every built-in case-driven solver
  saturnBurger/
    CMakeLists.txt
    SaturnBurger.hpp       # core solver (no YAML)
    SaturnBurger.cpp
    BurgersCase.hpp        # YAML schema + factory + registration
    BurgersCase.cpp        # case presets, including the exact wave
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

## `saturnStokes`

`saturnStokes` solves

$$
-\nabla p + \mu\nabla^2\mathbf{u}=\mathbf{f},
\qquad
\nabla\cdot\mathbf{u}=0
$$

with a coupled $u,v,w,p$ saddle-point matrix and PETSc. The validation core
uses the direct coupled solve; the fieldsplit/Schur path is being tuned.
Examples are in `examples/saturnStokes/`. Design:
[`docs/theory/coupledSaddlePoint.md`](../../docs/theory/coupledSaddlePoint.md)
and [`docs/theory/stokes.md`](../../docs/theory/stokes.md).

## Tests

- `tests/TestSaturnBurger.cpp` and `tests/TestSaturnBurger2D.cpp` — unit and
  analytic tests, run by `testSaturnFV` on 1/2/4 ranks.
- `tests/regression/burgers/traveling64.yaml` — 1D YAML CLI regression.
- `tests/regression/burgers2d/traveling64.yaml` — 2D YAML CLI regression.
- `tests/TestSaturnStokes.cpp` — 2D/3D Poiseuille and Couette.
- `ctest --preset solver` — Burgers, heat and Stokes tests.
- `python3 docs/validation/scripts/validateBurgers.py` — 1D convergence table and plot.

## Docs

- `docs/guides/writingASolver.md`
- `docs/theory/burgers.md`
- `src/solvers/saturnBurger/README.md`
- ADR `0011`
