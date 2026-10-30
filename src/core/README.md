# core

Foundation types and policies. No mesh, no equations, no PETSc.

## Responsibilities

- `Types.hpp` — `Real`, `Index`, `BigIndex`, `Vec3`.
- `Units.hpp` — `UnitSystem { SI }`, `Dimension`, SI-only validation.
- `Error.hpp` — `SaturnError`, `ConfigError`, `MeshError`, `UsageError`.
- `Log.hpp` (later) — rank-aware logging.
- `Timer.hpp` (later) — fenced wall-clock timing.
- `PhysicalProperties.hpp` (later) — density, viscosity, diffusivity in SI.

## Boundaries

- No file I/O.
- No YAML.
- No Kokkos/PETSc calls.
- No physics equations.

## Tests

- `tests/core/` — units, error codes, vector algebra.
- `tests/regression/config/` — SI-only acceptance and rejection.

## Docs

- `docs/theory/units.md`
- `docs/protocols/units.md`
- ADR `0008`
