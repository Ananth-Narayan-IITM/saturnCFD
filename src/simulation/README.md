# simulation

The driver-facing layer: solver selection, time loop, diagnostics and output
cadence.

## Responsibilities

- `Solver` interface and `SolverRegistry`.
- `Simulation`: `time`, `step`, `advance`, output, checkpoint, manifest.
- Solver factory and `-listSolvers`.
- Common diagnostics collection.

## Boundaries

- Solvers own their fields and equations; `Simulation` does not.
- Solvers do not write files directly.
- No YAML parsing; the config is already validated.
- No unit conversion.

## Tests

- `tests/simulation/` — registry, unknown solver, time loop with a test solver,
  diagnostics cadence.

## Docs

- `docs/protocols/solver.md`
- `docs/guides/writingASolver.md`
- ADR `0005`, `0011`
