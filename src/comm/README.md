# comm

MPI decomposition and ghost exchange. No FVM equations live here.

## Responsibilities

- `MeshPartition` — Cartesian process grid, local index box, periodic
  neighbours.
- `HaloExchange` — 26-direction ghost fill: local copies, packed MPI,
  host-staged buffers.
- MPI runtime helpers (later): local-rank/device mapping, `MPI_Init_thread`.

## Boundaries

- No cell equations, no fluxes, no YAML.
- One owner per cell; ghosts are read-only copies.
- Physical boundaries are not filled here.
- GPU pack/unpack is a later implementation behind the same HaloExchange API.

## Tests

- `tests/TestParallel.cpp` — partition invariants, analytic periodic halo,
  1/2 ghost layers, 2D, non-periodic internal, two components.
- CTest labels: `mpi`.
- `ctest --preset mpi` runs 1, 2 and 4 ranks.

## Docs

- `docs/plan/mpiGpuDesign.md`
- `docs/protocols/parallel.md`
- ADR `0014`
