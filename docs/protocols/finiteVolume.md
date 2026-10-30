# P3 — Finite-volume protocol

**Status:** accepted (ADR `0004`)

## Purpose

Define the cell/face data model and the operators every solver uses, so that
Burgers, heat, Stokes and incompressible flow share one discretisation
foundation.

## Fields

- `CellField<Real>` — `ncomp` values per cell, stored contiguously, cell-major.
- `FaceField<Real>` — values per face; used for fluxes and face gradients.
- `VectorCellField` — a `CellField` with three components $(u,v,w)$.
- `BoundaryField` — per-patch values or gradients, filled before an operator.

All fields are indexed by cell or face id; no hidden `Array4` global-index
arithmetic. Ghost cells/faces are explicit for MPI later.

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
| `gradient` | `CellField` | `VectorCellField` | Gauss or least-squares; boundary values included |
| `divergence` | `FaceField` | `CellField` | sum of face fluxes / $V_P$ |
| `laplacian` | `CellField` | `CellField` | over-relaxed non-orthogonal correction |
| `interpolate` | `CellField` | `FaceField` | linear with face weight |
| `convectiveFlux` | face velocity + face value | `FaceField` | Rusanov/upwind/MUSCL |
| `rhieChowFlux` | velocity, pressure | `FaceField` | collocated pressure correction |

## Face interpolation

For a face between $P$ and $N$:

$$
\phi_f = w_P \phi_P + w_N \phi_N,\qquad
w_P = \frac{d_{fN}}{d_{PN}},\quad w_N = 1-w_P.
$$

For non-orthogonal meshes the face-normal gradient is split into an
orthogonal part and a correction:

$$
\nabla\phi_f\cdot\mathbf{n}_f
= \frac{\phi_N-\phi_P}{d_{PN}}
+ \underbrace{\left(\overline{\nabla\phi}_f - \frac{\phi_N-\phi_P}{d_{PN}}\mathbf{n}_f\right)\cdot\mathbf{n}_f}_{\text{correction}}.
$$

The correction is deferred or iterated; document which.

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

Supported v1.0 types:

- `fixedValue` (Dirichlet, scalar or vector);
- `zeroGradient` (Neumann);
- `noSlip` (vector, zero velocity);
- `symmetry`;
- `empty` (2D front/back);
- `periodic` (mapping built from patch geometry; later).

BCs are read from YAML, validated against mesh patch names, and applied by the
operator before it is used.

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
