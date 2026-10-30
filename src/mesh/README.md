# mesh

Read a Gmsh `.msh` file, validate it, and expose a structured view to the
finite-volume layer.

## Responsibilities

- `GmshReader` — Gmsh 2.2 ASCII first, 4.1 next.
- `Mesh` — points, cells, faces, patches, geometry arrays.
- `checkMesh` — topology, volume, orientation, patches, quality metrics.
- `StructuredMesh` — `(i,j,k)` mapping and neighbour tables for the solver.
- `VtuWriter` — default `.vtu`; `-writeVTK` adds per-patch `.vtp` and `.pvd`.
- `MeshReport` — text and JSON report.

## Boundaries

- v1.0 accepts structured hexahedral meshes only.
- Unstructured meshes are detected and rejected, not silently accepted.
- Coordinates are SI metres; no `mesh.scale`.
- No solver state and no equations.

## Tests

- `tests/mesh/` — reader, topology, geometry, structured detection, VTK.
- `tests/regression/mesh/` — accepted/rejected meshes, 2D rule, report keys.

## Docs

- `docs/protocols/mesh.md`
- ADR `0002`, `0007`, `0010`
