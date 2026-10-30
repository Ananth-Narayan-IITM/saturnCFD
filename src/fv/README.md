# fv

Shared finite-volume operators. Every solver uses this layer; none of them
reimplements a gradient or a flux.

## Implemented

- `CellField` — ghosted cell-centred Kokkos view with `ncomp` components and
  `nGrow` ghost layers. Global `(i,j,k,c)` indexing matches `MeshPartition` and
  `HaloExchange`.
- `CartesianGeometry` — uniform Cartesian device geometry: cell centres,
  spacing and the seven-point Laplacian coefficient `1/h_d^2`.
- `FieldHalo` — Kokkos halo adapter: device pack, host mirror, MPI
  `Isend`/`Irecv`, device unpack, plus local periodic copies. Buffers are
  allocated once, not inside a time loop.
- `laplacian` — orthogonal seven-point operator over owned cells; reads ghosts,
  writes owned cells only.

## Next

- `LocalMesh`/`DeviceGeometry` from the Gmsh structured mesh (non-uniform
  hexahedra, face areas/normals/centroids, boundary patches).
- Gradient, divergence and face interpolation.
- Convective fluxes: upwind, Rusanov, then MUSCL/TVD.
- Rhie–Chow face flux and a persistent `FaceField` once pressure correction
  needs it.
- Boundary-condition ghosts (Dirichlet/Neumann) for the MMS cases beyond
  periodic.

## Boundaries

- No solver lifecycle, no YAML parsing, no file I/O.
- Operators take fields and geometry; no hidden allocation in a time loop.
- No unit conversion: fields are SI.

## Tests

- `tests/TestFv.cpp` — CellField fill/reduce, Kokkos halo on two periodic ghost
  layers, 2D/3D manufactured Laplacian order, CPU-reference norm, and
  CUDA-labelled device tests.
- `ctest --preset fv` runs the FV binary on 1, 2 and 4 ranks.
- GPU builds label the same tests `gpu`; `ctest --preset gpu` runs them on the
  device.

## Docs

- `docs/protocols/finiteVolume.md`
- `docs/theory/finiteVolume.md`
- `docs/theory/manufacturedSolutions.md`
- `docs/plan/fvmKokkosPlan.md`
- ADR `0016`
