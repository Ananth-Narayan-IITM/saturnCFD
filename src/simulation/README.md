# simulation

The driver-facing layer: solver selection, time loop, diagnostics and output
cadence.

## Implemented

- `Solver` interface — state, explicit `rhs`, boundary refresh, maximum wave
  speed, maximum diffusivity, minimum cell size and spatial dimension.
- `SolverRegistry` — factory registration and lookup by name.
- `TimeControl` — dt bounds, CFL, diffusion safety, adaptive stability limit,
  maximum step count.
- `TimeScheme` — forward Euler, SSPRK2, SSPRK3 implemented; backward Euler,
  Crank–Nicolson and BDF2 represented but awaiting the implicit diffusion
  solve.
- `Simulation` — initialize, initial conditions, boundary refresh, stage
  combination, diagnostics and final state.
- Diagnostics — L1, L2, Linf, CFL, diffusive number, step, time and dt.
- Workspaces are allocated once; no allocation inside the time loop.

## Boundaries

- Solvers own their fields and equations; `Simulation` does not.
- Solvers do not write files directly.
- No YAML parsing; the config is already validated.
- No unit conversion.
- Implicit time schemes throw until a solver provides an implicit solve.

## How a solver plugs in

```cpp
class MySolver : public saturn::Solver {
  std::string name() const override;
  void initialize() override;
  void applyInitialConditions() override;
  void applyBoundaryConditions() override;
  void rhs(const CellField& state, CellField& rate) override;
  Real maxWaveSpeed() const override;
  Real maxDiffusivity() const override;
  Real minCellSize() const override;
  int dimension() const override;
  CellField& state() override;
  const CellField& state() const override;
};
```

`Simulation` calls `initialize`, `applyInitialConditions` and
`applyBoundaryConditions` once, then per stage calls `rhs` and refreshes the
ghosts after every state update.

## Tests

`tests/TestSimulation.cpp` — constant-field preservation for all explicit
schemes, cosine diffusion decay, bounded sine advection, stability-limit
selection, registry behaviour, and implicit-scheme rejection. It runs inside
`testSaturnFV` under 1/2/4 ranks.

## Docs

- `docs/theory/timeIntegration.md`
- `docs/protocols/solver.md`
- `docs/guides/writingASolver.md`
- `docs/plan/solverSequence.md`
- ADR `0005`, `0011`
