# P1 — Units protocol

**Status:** accepted (ADR `0008`)

## Purpose

Keep the whole solver dimensionally correct without putting unit types inside
Kokkos kernels. YAML values are converted to SI once, at configuration binding.
The solver receives SI `double` values only.

## Dimensions

The unit layer tracks four base dimensions:

| symbol | dimension |
|---|---|
| L | length |
| M | mass |
| T | time |
| Θ | temperature |

Derived dimensions:

| quantity | dimension | SI unit |
|---|---|---|
| velocity | L/T | m/s |
| pressure | M/(L T²) | Pa |
| density | M/L³ | kg/m³ |
| dynamic viscosity | M/(L T) | Pa·s |
| kinematic viscosity | L²/T | m²/s |
| thermal diffusivity | L²/T | m²/s |
| thermal conductivity | M L/(T³ Θ) | W/(m·K) |
| specific heat | L²/(T² Θ) | J/(kg·K) |
| body force | M/(L² T²) | N/m³ |

## Supported unit strings

The v1.0 parser accepts an exact string from this table; it does not evaluate
arbitrary unit expressions.

| family | accepted strings |
|---|---|
| length | `m`, `mm`, `cm`, `km` |
| time | `s`, `ms`, `min`, `h` |
| mass | `kg`, `g` |
| temperature | `K` |
| velocity | `m/s`, `mm/s`, `cm/s`, `km/h` |
| pressure | `Pa`, `kPa`, `MPa`, `bar`, `atm` |
| density | `kg/m^3`, `g/cm^3` |
| dynamic viscosity | `Pa.s`, `mPa.s`, `P`, `cP` |
| kinematic viscosity | `m^2/s`, `mm^2/s`, `cSt` |
| thermal diffusivity | `m^2/s`, `mm^2/s` |
| thermal conductivity | `W/(m.K)` |
| specific heat | `J/(kg.K)` |
| body force | `N/m^3` |
| dimensionless | `1` |

The table is a `Unit` record: dimension exponents plus a factor to SI. Adding a
unit is one table entry plus a test.

## YAML block

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

Every entry is optional and defaults to the SI unit shown. `-dumpConfig` emits
the block with SI defaults. The block applies globally to all values of that
family. Entries are independent: `mass: g` and
`density: kg/m^3` mean mass-valued keys are read in grams and density-valued
keys in kg/m³. Set the base and derived entries consistently, or leave the
derived entries at SI.

Example:

```yaml
units:
  length: mm
  pressure: kPa
  dynamicViscosity: mPa.s
```

- A mesh coordinate `1000` is read as `1000 mm` and stored as `1 m`.
- A pressure `101.325` is stored as `101325 Pa`.
- A viscosity `1.0` is stored as `1.0e-3 Pa.s`.

## Field units

A generic scalar may not have an obvious physical unit. Declare it:

```yaml
fields:
  u:
    unit: m/s
    initial:
      type: constant
      value: 1.0
  T:
    unit: K
    initial:
      type: constant
      value: 300.0
  alpha:
    unit: 1          # dimensionless
```

For a vector field, the same unit applies to all three components. The solver
supplies a default for known fields:

| field | default unit |
|---|---|
| velocity | `units.velocity` |
| pressure | `units.pressure` |
| temperature | `units.temperature` |

## Binding contract

`ConfigSchema` entries carry a `Dimension`:

```cpp
schema.addDouble("flow.lidVelocity", 1.0,
                 "lid speed", Dimension::Velocity);
schema.addDouble("flow.pressure", 0.0,
                 "outlet pressure", Dimension::Pressure);
schema.addVec3("fields.u.initial.value", {1,0,0},
               "initial velocity", Dimension::Velocity);
```

The binder:

1. reads the `units:` block;
2. resolves the dimension of the key;
3. converts the YAML number to SI;
4. stores a plain `double` in `PhysicalProperties` or the case struct.

The solver never sees a unit string or a conversion factor.

## Solver contract

A solver function never sees a unit string. Its public configuration struct
contains SI values:

```cpp
struct BurgersCase {
  double viscosity;      // m^2/s, SI
  double inletVelocity;  // m/s, SI
  double endTime;        // s, SI
  double length;         // m, SI
};
```

`PhysicalProperties` is SI. Boundary-condition values are SI. Kernels operate
on SI `Real` values. The only place a unit appears is the config binder and the
run-manifest metadata.

## Validation

- Unknown unit string: error with the accepted strings for that family.
- Wrong dimension: `units.pressure: m/s` is an error.
- Unit on a dimensionless key (`reynolds`, `cfl`): error.
- `fields.<name>.unit` missing for a generic scalar with no default: error.
- `units.length` and `mesh.scale` both present: error; `mesh.scale` is removed.

## Output

VTK, CSV and checkpoints are written in SI. The run manifest records:

```yaml
units:
  length: mm
  pressure: kPa
...
conversions:
  length: 1.0e-3
  pressure: 1.0e3
```

## Tests

- `1000 mm` length equals `1 m`.
- `1 kPa` equals `1000 Pa`.
- `1 mPa.s` equals `1e-3 Pa.s`.
- `1 cSt` equals `1e-6 m²/s`.
- wrong dimension fails;
- unknown unit fails;
- vector components all converted;
- dimensionless key with a unit fails;
- no unit symbols in a kernel signature.

## Future

Per-value suffixes (`1.0 cm`, `1.0e-3 Pa s`) and a `units.system: CGS` preset can
be added later. Internally everything stays SI.
