# saturnStokes 2D validation

Independent 2D examples for the steady incompressible Stokes solver:

$$
-\mu\nabla^2\mathbf{u}+\nabla p=\mathbf{f},
\qquad
\nabla\cdot\mathbf{u}=0.
$$

The channel is periodic in \(x\); the walls are at \(y=0\) and \(y=H\).
The coupled velocity-pressure system is assembled as a saddle point and solved
with PETSc.

## Cases

| file | exact solution | driving |
|---|---|---|
| `poiseuille.yaml` | \(u(y)=\dfrac{G}{2\mu}y(H-y)\), \(v=0\), \(p=0\) | body force \(f_x=G\) |
| `couette.yaml` | \(u(y)=Uy/H\), \(v=0\), \(p=0\) | moving upper wall \(U\) |
| `mmsWall.yaml` | wall-bounded manufactured solution | momentum + continuity sources |

Default parameters: \(\mu=1\), \(G=1\) or \(U=1\), \(H=1\), \(64\times64\)
cells for the analytic cases; the MMS case uses a convergence sequence.

## Run

Standalone cases:

```bash
cd examples/saturnStokes/saturnStokes2D
saturnCPU poiseuille.yaml
saturnCPU couette.yaml
saturnCPU mmsWall.yaml
```

The full validation and solver comparison is one script at the parent level:

```bash
cd examples/saturnStokes
python3 validate.py --dim 2d --cli saturnCPU
```

`validate.py` runs every case with **direct LU** and **fieldsplit**, measures
wall time and peak process-tree memory, checks the analytic error and the
direct-vs-fieldsplit difference, runs the wall-MMS convergence study, and
writes the organized results tree. Useful options:

```bash
python3 validate.py --dim 2d --quick          # 96^2 / 16^2,32^2
python3 validate.py --dim 2d --ranks 4        # production GAMG/GAMG fieldsplit
```

## Results

```text
examples/saturnStokes/results/
  summary.txt
  2d/
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

The validation figures show the numerical field, the analytic field, the
pointwise error and the averaged profile. The solver figures show time,
memory and the fieldsplit-vs-direct solution difference. The MMS figure
shows the wall-bounded convergence order.

Measured on the default meshes:

```text
Poiseuille  L2 = 7.63e-06   direct 4.9 s / 600 MB   fieldsplit 8.3 s / 175 MB
Couette     L2 = 5.1e-12    direct 4.7 s / 600 MB   fieldsplit 8.8 s / 164 MB
wall MMS    velocity order 1.98, pressure order 1.88
```

The 2D fieldsplit comparison uses the serial-tuned ILU/Jacobi configuration.
For `--ranks > 1` the script switches to the production GAMG/GAMG
configuration. The examples use the fieldsplit default; direct remains
available through `stokes.linearSolver: direct`.

## Files

```text
poiseuille.yaml      plane Poiseuille case
couette.yaml         plane Couette case
mmsWall.yaml         wall-bounded manufactured solution
README.md            this page
```

The shared driver is `../validate.py`.

## VTK output

With `output.vtkEvery: 1` the steady solver writes one `prefix.vtu` containing
all four cell arrays `u`, `v`, `w` and `p`. Open it directly in ParaView; no
per-variable files are created.
