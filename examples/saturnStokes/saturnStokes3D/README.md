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
solver's exact column, and plots a midplane field plus the averaged \(u(y)\)
profile.

## Expected result

CPU, 24 x 24 x 8, direct coupled PETSc solve:

```text
Poiseuille: L2 ≈ 2.17e-04, Linf ≈ 2.17e-04
Couette   : L2 ≈ 5.7e-12,  Linf ≈ 6.6e-11
```

## Parallel note

The 3D core is validated on 1 and 2 MPI ranks. The 4-rank 3D DMDA layout is a
known follow-up item; the 2D cases are validated on 1/2/4 ranks. The CTest
`stokes_np4` therefore runs the 2D cases and skips the 3D cases with a message.

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
