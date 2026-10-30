# P5 — Configuration protocol

**Status:** accepted (ADR `0006`)

## Purpose

Make YAML the only user interface for a case, generate the template from the
solver code, and reject invalid input before any field is allocated.

## Vocabulary

- **Key** — a dotted path such as `mesh.file`.
- **Schema** — the registry of known keys, types, defaults, descriptions,
  units, allowed ranges and validators.
- **Case** — a YAML file with `solver:` and the solver's groups.
- **Stage** — one of the ordered validation passes below.

## Interface

```cpp
class ConfigSchema {
public:
  void addString(std::string key, std::string def, std::string desc);
  void addInt(std::string key, int def, std::string desc, Range range);
  void addDouble(std::string key, double def, std::string desc, Range range);
  void addBool(std::string key, bool def, std::string desc);
  void addEnum(std::string key, std::string def, std::vector<std::string>);
  void addVec3(std::string key, Vec3 def, std::string desc, std::string unit);
  void require(std::string key);
};
```

Each solver implements `registerOptions(ConfigSchema&)`. Common modules
(mesh, time, linear solver, output) register their own groups.

## Commands

- `-help [option]` — general or per-option help.
- `-listSolvers`
- `-dumpConfig <solver>` — generate a commented template from the schema.
- `-dumpConfig <solver> -mesh <file>` — include actual patch names.
- `-checkConfig <case.yaml>` — run all stages and exit.
- `-checkMesh <file.msh>` — mesh-only check, with optional `-report` and
  `-writeVtk`.

## Validation stages

1. **Syntax** — YAML parses; duplicate keys rejected; line numbers kept.
2. **Schema** — required keys present; unknown keys rejected; type, range,
   enum and vector length correct.
3. **Units** — the `units:` block parses, every unit has the expected
   dimension, and every dimensioned value is converted to SI.
4. **Cross-field** — solver/field compatibility; CFL and time step; dimension;
   required physical properties.
5. **Mesh** — file exists, parses, `checkMesh` passes.
6. **Boundary** — every mesh patch has a BC; every BC patch exists.
7. **BC semantics** — type valid for the field; value shape matches the
   dimension; no `fixedValue` on an `empty` patch.
8. **Initial conditions** — shape, dimension and range valid.
9. **Linear solver** — PETSc available; method/preconditioner names valid.
10. **Output** — prefix directory writable.

A failure prints the key, the file/line where possible, the expected value and
the actual value, and exits non-zero.

## Units block

```yaml
units:
  length: m
  time: s
  velocity: m/s
  pressure: Pa
  density: kg/m^3
  dynamicViscosity: Pa.s
  kinematicViscosity: m^2/s
  temperature: K
```

All entries are optional and default to SI. Values are converted once, at
binding; the solver sees SI `double` only. A generic scalar declares its unit
in `fields.<name>.unit`. See `docs/protocols/units.md`.

A complete fragment:

```yaml
solver: burgers

units:
  length: mm
  time: s
  velocity: m/s
  dynamicViscosity: mPa.s

mesh:
  file: meshes/channel.msh
  dimension: auto

fields:
  u:
    unit: m/s
    initial:
      type: constant
      value: 1.0

boundary:
  inlet:
    type: fixedValue
    value: 1.0
  outlet:
    type: zeroGradient
  walls:
    type: zeroGradient
  frontAndBack:
    type: empty
```

The mesh coordinates are interpreted in mm and stored in m; the velocity and
the inlet value stay in m/s; the viscosity is converted from mPa·s to Pa·s.

## Invariants

- The template is generated from the same registration used to read a case.
- No solver reads a key that is not in the schema.
- No default is duplicated in two places.
- A case that passes `-checkConfig` cannot fail later because of config
  semantics.

## Host/device rules

Config parsing and validation are host-only and happen before
`Kokkos::initialize` or before any device allocation.

## Tests

- round-trip: `-dumpConfig` then `-checkConfig`;
- unknown key;
- missing required key;
- wrong type;
- out-of-range value;
- BC for a missing patch;
- missing BC for an existing patch;
- scalar value for a vector field.

## References

- YAML 1.2 specification.
- OpenFOAM dictionary validation (methodology reference).
