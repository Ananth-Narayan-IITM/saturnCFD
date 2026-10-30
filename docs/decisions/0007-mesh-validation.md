# 0007 — Mesh validation and the `checkMesh` module

- **Status:** accepted
- **Date:** 2026-10-30
- **Owner:** saturnCFD v1
- **Supersedes:** none
- **Protocol:** `docs/protocols/mesh.md`
- **Tests:** `tests/mesh/...`

## Context

The solver accepts structured hexahedral `.msh` files only. A mesh that looks
plausible but has a non-manifold face, a negative volume, a wrong orientation
or a non-contiguous index space will produce silent wrong answers. The mesh
must be validated independently of the solver and the YAML, and the report must
be useful enough to fix the mesh.

## Decision

Implement a `checkMesh` module, callable as:

```bash
saturnCFD --check-mesh mesh.msh
saturnCFD --check-mesh mesh.msh --json report.json
```

and always run automatically before a solver starts.

### Structured detection

1. Parse the general `Mesh`: points, cells, faces, patches.
2. Require every volume cell to be a hexahedron (8 nodes, 6 faces).
3. Build unique faces from sorted node ids. Each face must belong to one
   boundary cell or two volume cells; more than two is a non-manifold error.
4. For each cell, pair its six faces into three opposite pairs.
5. Choose a seed cell and assign \((0,0,0)\). Walk face neighbours, assigning
   \(\pm e_i\), and propagate indices by BFS/DFS.
6. Reject any conflict: two paths assigning different indices to one cell.
7. After the walk, require the index set to be exactly
   \([0,n_x-1]\times[0,n_y-1]\times[0,n_z-1]\) and every internal face to
   connect cells differing by exactly one in exactly one index.
8. On success, build the `StructuredMesh` view: `(i,j,k) -> cellId`,
   `cellId -> (i,j,k)`, neighbour tables and face tables.

### Report

The text report contains:

```text
mesh            : channel.msh
format          : Gmsh 2.2 ASCII
cells           : 4096 (hexahedra)
points          : 4913
faces           : 12672 (internal 11520, boundary 1152)
patches         : inlet(64), outlet(64), walls(512), frontAndBack(512)
bounding box    : [0, 2] x [0, 1] x [0, 0.05]
volume          : min 1.2e-6  max 2.0e-6  total 6.4e-3
face area       : min 2.0e-4  max 5.0e-4
non-orthogonality: max 18.3 deg  average 4.1 deg
skewness        : max 0.21  average 0.04
aspect ratio    : max 4.0  average 1.6
structured      : yes
dimension       : 3D (nx=16, ny=16, nz=16)
errors          : 0
warnings        : 0
status          : PASS
```

Errors (non-zero exit): unsupported element type, non-manifold face, negative
or zero volume, open boundary, duplicate face, inconsistent orientation,
non-contiguous index space, disconnected components.

Warnings (zero exit): high non-orthogonality, high skewness, large aspect
ratio, large volume ratio, very small cells.

### 2D detection

If exactly one index direction has extent 1 and the corresponding boundary
patches are `empty`, report 2D and deactivate that direction. If two
directions have extent 1, report 1D (allowed for scalar validation). Otherwise
report 3D.

## Alternatives considered

| alternative | why not chosen |
|---|---|
| Trust the mesh reader | silent wrong answers; mesh errors are the most common source of CFD failure |
| Validate inside each solver | duplicated logic and inconsistent thresholds |
| Use an external checker | no dependency gives the same structured-index and patch checks |
| Accept unstructured and reject later | the solver cannot use unstructured cells; fail early |

## Consequences

- **Code:** `src/mesh/CheckMesh.{hpp,cpp}`, `MeshReport`, `StructuredMesh`.
- **CLI:** `--check-mesh`, optional `--json`.
- **Validation:** every integration test starts by asserting `checkMesh` passes.
- **Risk:** the BFS classifier must handle curvilinear/stretched hexahedra and
  periodicity; periodic patches are treated as boundary patches for now.

## Validation / exit criteria

- A uniform hex box passes and reports the correct index range.
- A stretched hex box passes and reports correct volumes/areas.
- A tetrahedral mesh fails with "unsupported element type".
- A hex mesh with one flipped face fails with an orientation error.
- A one-cell-thick hex mesh with `empty` patches reports 2D.

## References

- Geuzaine & Remacle (2009) — Gmsh.
- OpenFOAM `checkMesh` report structure (methodology reference).
