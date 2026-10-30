# fv

Shared finite-volume operators. Every solver uses this layer; none of them
reimplements a gradient or a flux.

## Implemented

- `CellField` — ghosted cell-centred Kokkos view with `ncomp` components and
  `nGrow` ghost layers. Global `(i,j,k,c)` indexing matches `MeshPartition` and
  `HaloExchange`.
- `FaceField` — six face values per owned cell, `(i,j,k,dir,c)`. Internal faces
  are stored from both sides; this is the cell-local representation the
  gradient and divergence loops need.
- `LocalMesh` — structured local geometry extracted from the global hex mesh:
  cell volume/centroid, outward face area/normal/centroid, centroid-to-face
  distance and boundary patch index for the six faces of every owned cell.
  Device views plus host mirrors.
- `FieldHalo` — Kokkos halo adapter: device pack, host mirror, MPI
  `Isend`/`Irecv`, device unpack, plus local periodic copies. Buffers are
  allocated once, not inside a time loop.
- `fillBoundaryGhosts` — physical Dirichlet/Neumann ghost fill after the
  periodic/partition halo. Periodic directions and inactive 2D directions are
  skipped.
- `interpolate` — linear cell-to-face interpolation.
- `gradient` — Gauss gradient, `(1/V) sum_f phi_f A_f n_f`.
- `divergence` — `(1/V) sum_f u_f . n_f A_f`.
- `CartesianGeometry` + `laplacian` — uniform-block orthogonal seven-point
  operator, kept for the periodic Laplacian MMS.

## Current limitations

- `LocalMesh` supports structured hexahedral meshes only.
- Interpolation uses centroid distances when both cells are owned; faces to a
  ghost use equal weights. That is exact on a uniform mesh and for the mirror
  ghosts used by Dirichlet/Neumann fill. A general non-uniform partition-
  boundary weight is a later refinement.
- Boundary fill runs through host mirrors in this CPU-first slice. The device
  path is the same API but not yet optimized.
- Corner ghosts (two/three boundary directions) are not filled; current
  operators read face-neighbour ghosts only.
- `FaceField` is temporary/workspace storage. A persistent face-flux field for
  pressure correction comes later.

## Next

- Non-orthogonal Laplacian correction on general hexahedra.
- Upwind/Rusanov and MUSCL/TVD convective fluxes.
- Rhie–Chow face flux and a persistent `FaceField`.
- Boundary-condition YAML schema and patch selection.

## Boundaries

- No solver lifecycle, no YAML parsing, no file I/O.
- Operators take fields and geometry; no hidden allocation in a time loop.
- No unit conversion: fields are SI.

## Tests

- `tests/TestFv.cpp` — CellField fill/reduce, Kokkos halo on two periodic ghost
  layers, 2D/3D manufactured Laplacian order, CPU reference norm, CUDA cases.
- `tests/TestLocalMesh.cpp` — LocalMesh geometry, linear interpolation,
  Dirichlet and Neumann ghost fill, gradient/divergence exactness on 1/2/4
  ranks, and periodic gradient/divergence MMS.
- `ctest --preset fv` runs the FV binary on 1, 2 and 4 ranks.

## Docs

- `docs/protocols/finiteVolume.md`
- `docs/theory/finiteVolume.md`
- `docs/theory/manufacturedSolutions.md`
- `docs/plan/fvmKokkosPlan.md`
- ADR `0016`
