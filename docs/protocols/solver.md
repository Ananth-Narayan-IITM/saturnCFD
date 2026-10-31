# P6 — Solver lifecycle protocol

**Status:** accepted (ADR `0005`). The explicit lifecycle and time loop are
implemented; config registration, output cadence and implicit solves follow.

## Purpose

Let one binary run several PDE solvers selected by YAML without duplicating the
time loop, output, checkpointing or configuration machinery.

## Interface

```cpp
class Solver {
public:
  virtual ~Solver() = default;
  virtual std::string name() const = 0;
  virtual void initialize() = 0;
  virtual void applyInitialConditions() = 0;
  virtual void applyBoundaryConditions() = 0;
  virtual void rhs(const CellField& state, CellField& rate) = 0;
  virtual Real maxWaveSpeed() const = 0;
  virtual Real maxDiffusivity() const = 0;
  virtual Real minCellSize() const = 0;
  virtual int dimension() const = 0;
  virtual CellField& state() = 0;
  virtual const CellField& state() const = 0;
};
```

The solver owns its fields and exposes the explicit rate `du/dt = rhs(u)`.
`Simulation` owns the stage combination, stability limit and diagnostics.
Forward Euler, SSPRK2 and SSPRK3 are implemented. Backward Euler,
Crank–Nicolson and BDF2 are represented in `TimeScheme` and will use
`saturnLinAlg` for implicit diffusion.

Config registration, output/checkpoint callbacks and solver factories that
receive `Config` are the next interface extension.

## Ownership

- `Simulation` owns `time`, `step`, the time loop, output cadence, the run
  manifest and checkpoint scheduling.
- `Solver` owns its fields and internal stage state.
- `Solver` does not write files directly; it fills `Diagnostics` and
  `Checkpoint` objects.
- `Solver` does not parse YAML; it receives a validated `Config`.

## Registry

```cpp
solverRegistry().add("burgers",
                     [] { return std::make_unique<BurgersSolver>(); });
```

`-listSolvers`, `solver:` in YAML and `-dumpConfig <solver>` all use the
same registry.

## User-editable solvers

A solver is a self-contained directory under `src/solvers/<name>/`. It contains
its lifecycle, options, state setup, equations and diagnostics. It is compiled
into `saturnCFD` at build time; there is no runtime plugin in v1.0.

A user copies a built-in solver, adds one line to
`src/solvers/CMakeLists.txt`, edits `createFields`, `equations` and `options`,
and rebuilds:

```bash
saturnCFDSolver burgers myBurgers
# add: addSolver(myBurgers)   to src/solvers/CMakeLists.txt
cmake -S . -B buildCPU
cmake --build buildCPU -j
```

The full workflow, directory layout and API-version rule are in
[`../guides/writingASolver.md`](../guides/writingASolver.md) and ADR `0011`.

## Time loop

```text
initialize()
applyInitialConditions()
applyBoundaryConditions()
while (time < end && step < maxSteps) {
  dt = TimeControl::stableDt(maxWaveSpeed, maxDiffusivity, hMin, dimension)
  dt = min(dt, end - time)
  explicitStep(dt)            // FE / SSPRK2 / SSPRK3
  applyBoundaryConditions()
  time += dt; step += 1
  diagnostics at the requested cadence
}
```

`Simulation` clips the stability limit to the remaining time; it does not
invent a larger step. A step below `dtMin` is an error. `docs/theory/timeIntegration.md`
contains the stage formulas and the implicit schemes.

## Diagnostics

Common fields: time, step, dt, wall time, kinetic energy, max velocity,
divergence norm, linear-solver iterations/residual. Solver-specific fields are
added by the solver.

## Checkpoint / restart

The checkpoint contains the mesh identity, solver name, config hash, step,
time and field data. Restart requires the same solver, mesh and precision.
Bitwise reproducibility is not promised across backends; document the
tolerance.

## Invariants

- A solver is fully constructed before `initialize`.
- `advance` is the only place that mutates the solver state.
- The solver never advances `time`; `Simulation` does.
- Output cadence and checkpoint cadence are simulation concerns.
- Every solver registers all its options; no hidden key.

## Tests

- registry lists all solvers;
- dump-config for each solver round-trips through check-config;
- a trivial test solver advances and writes diagnostics;
- unknown solver is a clear error;
- restart reproduces the same step/time and fields within tolerance.

## References

- Nektar++ solver/module structure.
- OpenFOAM solver selection (methodology reference).
