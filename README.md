# saturnCFD

Block-structured AMR incompressible CFD, performance portable via Kokkos.

**Status: v0.1 in development.** The parallel substrate (decomposition, halo
exchange, field container, I/O, configuration) is implemented and tested. The
flow solver is being layered on top.

## Why this exists

Basilisk is the reference for interface-capturing multiphase flow, but it is a
CPU code built on a cell-based quadtree that maps badly onto GPUs. saturnCFD is
an independent implementation with the same *physics lineage* (collocated
variables, face fluxes, Bell–Colella–Glaz projection) but a GPU-native
*architecture*: block-structured AMR with regular patches, Kokkos kernels, and
MPI domain decomposition.

No Basilisk source is used. saturnCFD is Apache-2.0; Basilisk is GPL.

## Design in one paragraph

A level is a set of disjoint rectangular patches (`BoxArray`) distributed over
MPI ranks (`DistributionMapping`). Field data lives in `MultiFab`: one flat
`Kokkos::View` per rank holding only its own patches, with a per-patch offset
table. Kernels iterate patches as Kokkos teams, so access patterns are regular
and coalesced — this is what makes AMR viable on a GPU. Ghost cells are filled
by `HaloExchange`, which resolves every patch-to-patch dependency (including
periodic images, edges and corners) on the host once, then launches one kernel
for all local copies and one packed MPI exchange per peer.

## Building

Requires CMake >= 3.24 and Kokkos (>= 4.0).

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_PREFIX_PATH=/path/to/kokkos
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Options:

| CMake option | Default | Meaning |
|---|---|---|
| `SATURN_NDIM` | `2` | `2` or `3` |
| `SATURN_PRECISION` | `double` | `float` or `double` |
| `SATURN_ENABLE_MPI` | `OFF` | enable MPI decomposition |
| `SATURN_BUILD_TESTS` | `ON` | build `saturn_tests` |

## Running

```bash
./build/saturnCFD apps/sample.yaml
./build/saturnCFD --dump-config          # commented YAML template
./build/saturnCFD --threads 8            # override the Kokkos thread count
```

The driver currently validates the substrate end to end: it initialises a
smooth periodic field analytically, halo-exchanges it, compares every ghost cell
against the analytic value (should be ~1e-16), writes a ParaView collection, and
reports sustained stencil bandwidth.

## Output

`output/field.pvd` is a ParaView collection referencing one `.vti` per patch per
timestep. Open the `.pvd`, not the individual `.vti` files.

## Repository layout

```
src/core    Types, Config (YAML subset), Timer
src/amr     Box, BoxArray, DistributionMapping, MultiFab
src/comm    HaloExchange
src/mesh    Geometry
src/io      VTKWriter
src/solver  (the flow solver — in progress)
apps        driver, sample.yaml
tests       unit tests (no external framework)
docs        SCOPE.md — the frozen deliverable definition
```

## Roadmap

- [x] v0.1 substrate: decomposition, halo exchange, I/O, config, tests
- [ ] Single-phase projection solver (2D/3D), multigrid pressure
- [ ] Two-level AMR: tagging, regridding, prolongation/restriction, reflux
- [ ] Validation: lid-driven cavity (Ghia), Taylor–Green order of accuracy
- [ ] Scaling study on cluster
- [ ] v0.2: cut-cells (GMSH/STL -> volume fractions), multiphase VOF

## License

Apache-2.0. See `LICENSE`.
