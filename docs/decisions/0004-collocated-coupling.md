# 0004 — Collocated state and pressure–velocity coupling

- **Status:** accepted
- **Date:** 2026-10-30
- **Owner:** saturnCFD v1
- **Supersedes:** none
- **Protocol:** `docs/protocols/finiteVolume.md`
- **Tests:** `tests/fv/...`, `tests/solvers/...`

## Context

The mesh is structured hexahedral, but the solver must still choose where the
velocity and pressure unknowns live. A staggered MAC grid gives exact discrete
continuity and no checkerboard mode, but splits the velocity components onto
different face meshes, complicates the vector form and complicates the `.msh`
and 2D/3D handling. A collocated cell-centred grid puts all unknowns at cell
centroids, makes the vector form natural and matches OpenFOAM, Basilisk's
default centred solver and the collocated mixed spaces of Nektar++/NekRS.

## Decision

**Use a collocated, cell-centred finite-volume formulation.**

- Velocity \(\mathbf{u}_P=(u_P,v_P,w_P)\) and dynamic pressure \(p_P\) live at
  the cell centroid.
- Convective and diffusive fluxes are evaluated on faces; face values are
  interpolated from the two adjacent cells.
- Suppress the checkerboard pressure mode with **Rhie–Chow momentum
  interpolation** on the face flux.
- **SIMPLE** for steady Stokes and steady incompressible flow.
- **PISO** for transient incompressible flow.
- The vector momentum equation is assembled and solved as a vector system:
  one operator, three components (block size 3 or multiple right-hand sides).
- A staggered MAC backend is not implemented in v1.0. It may be added later as
  a specialised structured-grid option.

## Alternatives considered

| alternative | why not chosen |
|---|---|
| Staggered MAC | exact continuity and no checkerboard, but splits components, complicates vector form and `.msh`/2D handling |
| Collocated without Rhie–Chow | checkerboard pressure; the discrete gradient/divergence stencil decouples odd/even cells |
| Collocated with BCG projection | Basilisk-style and accurate, but harder to combine with assembled vector implicit solves and PETSc |
| Mixed Taylor–Hood finite elements | stable, but changes the discretisation from finite volume and the `.msh` cell logic |

## Consequences

- **Code:** `CellField<Real>` for pressure and scalars, `VectorCellField` for
  velocity, `FaceField` for fluxes; a Rhie–Chow face-flux routine.
- **Linear algebra:** vector momentum matrix (block 3) and scalar pressure
  matrix; good fit for PETSc.
- **Accuracy:** Rhie–Chow introduces a pressure-gradient correction; it must be
  derived and documented, not treated as a magic interpolation.
- **Risk:** checkerboard can reappear at boundaries or after a bad
  under-relaxation choice; the divergence norm and pressure field are mandatory
  diagnostics.
- **Future:** a MAC option can be added behind the same solver protocol if a
  structured AMR path is introduced.

## Validation / exit criteria

- Plane Poiseuille and Couette profiles match the analytic solution.
- Lid-driven cavity at \(Re\to0\) converges and has no checkerboard pressure.
- The discrete divergence norm is below a stated tolerance after each
  pressure correction.
- The pressure null space is handled explicitly and the mean pressure is
  reported.

## References

- Rhie & Chow (1983).
- Issa (1986) — PISO.
- Patankar & Spalding (1972) — SIMPLE.
- Patankar (1980).
- Basilisk `centered.h` (methodology reference only).
