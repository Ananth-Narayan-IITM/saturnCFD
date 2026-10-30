# 0002 — Mesh strategy: structured hexahedral `.msh`, no AMR in v1.0

- **Status:** accepted
- **Date:** 2026-10-30
- **Owner:** saturnCFD v1
- **Supersedes:** none
- **Protocol:** `docs/protocols/mesh.md`
- **Tests:** `tests/mesh/...`

## Context

The solver must read Gmsh `.msh` files, work in 3D from the start, and treat a
mesh with one cell in the third direction as 2D, like OpenFOAM. The first
release does not attempt unstructured meshes. A structured hexahedral mesh
keeps the finite-volume stencils regular, the linear systems well behaved and
the validation tractable. AMR is deferred until the incompressible
Navier–Stokes solver is stable, and will be developed for two-phase flow.

## Decision

1. **Input format:** Gmsh `.msh`, ASCII 2.2 first, then 4.1. Gmsh can export
   2.2 with `gmsh -format msh2`.
2. **Accepted topology:** logically structured hexahedral meshes only. Every
   cell is a hexahedron; the cell dual graph must be a Cartesian grid with
   contiguous \((i,j,k)\) indices. Curvilinear and stretched hexahedral meshes
   are allowed. Non-hexahedral or non-contiguous meshes are rejected.
3. **Detection:** the reader builds a general `Mesh` (points, cells, faces,
   patches); a classifier then attempts to assign a unique \((i,j,k)\) to every
   cell by walking face neighbours. Success produces a `StructuredMesh`;
   failure is a hard error with a `checkMesh` report explaining why.
4. **2D rule:** a mesh with one cell in one direction and `empty` patches on
   the two corresponding sides is treated as 2D. A one-cell-thick mesh is still
   stored as a 3D mesh; the solver deactivates the empty direction. A
   degenerate 1D structured mesh is permitted for scalar Burgers validation
   only.
5. **No AMR in v1.0.** AMR, two-phase flow and adaptive refinement are
   explicitly deferred until the incompressible solver is validated.
6. **Mandatory validation:** `checkMesh` runs before any solver. It reports
   topology, structuredness, dimension, patch names, volume/area ranges,
   non-orthogonality, skewness, aspect ratio, errors and warnings, and exits
   non-zero on any error.

## Alternatives considered

| alternative | why not chosen |
|---|---|
| Unstructured polyhedral `.msh` | not needed for the first release; much larger mesh/FV/linear-algebra surface |
| Native 2D `.msh` meshes | two code paths; the one-cell 3D rule is OpenFOAM-like and keeps one path |
| AMR from the start | multiplies levels, ownership, halo and linear-solver complexity before the base solver is trusted |
| Cartesian uniform grid only | too restrictive for `.msh`; stretched/curvilinear hexahedra are useful and cheap to support |

## Consequences

- **Code:** a general `Mesh` parser, a `StructuredMesh` classifier/view,
  `CellField` and `FaceField`; v0's `BoxArray`/`MultiFab` are not the core.
- **Performance:** regular \((i,j,k)\) neighbour lookup, compact stencils.
- **Portability:** the same structured kernel works in 1D/2D/3D; no
  unstructured face loops in hot paths.
- **Risk:** `.msh` files that look structured but have hanging nodes or
  inconsistent orientation must be rejected, not silently accepted.
- **Future:** unstructured support and AMR can be added behind the same mesh
  interface later.

## Validation / exit criteria

- `checkMesh` accepts a structured hexahedral box and a stretched hexahedral
  box.
- `checkMesh` rejects a tetrahedral mesh, a non-contiguous hex mesh and a mesh
  with a non-manifold face.
- A one-cell-thick hex mesh with `empty` patches is reported as 2D.
- The Stokes Poiseuille case runs on a structured hex mesh and preserves the
  analytic profile.

## References

- Geuzaine & Remacle (2009) — Gmsh.
- OpenFOAM `checkMesh` and `empty` patch conventions (methodology reference).
