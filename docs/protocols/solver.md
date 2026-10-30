# P6 — Solver lifecycle protocol

**Status:** accepted (ADR `0005`)

## Purpose

Let one binary run several PDE solvers selected by YAML without duplicating the
time loop, output, checkpointing or configuration machinery.

## Interface

```cpp
class Solver {
public:
  virtual ~Solver() = default;

  virtual std::string name() const = 0;
  virtual void registerOptions(ConfigSchema& schema) const = 0;
  virtual void initialize(const StructuredMesh& mesh,
                          const Config& cfg) = 0;
  virtual Real maxTimeStep() const = 0;
  virtual void advance(Real dt) = 0;
  virtual void writeDiagnostics(Diagnostics& diag) const = 0;
  virtual void writeCheckpoint(Checkpoint& cp) const = 0;
};
```

## Ownership

- `Simulation` owns `time`, `step`, the time loop, output cadence, the run
  manifest and checkpoint scheduling.
- `Solver` owns its fields and internal stage state.
- `Solver` does not write files directly; it fills `Diagnostics` and
  `Checkpoint` objects.
- `Solver` does not parse YAML; it receives a validated `Config`.

## Registry

```cpp
registerSolver("burgers", [] { return std::make_unique<BurgersSolver>(); });
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
cmake -S . -B build
cmake --build build -j
```

The full workflow, directory layout and API-version rule are in
[`../guides/writingASolver.md`](../guides/writingASolver.md) and ADR `0011`.

## Time loop

```text
initialize()
while (time < end && step < maxSteps) {
  dt = min(maxTimeStep(), end - time)
  advance(dt)
  time += dt; step += 1
  diagnostics / output / checkpoint at their cadence
}
```

`maxTimeStep()` is the solver's stability or accuracy limit. `Simulation`
clips it to the remaining time; it does not invent a smaller step.

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
