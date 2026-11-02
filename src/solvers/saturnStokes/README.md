# saturnStokes — 2D/3D steady Stokes solver

**Target:** `saturnStokes`

**Class:** `saturn::SaturnStokes`

**Registered name:** `saturnStokes`

**Equation:** steady incompressible Stokes

$$
-\mu\nabla^2\mathbf{u}+\nabla p=\mathbf{f},
\qquad
\nabla\cdot\mathbf{u}=0.
$$

## Files

```text
src/solvers/saturnStokes/
  CMakeLists.txt       target saturnStokes
  SaturnStokes.hpp     options, solver class, profile/error diagnostics
  SaturnStokes.cpp     DMDA, coupled matrix assembly, PETSc solve, gather
  StokesCase.hpp       YAML schema, case factory and registration
  StokesCase.cpp       Poiseuille/Couette/MMS data and registry entry
  README.md            this page
```

## Method

- Collocated cell-centred `u,v,w,p` on a PETSc `DMDA`.
- Coupled saddle-point matrix:
  $$
  \left[\begin{array}{cc} A & -B^{T}\\ B & C\end{array}\right]
  \left[\begin{array}{cc}\mathbf{u}\\p\end{array}\right]
  =
  \left[\begin{array}{cc}\mathbf{f}\\0\end{array}\right].
  $$
- A Brezzi-Pitkaranta pressure Laplacian `C = beta lap(p)` replaces the raw
  collocated divergence; one pressure reference row fixes the constant gauge.
- **Row scaling:** every equation is assembled per unit volume. Momentum
  diffusion is `mu/h^2`, the pressure gradient and divergence are `0.5/h`, and
  the stabilisation is `beta/h^2` with `beta = stabilization * h_min^2 / mu`.
  This makes the discrete gradient the negative transpose of the divergence,
  keeps `p` physical, and gives a Schur complement of order `1/mu` instead of
  `1/(mu h^3)`.
- **Matrix numbering:** the matrix is inserted with `MatSetValuesStencil`.
  Hand-built PETSc ids from `i + nx*(j + ny*k)` are only correct when the DMDA
  is split along `x`; on `y`/`z` splits they disagree with the DA global
  ordering. The fieldsplit index sets are built from the matrix block owner
  range for the same reason.
- `stokes.linearSolver` defaults to `fieldsplit`, which needs only `O(N)`
  memory: outer FGMRES, `PC_COMPOSITE_SCHUR` with `fact_type full` and
  `selfp`, GMRES + GAMG on the velocity block (`rtol 1e-10`), and
  preonly + GAMG on the pressure block. The outer tolerance is `1e-12`.
  This gives 11-20 outer iterations in 2D and 3-13 in 3D on 1/2/4/8 ranks.
  `direct` selects PETSc `preonly` + `lu` (SuperLU_DIST when available; MUMPS
  can report a false zero pivot on small multi-periodic saddle-point systems)
  and is retained for validation or users with enough memory.
- 2D cases use the 3D core with `nz = 1`; `w` is constrained to zero.
- 2D runs on 1/2/4/8 ranks and 3D on 1/2/4 ranks; the 3D tests no longer skip
  multi-rank runs. A fieldsplit-vs-direct test guards the default solver.

## Cases

`stokes.case`:

- `poiseuille` — body force `forceX = G`, exact
  $u=G/(2\mu)y(H-y)$;
- `couette` — moving top wall `wallVelocityX = U`, exact $u=Uy/H$;
- `mms` — source-consistent manufactured solution. `stokes.mmsWall: false`
  (default) is fully periodic; `true` selects a wall-bounded variant
  (experimental: the velocity order is correct, the wall pressure order is
  still being investigated).

The core requires periodic `x` and `z`; `y` is either a wall pair or
periodic (`stokes.periodicY` for MMS).

## Running

```bash
./buildCPU/saturnCFD examples/saturnStokes/saturnStokes2D/poiseuille.yaml
./buildCPU/saturnCFD examples/saturnStokes/saturnStokes3D/poiseuille.yaml
```

Output is CSV (`x,y,z,u,v,w,p,u_exact,v_exact,w_exact,p_exact,error`) and JSON
(iterations, residual, exact norms) under `output.prefix`. When
`output.vtkEvery > 0`, one `.vtu` file contains all four cell arrays `u`, `v`,
`w`, `p` for ParaView.

## MMS

The `mms` case supplies the momentum source
`f = -mu lap(u_m) + grad(p_m)` and the continuity source
`g = div(u_m) - beta lap(p_m)`. Both manufactured velocities are
divergence-free, so `g = -beta lap(p_m)`. The pressure is shifted so the
reference cell has `p = 0`; a fully periodic problem is still defined only up
to a constant velocity, so the order test removes the mean velocity error.

The fully periodic case is run with the direct solver; the fieldsplit pressure
gauge is not yet configured for its nullspace. Measured periodic orders:

| case | meshes | velocity order | pressure order |
|---|---|---|---|
| 2D | 16$^2$ -> 32$^2$ | 2.01 | 1.89 |
| 3D | 8$^3$ -> 16$^3$ | 2.03 | 1.71 |

## Tests

- `tests/TestSaturnStokes.cpp` — 2D Poiseuille order 2, 2D Couette exact,
  3D Poiseuille, 3D Couette, fieldsplit-vs-direct, periodic MMS order, and a
  finite-difference check of the MMS sources.
- `tests/CMakeLists.txt` — `stokes_np1/2/4` under the `solver` label.
- Both 2D and 3D run on 1/2/4 ranks; 2D is also run on 8 ranks.

## Examples and validation

```text
examples/saturnStokes/saturnStokes2D/
examples/saturnStokes/saturnStokes3D/
```

Each has `poiseuille.yaml`, `couette.yaml`, `compareAnalytic.py` and a README.
Validation record: `docs/validation/stokes.md`.
Theory: `docs/theory/stokes.md` and `docs/theory/coupledSaddlePoint.md`.
