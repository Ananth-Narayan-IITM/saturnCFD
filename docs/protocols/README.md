# v1 protocols

A **protocol** is a stable contract between two parts of the code. It says what
is guaranteed, not how it is implemented. Protocols are written before physics
so that the same substrate can carry Burgers, heat, Stokes and incompressible
flow without being rewritten for each one.

## Status

| id | protocol | document | status |
|---|---|---|---|
| P0 | documentation and citations | `documentation.md` | accepted |
| P0b | command-line interface and help | `cli.md` | accepted |
| P1 | core types, errors, logging, runtime | (to write) | proposed |
| P1b | SI-only units and dimensional checks | `units.md` | accepted |
| P2 | mesh: structured `.msh`, `checkMesh` | `mesh.md` | accepted |
| P3 | finite volume: fields, operators, Rhie–Chow | `finiteVolume.md` | accepted |
| P4 | parallel distribution and halo | (later) | deferred |
| P5 | configuration and validation | `config.md` | accepted |
| P6 | solver lifecycle and registry | `solver.md` | accepted |
| P6b | user-editable solver authoring | `../guides/writingASolver.md` | accepted |
| P7 | linear algebra (PETSc) | `linearAlgebra.md` | accepted |
| P8 | I/O: VTU, CSV, checkpoint, manifest | (to write) | proposed |
| P9 | testing: unit, MMS, regression, parity | `testing.md` | accepted |
| P10 | physics: Burgers, heat, Stokes, NS | `docs/theory/` | in progress |

## Rules for every protocol

1. Purpose and vocabulary.
2. Interface (C++ signatures, concepts or abstract classes).
3. Invariants checked by tests.
4. Ownership and lifetime.
5. Host/device rules.
6. Error behaviour.
7. Parallel behaviour (local/global, rank ownership).
8. Tests.
9. References.

## Non-negotiable rules

- No physics in a protocol.
- No hidden allocation or host/device copy in a hot path.
- No `#ifdef` for physics. Backend and dimension switches are the only
  compile-time branches allowed in the substrate.
- Every public function is camelCase and documented; every numerical choice
  cites a reference.
- No code from GPL projects. Basilisk, NekRS and Nektar++ are read for
  methodology only.
