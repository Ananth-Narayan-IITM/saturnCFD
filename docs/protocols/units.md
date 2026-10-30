# P1b — Units protocol (SI only in v1.0)

**Status:** accepted (ADR `0008`)

## Purpose

Keep the solver dimensionally correct without putting unit types inside Kokkos
kernels. v1.0 accepts SI only. Non-SI input is rejected at configuration
validation, not converted.

## Policy

- **Internal values:** SI.
- **YAML values:** SI.
- **Accepted declaration:** `units.system: SI`, or no `units:` block (SI is the
  default).
- **Rejected:** any other system or per-quantity unit (`CGS`, `imperial`,
  `length: mm`, `pressure: kPa`, ...).
- **No conversion in v1.0.** The config layer validates dimensions; it does not
  scale values.
- **Future:** a unit table and conversion at the config boundary, never in a
  kernel.

## YAML

```yaml
units:
  system: SI
```

Equivalent:

```yaml
# no units block; SI is the default
```

Rejected with a clear error:

```yaml
units:
  system: CGS
```

```yaml
units:
  length: mm
```

```text
error: units.system: 'CGS' is not supported in v1.0
hint: only SI is implemented; non-SI conversion is a future extension
```

## Dimensions

Even with SI only, the schema records the dimension of every option so that a
future conversion layer has the right metadata and so that obvious mistakes
are caught now.

| quantity | dimension | SI unit |
|---|---|---|
| length | L | m |
| time | T | s |
| mass | M | kg |
| temperature | $\Theta$ | K |
| acceleration | L/T² | m/s² |
| velocity | L/T | m/s |
| pressure | M/(L T²) | Pa |
| density | M/L³ | kg/m³ |
| dynamic viscosity | M/(L T) | Pa·s |
| kinematic viscosity | L²/T | m²/s |
| thermal diffusivity | L²/T | m²/s |
| thermal conductivity | M L/(T³ $\Theta$) | W/(m·K) |
| specific heat | L²/(T² $\Theta$) | J/(kg·K) |
| body force | M/(L² T²) | N/m³ |

`ConfigSchema` entries carry a `Dimension`:

```cpp
schema.addDouble("flow.lidVelocity", 1.0,
                 "lid speed [m/s]", Dimension::Velocity);
schema.addDouble("flow.outletPressure", 0.0,
                 "outlet pressure [Pa]", Dimension::Pressure);
```

A value with the wrong dimension for its key must fail even in SI mode. The
dimension tag is not a unit conversion; it is a correctness check.

## Solver contract

A solver never sees a unit string or a conversion factor. Its configuration
struct contains SI values:

```cpp
struct BurgersCase {
  double viscosity;      // m^2/s
  double inletVelocity;  // m/s
  double endTime;        // s
  double length;         // m
};
```

`PhysicalProperties` is SI. Boundary-condition values are SI. Kernels operate
on SI `Real` values. The only unit-related code is in the config binder and the
run-manifest metadata.

## Output

VTK, CSV and checkpoints store SI values. The run manifest records:

```yaml
units:
  system: SI
```

## Validation

- no `units:` block: accepted, SI;
- `units.system: SI`: accepted;
- `units.system: CGS`: error;
- `units.length: mm`: error (unknown key in v1.0);
- a pressure value assigned to a velocity key: error;
- a unit string in a kernel signature: code review failure.

## Future extension

When non-SI is needed:

1. add a `UnitSystem` enum and a fixed unit table in `src/core/Units.hpp`;
2. add `units.length`, `units.pressure`, ... or per-value suffixes;
3. convert to SI in the config binder;
4. keep the solver contract unchanged (SI `double`).

The `units:` block already exists, so the YAML shape does not have to change.

## Tests

- default SI accepted;
- `system: SI` accepted;
- `system: CGS` rejected;
- per-quantity unit rejected in v1.0;
- wrong dimension rejected;
- manifest records `system: SI`;
- no unit symbols in kernel signatures.
