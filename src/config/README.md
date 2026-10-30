# config

YAML is the only user interface for a case. yaml-cpp parses it; this module
validates it before any field is allocated.

## Responsibilities

- `ConfigSource` — yaml-cpp wrapper with file/line/column errors.
- `ConfigSchema` — typed registry: key, type, default, description, dimension,
  allowed values, range, required.
- `ConfigBinder` — converts YAML nodes into typed SI values.
- `Validator` — unknown keys, types, ranges, units, BCs, initial conditions.
- Template generation for `-dumpConfig` and `-writeTemplate`.

## Boundaries

- No mesh topology logic; it calls `checkMesh` and consumes the patch list.
- No solver physics; solvers register their own options.
- No hand-written YAML parser.
- v1.0 accepts SI only; non-SI is an error, not a conversion.

## Tests

- `tests/config/` — parser, schema, units, BC validation.
- `tests/regression/config/` — SI accepted/rejected, template round-trip.

## Docs

- `docs/protocols/config.md`
- `docs/protocols/units.md`
- ADR `0006`, `0008`
