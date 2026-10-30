# 0005 — Solver lifecycle and runtime selection

- **Status:** accepted
- **Date:** 2026-10-30
- **Owner:** saturnCFD v1
- **Supersedes:** none
- **Protocol:** `docs/protocols/solver.md`
- **Tests:** `tests/simulation/...`

## Context

Each PDE is a separate solver, selected by a YAML case file. The driver must not
grow an `if (solver == "cavity") ... else if (solver == "burgers") ...` chain.
The time loop, output cadence, run manifest and checkpointing are common to all
solvers and should not be duplicated.

## Decision

Define a small host-side `Solver` interface and a registry.

```cpp
class Solver {
public:
  virtual ~Solver() = default;

  virtual std::string name() const = 0;
  virtual void registerOptions(ConfigSchema& schema) const = 0;
  virtual void initialize(const Mesh& mesh, const Config& cfg) = 0;
  virtual Real maxTimeStep() const = 0;
  virtual void advance(Real dt) = 0;
  virtual void writeDiagnostics(Diagnostics& diag) const = 0;
  virtual void writeCheckpoint(Checkpoint& cp) const = 0;
};
```

- `Simulation` owns the time loop, `time`, `step`, output cadence, diagnostics,
  checkpointing and the run manifest.
- The solver owns its fields, its numerical state and its stability limit.
- Runtime selection is a registry lookup on the top-level `solver:` key.
- Each solver registers its own YAML options; the template is generated from
  that registration.
- Concrete solvers compose internal operators; there is no heavy `Model` base
  class until a second solver actually needs one.

## Alternatives considered

| alternative | why not chosen |
|---|---|
| `if/else` in `main` | grows without bound and duplicates setup/output |
| CRTP/compile-time solver selection | no runtime YAML selection; harder to build a single binary |
| A large `Model` hierarchy | premature abstraction; the four solvers differ mainly in their state and operators |
| Solver owns the time loop | duplicates output/checkpoint/manifest logic and makes restart inconsistent |

## Consequences

- **Code:** `src/simulation`, `src/solvers/<name>`, a registry and a factory.
- **Extensibility:** a user copies `src/solvers/<name>`, adds one CMake line and
  rebuilds; the solver is built into the binary. See ADR `0011` and
  `docs/guides/writingASolver.md`.
- **YAML:** `solver:` selects the module; `-dumpConfig <solver>` calls its
  registration function.
- **Testing:** each solver can be driven by a small `Simulation` in a unit test.
- **Risk:** the interface must not leak solver-specific state into
  `Simulation`; diagnostics and checkpoint payloads are generic.

## Validation / exit criteria

- `-listSolvers` prints the registered names.
- `-dumpConfig burgers` prints only Burgers options plus common options.
- `-dumpConfig burgers -writeTemplate file.yaml` writes the same template.
- An unknown solver name is a clear error.
- A trivial solver can be registered in a test and advanced without changing
  `Simulation`.

## References

- Nektar++ solver/module structure (methodology reference).
- OpenFOAM solver selection and `fvSolution`/`fvSchemes` separation (methodology
  reference).
