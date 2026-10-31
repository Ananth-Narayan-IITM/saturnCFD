# P3 — Finite-volume protocol

**Status:** accepted (ADR `0004`)

## Purpose

Define the cell/face data model and the operators every solver uses, so that
Burgers, heat, Stokes and incompressible flow share one discretisation
foundation.

## Fields

- `CellField` — a ghosted Kokkos view with `ncomp` components per cell and
  `nGrow` ghost layers. Index `(i,j,k,c)` is global; the field subtracts its
  local ghost origin. `LayoutLeft` makes `i` contiguous, matching the host
  halo buffers.
- `FaceField` — six face values per owned cell, `(i,j,k,dir,c)`. An internal
  face is stored from both sides; this is the cell-local workspace used by
  interpolation and divergence.
- `VectorCellField` — a `CellField` with three components $(u,v,w)$.
- `BoundaryField` — per-patch values or gradients, implemented as
  `BoundaryCondition` callbacks in `fillBoundaryGhosts`.

All fields are indexed by cell or face id; no hidden `Array4` global-index
arithmetic. Ghost cells are explicit: periodic and partition ghosts are
exchanged by `FieldHalo`, physical ghosts are filled by
`fillBoundaryGhosts`.

`LocalMesh` stores structured cell/face geometry and patch tags. Implemented
operators:

- `interpolate` — linear cell to face;
- `gradient` — Gauss gradient `(1/V) sum_f phi_f A_f n_f`;
- `divergence` — `(1/V) sum_f u_f . n_f A_f`.

The orthogonal seven-point `laplacian` on `CartesianGeometry` remains for the
uniform periodic MMS. Non-orthogonal correction on general hexahedra is the
next operator slice.

## Integral form

For a cell $P$ with volume $V_P$, faces $f$, area $A_f$ and outward
normal $\mathbf{n}_f$:

$$
\frac{d}{dt}\left(V_P \phi_P\right)
+ \sum_f \phi_f\,\mathbf{u}_f\cdot\mathbf{n}_f A_f
= \sum_f \Gamma_f\,\nabla\phi_f\cdot\mathbf{n}_f A_f
+ V_P S_P .
$$

## Operators

| operator | input | output | notes |
|---|---|---|---|
| `gradient` | `CellField` | `CellField` with 3 components | Gauss; implemented |
| `divergence` | `FaceField` (1 or 3 components) | `CellField` | scalar normal flux or vector dotted with the normal; implemented |
| `laplacian` | `CellField` | `CellField` | orthogonal + non-orthogonal correction on `LocalMesh`; implemented |
| `interpolate` | `CellField` | `FaceField` | linear with face weight; implemented |
| `faceNormalVelocity` | `CellField` velocity | `FaceField` | interpolate then dot with the face normal; implemented |
| `convectiveFlux` | normal velocity + cell scalar | `FaceField` | upwind and Rusanov; MUSCL/TVD next |
| `rhieChowFlux` | velocity, pressure | `FaceField` | collocated pressure correction |

## Face interpolation

For a face between $P$ and $N$:

$$
\phi_f = w_P \phi_P + w_N \phi_N,\qquad
w_P = \frac{d_{fN}}{d_{PN}},\quad w_N = 1-w_P.
$$

For non-orthogonal meshes let
$\mathbf{d}=\mathbf{x}_N-\mathbf{x}_P$, $d_\perp=\mathbf{d}\cdot\mathbf{n}_f$
and $\overline{\nabla\phi}_f$ the interpolated cell gradient. The normal
gradient used by `laplacian` is

$$
\nabla\phi_f\cdot\mathbf{n}_f
= \frac{(\phi_N-\phi_P) - \mathbf{d}\cdot\nabla\phi_f^{\,t}}{d_\perp},
\qquad
\nabla\phi_f^{\,t}
= \overline{\nabla\phi}_f
  - \left(\overline{\nabla\phi}_f\cdot\mathbf{n}_f\right)\mathbf{n}_f .
$$

The implemented correction uses one gradient pass; deferred/iterated
correction is a later refinement. On a physical boundary face the
interpolated face value and the owner gradient provide the same correction,
with $d_\perp$ taken from the owner centroid to the face centroid. On a
partition or periodic ghost face the owner gradient is used; that is exact
for a linear field and a documented approximation for a general smooth field.

## Rhie–Chow face flux

The face velocity is not a plain average. It is corrected so that the pressure
gradient in the flux is compact:

$$
\mathbf{u}_f = \overline{\mathbf{u}}_f
+ \Delta t\left(\overline{\nabla p}_f - \nabla p_f\right)
$$

(in the unsteady form; the steady SIMPLE form omits the time factor). The exact
sign and discretisation are derived in `docs/theory/finiteVolume.md` and
validated by the checkerboard test.

## Boundary conditions

Implemented:

- `fixedValue` (Dirichlet, scalar or vector): `fillBoundaryGhosts` mirrors the
  boundary value;
- `zeroGradient` / general Neumann: ghost layer `l` is
  `owner + 2*l*d*g`;
- `noSlip`: a zero Dirichlet condition on all velocity components;
- `empty`: directions beyond `LocalMesh::dimension()` are ignored;
- `periodic`: `FieldHalo` fills the wrapped ghost cells.

Still to wire: YAML patch selection, `symmetry`, and a patch-to-BC schema. The
current tests construct `BoundaryCondition` callbacks directly.

## Parallel invariants

See `docs/protocols/parallel.md` for the full contract. In short:

- each cell is owned by exactly one rank;
- ghost cells are read-only copies with one writer;
- operators iterate owned cells/faces only;
- halos are refreshed before an operator reads them;
- a partition-boundary face is shared, but the cell update is not.

## Invariants

- Face owner/neighbour orientation is consistent.
- A boundary face has exactly one cell; an internal face has two.
- Every operator leaves valid cells unchanged outside its output.
- No operator allocates in a time loop; workspaces are passed in.
- The discrete divergence of a corrected velocity is reported every step.

## Host/device rules

Operators are Kokkos kernels over cell/face ranges. Geometry arrays are
device-resident. No operator parses YAML or touches host objects.

## Tests

- constant field: gradient zero, divergence of a constant flux zero;
- linear field: gradient exact on a uniform hex mesh;
- manufactured Poisson: second-order convergence;
- checkerboard pressure: Rhie–Chow removes it, plain averaging does not;
- boundary-layer profile: heat solver matches the analytic solution.

## References

- Versteeg & Malalasekera, *An Introduction to Computational Fluid Dynamics*.
- Patankar (1980).
- Rhie & Chow (1983).
- Ferziger & Perić (2002).
