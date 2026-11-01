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
  \left[\begin{array}{cc} A & B^{T}\\ B & 0\end{array}\right]
  \left[\begin{array}{cc}\mathbf{u}\\p\end{array}\right]
  =
  \left[\begin{array}{cc}\mathbf{f}\\0\end{array}\right].
  $$
- A compact pressure stabilisation replaces the raw collocated divergence;
  one pressure reference row fixes the constant gauge.
- The validation core uses `stokes.linearSolver: direct` (PETSc
  `preonly` + `lu`). The `fieldsplit` option configures GMRES/FGMRES with a
  Schur fieldsplit but still needs tuning for refined meshes.
- 2D cases use the 3D core with `nz = 1`; `w` is constrained to zero.

## Cases

`stokes.case`:

- `poiseuille` — body force `forceX = G`, exact
  $u=G/(2\mu)y(H-y)$;
- `couette` — moving top wall `wallVelocityX = U`, exact $u=Uy/H$;
- `mms` — periodic manufactured solution (the MMS source-consistent test is
  still being finished).

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

## Tests

- `tests/TestSaturnStokes.cpp` — 2D Poiseuille order 2, 2D Couette exact,
  3D Poiseuille, 3D Couette.
- `tests/CMakeLists.txt` — `stokes_np1/2/4` under the `solver` label.
- 2D runs on 1/2/4 ranks; 3D is serial in the current core.

## Examples and validation

```text
examples/saturnStokes/saturnStokes2D/
examples/saturnStokes/saturnStokes3D/
```

Each has `poiseuille.yaml`, `couette.yaml`, `compareAnalytic.py` and a README.
Validation record: `docs/validation/stokes.md`.
Theory: `docs/theory/stokes.md` and `docs/theory/coupledSaddlePoint.md`.
