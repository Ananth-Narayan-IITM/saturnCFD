# P2 — Mesh protocol (structured hexahedral `.msh`)

**Status:** accepted (ADR `0002`, `0007`)

## Purpose

Define how a Gmsh `.msh` file becomes a validated `StructuredMesh` that the
finite-volume layer can use without re-checking topology.

## Vocabulary

- **Cell** — a hexahedral control volume.
- **Face** — a quadrilateral shared by one or two cells.
- **Patch** — a named set of boundary faces, from a Gmsh physical group.
- **Structured** — every cell has a unique contiguous $(i,j,k)$ and every
  internal face connects cells that differ by exactly one index.
- **Empty patch** — the front/back boundary of a one-cell-thick 2D mesh.

## Input

- Gmsh `.msh` ASCII 2.2 first, 4.1 second.
- Only volume elements of type hexahedron (Gmsh type 5) are accepted.
- Physical groups become patches. A patch name must be unique.
- Coordinates are in metres. There is no `mesh.scale` and no unit conversion in
  v1.0.

## Pipeline

```
.msh -> Mesh (general) -> checkMesh -> StructuredMesh -> solver
```

`Mesh` is the general parsed form: points, cells, faces, patches, boundary
tags. `StructuredMesh` adds:

- `nx`, `ny`, `nz`, `nCells`;
- `cellId(i,j,k)` and `cellIndex(cellId)`;
- neighbour tables `cellNeighbour(cell, dir, side)`;
- face tables `faceId(cell, dir, side)`;
- per-cell volume and centroid; per-face area vector, centroid and owner/neighbour.

The solver never walks a general cell-face list in a hot loop; it uses the
structured neighbour tables.

## Structured detection invariant

1. All volume cells are hexahedra.
2. Every face belongs to one or two cells; never more.
3. Each cell has six faces, paired into three opposite pairs.
4. A BFS from a seed assigns $(0,0,0)$ and propagates $\pm e_i$.
5. No cell receives two different indices.
6. The final index set is exactly a contiguous box.
7. Every internal face changes exactly one index by exactly one.

If any invariant fails, the mesh is rejected as unstructured or malformed.

## Dimension rule

- `nz == 1` (or `nx == 1`, `ny == 1`) with `empty` patches on the two
  corresponding sides: treat as 2D; deactivate that direction.
- Two directions with extent 1: 1D; permitted only for scalar validation.
- Otherwise 3D.
- The mesh is always stored as 3D; `dimension` is a solver view.

## `checkMesh` report

`saturnCFD -checkMesh file.msh [-report report.json] [-writeVtk prefix]` prints:

- file, format, cell/point/face counts, element types;
- patch names and face counts;
- bounding box, volume range/total, face-area range;
- non-orthogonality (max/average), skewness, aspect ratio, volume ratio;
- structured yes/no, index range, dimension;
- errors, warnings, status.

Errors exit non-zero and prevent any solver. Warnings do not.

`-writeVtk prefix` writes `<prefix>.vtu` (volume mesh with cell quality), one
`<prefix>_<patch>.vtp` per boundary patch, `<prefix>.pvd` for ParaView and
`<prefix>.json` with patch names and metrics. Open the `.pvd`.

## Host/device rules

- Parsing, classification and `checkMesh` are host-only.
- The resulting geometry arrays are copied to the device once, before the time
  loop.
- No `.msh` parsing or topology walking inside a Kokkos kernel.

## Error behaviour

- Unsupported element: `MeshError::UnsupportedElement`.
- Non-manifold face: `MeshError::NonManifoldFace`.
- Negative/zero volume: `MeshError::InvalidVolume`.
- Open boundary or inconsistent orientation: `MeshError::InvalidTopology`.
- Non-contiguous indices: `MeshError::NotStructured`.
- Missing/duplicate physical group: `MeshError::InvalidPatch`.

## Tests

- uniform hex box: pass, correct index range;
- stretched hex box: pass, correct volumes/areas;
- tetrahedral mesh: fail with unsupported element;
- one flipped face: fail with orientation;
- one-cell-thick box with `empty`: 2D;
- non-contiguous hex mesh: fail as unstructured;
- `-writeVtk` output contains the volume and every named patch.

## References

- Geuzaine & Remacle (2009).
- OpenFOAM `checkMesh` and `empty` patch conventions.
