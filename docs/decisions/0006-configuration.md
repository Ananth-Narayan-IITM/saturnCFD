# 0006 — Configuration: typed schema, generated templates, strict validation

- **Status:** accepted
- **Date:** 2026-10-30
- **Owner:** saturnCFD v1
- **Supersedes:** none
- **Protocol:** `docs/protocols/config.md`
- **Tests:** `tests/config/...`

## Context

All initial and boundary conditions live in YAML. The user must be able to ask
a solver for its available parameters, and a typo or an invalid boundary patch
must fail before the solver allocates fields or reads a mesh. Maintaining a
separate YAML template file guarantees that it will drift from the code.

## Decision

- A typed `ConfigSchema` is the single source of truth. Every option is
  registered with key, type, default, description, units, allowed range and an
  optional validator.
- Each solver registers its options in `registerOptions()`. Common options are
  registered by the mesh, time, linear-solver and I/O modules.
- `-dumpConfig <solver>` calls the same registration code and prints a
  commented YAML template. No separate template file exists.
- `-checkConfig <case.yaml>` runs the full validation and exits without
  solving.
- Unknown keys are errors, not warnings. Missing required keys are errors.
- Validation stages, in order:
  1. syntax and types;
  2. schema (required, unknown, range, enum);
  3. units parse, match the expected dimension and convert to SI;
  4. cross-field consistency (CFL, time step, dimension, solver/field match);
  5. mesh file exists, parses and passes `checkMesh`;
  6. every mesh patch has a BC, and every BC patch exists;
  7. BC type is valid for the field and dimension;
  8. initial conditions have the right shape and dimension;
  9. linear-solver backend and parameters are available;
  10. output directory is writable.
- The parser is behind a `ConfigSource` interface. The recommended parser is
  yaml-cpp; a self-contained subset parser remains possible if dependency
  weight matters.
- `-dumpConfig <solver> -mesh <file>` may list the actual patch names and
  emit a matching `boundary:` skeleton.

## Alternatives considered

| alternative | why not chosen |
|---|---|
| A hand-maintained template file | drifts from the code; the user explicitly asked for generation |
| Loose parsing with warnings | typos silently run with defaults; unacceptable for BCs and initial conditions |
| JSON instead of YAML | the user specified YAML; YAML is more readable for nested BCs |
| A full YAML library plus no schema | syntax is checked but semantics are not |

## Consequences

- **Code:** `src/config` with `ConfigSchema`, `ConfigSource`, validators and a
  `-checkConfig` entry point.
- **YAML:** keys are camelCase; groups are `mesh`, `time`, `fields`,
  `boundary`, `numerics`, `linearSolver`, `output`.
- **Testing:** schema tests, unknown-key tests, patch-mismatch tests.
- **Risk:** yaml-cpp adds a dependency; isolate it behind `ConfigSource`.

## Validation / exit criteria

- `-dumpConfig burgers` is generated and round-trips through
  `-checkConfig`.
- A misspelled key fails with file/line and the closest known key.
- A BC for a non-existent patch fails before field allocation.
- A missing BC for an existing patch fails before field allocation.
- A scalar BC value on a vector field fails.

## References

- YAML 1.2 specification.
- OpenFOAM dictionary validation and `checkMesh` workflow (methodology
  reference).
