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

- `--list-solvers`
- `--dump-config <solver>` — generate a commented template from the schema.
- `--dump-config <solver> --mesh <file>` — include actual patch names.
- `--check-config <case.yaml>` — run all stages and exit.
- `--check-mesh <file.msh>` — mesh-only check.

## Validation stages

1. **Syntax** — YAML parses; duplicate keys rejected; line numbers kept.
2. **Schema** — required keys present; unknown keys rejected; type, range,
   enum and vector length correct.
3. **Cross-field** — solver/field compatibility; CFL and time step; dimension;
   required physical properties.
4. **Mesh** — file exists, parses, `checkMesh` passes.
5. **Boundary** — every mesh patch has a BC; every BC patch exists.
6. **BC semantics** — type valid for the field; value shape matches the
   dimension; no `fixedValue` on an `empty` patch.
7. **Initial conditions** — shape, dimension and range valid.
8. **Linear solver** — PETSc available; method/preconditioner names valid.
9. **Output** — prefix directory writable.

A failure prints the key, the file/line where possible, the expected value and
the actual value, and exits non-zero.

## Invariants

- The template is generated from the same registration used to read a case.
- No solver reads a key that is not in the schema.
- No default is duplicated in two places.
- A case that passes `--check-config` cannot fail later because of config
  semantics.

## Host/device rules

Config parsing and validation are host-only and happen before
`Kokkos::initialize` or before any device allocation.

## Tests

- round-trip: `--dump-config` then `--check-config`;
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
