# saturnStokes 3D validation

Independent 3D examples for the steady incompressible Stokes solver. The
channel is extruded along \(z\) and periodic in \(x\) and \(z\), so the 3D
coupled `u,v,w,p` system is exercised while the analytic solution remains a
1D profile in \(y\).

$$
-\mu\nabla^2\mathbf{u}+\nabla p=\mathbf{f},
\qquad
\nabla\cdot\mathbf{u}=0.
$$

## Cases

| file | exact solution | driving |
|---|---|---|
| `poiseuille.yaml` | \(u(y)=\dfrac{G}{2\mu}y(H-y)\), \(v=w=0\), \(p=0\) | body force \(f_x=G\) |
| `couette.yaml` | \(u(y)=Uy/H\), \(v=w=0\), \(p=0\) | moving upper wall \(U\) |
| `mmsWall.yaml` | wall-bounded manufactured solution | momentum + continuity sources |

Default parameters: \(\mu=1\), \(G=1\) or \(U=1\), \(H=1\), \(24\times24\times8\)
cells for the analytic cases; the MMS case uses a convergence sequence.

## Run

Standalone cases:

```bash
cd examples/saturnStokes/saturnStokes3D
saturnCPU poiseuille.yaml
saturnCPU couette.yaml
saturnCPU mmsWall.yaml
```

The full validation and solver comparison is one script at the parent level:

```bash
cd examples/saturnStokes
python3 validate.py --dim 3d --cli saturnCPU
```

`validate.py` runs every case with **direct LU** and **fieldsplit**, measures
wall time and peak process-tree memory, checks the analytic error and the
direct-vs-fieldsplit difference, runs the wall-MMS convergence study, and
writes the organized results tree. Useful options:

```bash
python3 validate.py --dim 3d --quick          # 24^2 x 8, MMS 8^3/12^3
python3 validate.py --dim 3d --ranks 4        # production GAMG/GAMG fieldsplit
```

## Results

```text
examples/saturnStokes/results/
  summary.txt
  3d/
    config/     generated YAML variants
    data/       CSV/JSON for every run
    logs/       solver logs
    figures/
      poiseuille_validation.png / .pdf
      poiseuille_solvers.png / .pdf
      couette_validation.png / .pdf
      couette_solvers.png / .pdf
      mmsWall_order.png / .pdf
      mmsWall_solvers.png / .pdf
      summary_solvers.png / .pdf
```

The validation figures show the numerical, analytic and error velocity
surfaces at a mid-\(z\) plane. The solver figures show time, memory and the
fieldsplit-vs-direct solution difference. The MMS figure shows the
wall-bounded convergence order.

Measured on the default meshes:

```text
Poiseuille  L2 = 2.17e-04   direct 94.8 s / 1486 MB   fieldsplit 1.7 s / 166 MB
Couette     L2 = 2.7e-12    direct 96.4 s / 1486 MB   fieldsplit 1.9 s / 165 MB
wall MMS    velocity order 1.97, pressure order 1.02 (8^3 -> 16^3)
```

The 3D fieldsplit comparison uses the serial-tuned ILU/Jacobi configuration.
For `--ranks > 1` the script switches to the production GAMG/GAMG
configuration. The examples use the fieldsplit default; direct remains
available through `stokes.linearSolver: direct`.

## Parallel note

The 3D core is validated on 1/2/4 ranks. Matrix entries are inserted with
`MatSetValuesStencil`, so PETSc's DMDA global ordering is respected on any
`x`, `y` or `z` split. The CTest `stokes_np1/2/4/8` runs both 2D and 3D; 2D
also runs on 8 ranks.

## Files

```text
poiseuille.yaml      extruded plane Poiseuille case
couette.yaml         extruded plane Couette case
mmsWall.yaml         wall-bounded manufactured solution
README.md            this page
```

The shared driver is `../validate.py`.

## VTK output

With `output.vtkEvery: 1` the steady solver writes one `prefix.vtu` containing
all four cell arrays `u`, `v`, `w` and `p`. Open it directly in ParaView; no
per-variable files are created.
