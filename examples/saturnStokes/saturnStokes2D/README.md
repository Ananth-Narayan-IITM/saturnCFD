# saturnStokes 2D validation

Independent 2D examples for the steady incompressible Stokes solver:

$$
-\mu\nabla^2\mathbf{u}+\nabla p=\mathbf{f},
\qquad
\nabla\cdot\mathbf{u}=0.
$$

The channel is periodic in \(x\); the two walls are at \(y=0\) and \(y=H\).
The coupled velocity-pressure system is assembled as a saddle point and solved
with PETSc.

## Cases

| file | exact solution | driving |
|---|---|---|
| `poiseuille.yaml` | \(u(y)=\dfrac{G}{2\mu}y(H-y)\), \(v=0\), \(p=\text{const}\) | body force \(f_x=G\) |
| `couette.yaml` | \(u(y)=Uy/H\), \(v=0\), \(p=\text{const}\) | moving upper wall \(U\) |

Default parameters: \(\mu=1\), \(G=1\) or \(U=1\), \(H=1\), \(64\times64\)
cells.

## Run

```bash
cd examples/saturnStokes/saturnStokes2D
saturnCFD poiseuille.yaml
python3 compareAnalytic.py --case poiseuille

saturnCFD couette.yaml
python3 compareAnalytic.py --case couette
```

`compareAnalytic.py` recomputes the analytic profile from the YAML parameters,
independently of the solver's exact column. Run it without `--case` to validate
both cases together and write `results/summary.txt`. Options:

```bash
python3 compareAnalytic.py --case poiseuille --cli /path/to/saturnCFD
python3 compareAnalytic.py --case couette --no-run
python3 compareAnalytic.py --tolerance 1e-5   # both cases
```

## Plot

`comparison_poiseuille.png` / `.pdf` and `comparison_couette.png` / `.pdf`
contain:

1. the numerical \(u\) field on a midplane,
2. the independent analytic field,
3. the pointwise error,
4. the \(u(y)\) profile averaged over the periodic direction.

## Expected result

CPU, 64 x 64, direct coupled PETSc solve:

```text
Poiseuille: L2 ≈ 3.05e-05, Linf ≈ 3.05e-05
Couette   : L2 ≈ 7.5e-13,  Linf ≈ 1.1e-12
```

The Poiseuille error is the expected second-order boundary-closure error; the
3D test in `tests/TestSaturnStokes.cpp` checks its convergence.

## Files

```text
poiseuille.yaml      plane Poiseuille case
couette.yaml         plane Couette case
compareAnalytic.py   independent analytic comparison and plot
README.md            this page
```

## Note

The current validation core uses PETSc's coupled direct LU solve
(`stokes.linearSolver: direct`). The fieldsplit/Schur path is exposed as
`stokes.linearSolver: fieldsplit` but is still being tuned for larger meshes.

## VTK output

With `output.vtkEvery: 1` the steady solver writes one `prefix.vtu` containing
all four cell arrays `u`, `v`, `w` and `p`. Open it directly in ParaView; no
per-variable files are created.
