# io

Output and restart. Solvers fill data structures; this module writes them.

## Responsibilities

- VTU volume output and VTP boundary patches for ParaView.
- PVD time-series collection.
- CSV diagnostics.
- Binary checkpoint with metadata.
- Run manifest: git hash, build, PETSc/yaml-cpp versions, mesh hash, case hash.

## Boundaries

- SI values only.
- No solver physics.
- No host access to device memory without an explicit copy.
- ASCII first; binary/compressed output is a later optimisation.

## Tests

- `tests/io/` — file headers, field names, `.pvd` references, checkpoint
  round-trip.

## Docs

- `docs/protocols/cli.md` (`-writeVtk`)
- ADR `0010`
