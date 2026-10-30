# 0008 — SI units only in v1.0; non-SI is a validation error

- **Status:** accepted
- **Date:** 2026-10-30
- **Owner:** saturnCFD v1
- **Supersedes:** none
- **Protocol:** `docs/protocols/units.md`
- **Tests:** `tests/config/units...`

## Context

The solver must be dimensionally correct from day one. The first release will
use SI only; other unit systems are a later extension. Mixing metres,
millimetres, pascals and hidden dimensionless numbers is the failure mode to
avoid. A unit library inside the numerical kernels is not wanted.

## Decision

1. **Internal representation is SI.** Every field, property and operator uses
   SI values. There is no dimensionless internal mode.
2. **YAML values are SI.** The case file may declare the unit system:
   ```yaml
   units:
     system: SI
   ```
   The block is optional; omitting it means SI.
3. **v1.0 accepts `system: SI` only.** Any other value (`CGS`, `imperial`,
   `custom`, per-quantity units such as `length: mm`) is a validation error.
   The message states that only SI is supported in v1.0 and that other systems
   are a future extension.
4. **No unit conversion happens in v1.0.** The solver receives plain SI
   `double` values. The config binder validates the `units:` block and the
   dimensions of each option, but does not scale values.
5. **`mesh.scale` is removed.** Mesh coordinates are metres.
6. **Output is SI.** VTK, CSV and checkpoints store SI values. The run manifest
   records `units.system: SI`.
7. **Future extension:** a unit table, `system: CGS`, per-value suffixes
   (`1.0 cm`, `1.0e-3 Pa s`) and `fields.<name>.unit`. The schema reserves the
   `units:` block so those can be added without changing solver code.

## YAML form

Accepted in v1.0:

```yaml
units:
  system: SI
```

Also accepted (same meaning):

```yaml
# no units block: SI is the default
```

Rejected in v1.0:

```yaml
units:
  system: CGS
```

```yaml
units:
  length: mm
```

The error is explicit, not a silent conversion.

## Alternatives considered

| alternative | why not chosen |
|---|---|
| Fully dimensionless internally | poor fit for compressible/two-phase and hides unit errors |
| Multi-unit YAML table now | more implementation than v1.0 needs; the user asked for SI only first |
| Full units library inside kernels | compile-time and GPU cost; no benefit in hot loops |
| Raw `Real` with comments only | exactly the unit confusion the user wants to avoid |
| Silent conversion from non-SI | hides mistakes; the user asked for an error |

## Consequences

- **Code:** `src/core/Units.hpp` defines `UnitSystem { SI }` and the
  dimension tags used by the schema. `src/config` validates the block.
- **YAML:** a single optional `units.system: SI` entry; `-dumpConfig` prints it.
- **Validation:** non-SI is an error at `-checkConfig`, before allocation.
- **Solver:** solvers receive SI only. They do not parse or convert units.
- **Future:** the unit table and conversion live in the config layer, never in
  a kernel.

## Validation / exit criteria

- A case with no `units:` block runs as SI.
- A case with `units.system: SI` runs as SI.
- A case with `units.system: CGS` fails before allocation.
- A case with `units.length: mm` fails before allocation.
- The run manifest records `units.system: SI`.
- No unit string or conversion factor appears in a Kokkos kernel signature.

## References

- BIPM, *SI Brochure*, 9th edition.
- `mp-units` documentation (future extension, not v1.0).
