# saturnStokes 3D validation

Independent 3D examples for the steady incompressible Stokes solver. The
channel is extruded along \(z\) and periodic in \(x\) and \(z\), so the 3D
coupled `u,v,w,p` system is exercised while the exact solution remains a 1D
profile in \(y\).

$$
-\mu\nabla^2\mathbf{u}+\nabla p=\mathbf{f},
\qquad
\nabla\cdot\mathbf{u}=0.
$$

## Cases

| file | exact solution | driving |
|---|---|---|
| `poiseuille.yaml` | \(u(y)=\dfrac{G}{2\mu}y(H-y)\), \(v=w=0\), \(p=\text{const}\) | body force \(f_x=G\) |
| `couette.yaml` | \(u(y)=Uy/H\), \(v=w=0\), \(p=\text{const}\) | moving upper wall \(U\) |

Default parameters: \(\mu=1\), \(G=1\) or \(U=1\), \(H=1\), \(24\times24\times8\)
cells.

## Run

```bash
cd examples/saturnStokes/saturnStokes3D
saturnCFD poiseuille.yaml
python3 compareAnalytic.py --case poiseuille

saturnCFD couette.yaml
python3 compareAnalytic.py --case couette
```

The script recomputes the analytic profile from the YAML, independent of the
solver's exact column, validates both cases, and plots the 3D velocity
surfaces of the numerical and analytic solution plus the pointwise error.

## Expected result

CPU, 24 x 24 x 8, direct coupled PETSc solve:

```text
Poiseuille: L2 ≈ 2.17e-04, Linf ≈ 2.17e-04
Couette   : L2 ≈ 5.7e-12,  Linf ≈ 6.6e-11
```

## Parallel note

The 3D core is validated on 1/2/4 ranks. Matrix entries are inserted with
`MatSetValuesStencil`, so PETSc's DMDA global ordering is respected on any
`x`, `y` or `z` split; the previous hand-built id formula only worked when the
DA was split along `x`. The CTest `stokes_np1/2/4/8` runs both 2D and 3D; the 2D cases also run on 8 ranks.

## Files

```text
poiseuille.yaml      extruded plane Poiseuille case
couette.yaml         extruded plane Couette case
compareAnalytic.py   independent analytic comparison and plot
README.md            this page
```

## Note

The validation core uses PETSc's coupled direct LU solve
(`stokes.linearSolver: direct`). The fieldsplit/Schur path is available as
`stokes.linearSolver: fieldsplit` and is still being tuned for production
meshes.

## VTK output

With `output.vtkEvery: 1` the steady solver writes one `prefix.vtu` containing
all four cell arrays `u`, `v`, `w` and `p`. Open it directly in ParaView; no
per-variable files are created.

## Solver comparison

`compareSolvers.py` runs the same case with the direct LU and fieldsplit
solvers and reports wall time, peak process-tree memory, the analytic error
and the fieldsplit-vs-direct solution difference:

```bash
python3 compareSolvers.py --cli ../../../buildCPU/saturnCFD
```

The results and interpretation are in
[`docs/validation/fieldsplit.md`](../../../docs/validation/fieldsplit.md).

## Manufactured solution

`mms_wall.yaml` runs the wall-bounded manufactured solution (no-slip walls at
`y = 0, 1`, periodic in the other directions). The `mms` case supplies both
the momentum source and the continuity source.

```bash
saturnCFD mms_wall.yaml
```

3D wall MMS (16^3): velocity order ~2; the pressure order is measured at 1.53 on 24^3 -> 32^3.
