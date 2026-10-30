# 0008 — SI units from the start; units parsed at the configuration boundary

- **Status:** accepted
- **Date:** 2026-10-30
- **Owner:** saturnCFD v1
- **Supersedes:** none
- **Protocol:** `docs/protocols/units.md`
- **Tests:** `tests/config/units...`

## Context

The user wants the whole solver to be dimensionally correct, but does not want
a units library inside the numerical kernels. The risk is that raw `Real`
values mix metres, seconds, pascals and hidden dimensionless numbers, and that
adding units later forces every validation case to be re-run.

## Decision

1. **Internal representation is SI.** Every field, property and operator uses
   SI values. There is no dimensionless internal mode.
2. **YAML values are interpreted in the unit declared in a global `units:`
   block.** The block is optional; omitted entries default to SI.
3. **The conversion happens once, at configuration binding.** By the time a
   solver sees a value, it is an SI `double`. No unit string or conversion
   factor enters a Kokkos kernel.
4. **A solver declares the dimension of each option.** The schema entry says
   whether a value is a length, time, velocity, pressure, density, dynamic
   viscosity, kinematic viscosity, temperature, thermal diffusivity, thermal
   conductivity, specific heat, body force or dimensionless.
5. **`fields.<name>.unit` declares the unit of a generic scalar or vector
   field.** The solver supplies a default when the field has an obvious
   physical meaning (velocity, pressure, temperature).
6. **`mesh.scale` is removed.** Mesh coordinates use `units.length`.
7. **Output is SI.** VTK, CSV and checkpoints store SI values. The run manifest
   records the input `units:` block and the conversion factors.
8. **Per-value unit strings** (`1.0 cm`, `1.0e-3 Pa s`) are a later extension,
   not required for v1.0.

## YAML form

```yaml
units:
  length: m
  time: s
  mass: kg
  temperature: K
  velocity: m/s
  pressure: Pa
  density: kg/m^3
  dynamicViscosity: Pa.s
  kinematicViscosity: m^2/s
  thermalDiffusivity: m^2/s
  thermalConductivity: W/(m.K)
  specificHeat: J/(kg.K)
  bodyForce: N/m^3
```

All entries are optional and default to the SI unit shown. `-dumpConfig`
includes the block with the SI defaults, so the user can see and change it. A
case may use, for example:

```yaml
units:
  length: mm
  pressure: kPa
  dynamicViscosity: mPa.s
```

Then every length-valued input (mesh coordinates, geometry) is read in mm,
every pressure in kPa and every dynamic viscosity in mPa·s. The solver receives
metres, pascals and Pa·s.

## Alternatives considered

| alternative | why not chosen |
|---|---|
| Fully dimensionless internally | compact for incompressible benchmarks, but poor fit for compressible/two-phase and hides unit errors |
| Full units library inside kernels | compile-time and GPU cost; no benefit in hot loops |
| Raw `Real` with comments only | exactly the unit confusion the user wants to avoid |
| Per-value unit suffix first | more parser work; the global block is enough for v1.0 |
| `mesh.scale` plus units | two conversion mechanisms; easy to double-convert |

## Consequences

- **Code:** `src/core/Units.hpp` (unit table, dimensions, factors),
  `src/config` (unit-aware binding), `PhysicalProperties` in SI.
- **YAML:** a `units:` block and optional `fields.<name>.unit`.
- **Validation:** every option has a dimension; a pressure in `m/s` fails at
  `-checkConfig`.
- **Solver:** solvers receive SI only. They do not parse or convert units.
- **Risk:** a user may set inconsistent derived units; the config layer
  validates dimensions, not the user's mental model. Documentation gives the
  recommended SI block.

## Validation / exit criteria

- A case with `units.length: mm` and a 1000 mm domain produces the same
  solution as a 1 m case.
- A pressure written in kPa equals the same pressure in Pa after binding.
- A velocity in `m/s` assigned to a pressure key fails validation.
- The run manifest records the units block.
- No unit type appears in a Kokkos kernel signature.

## References

- BIPM, *SI Brochure*, 9th edition.
- `mp-units` documentation (future replacement for the small unit table).
