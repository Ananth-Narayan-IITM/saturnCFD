# saturnStokes examples

This directory keeps the 2D and 3D steady Stokes examples together.

```text
saturnStokes2D/
  poiseuille.yaml
  couette.yaml
  mmsWall.yaml
saturnStokes3D/
  poiseuille.yaml
  couette.yaml
  mmsWall.yaml
validate.py
results/            generated, organized output
```

`validate.py` is the single validation and solver-comparison driver. It runs
the three cases with direct LU and fieldsplit, measures wall time and
peak process-tree memory, checks the analytic and MMS errors, verifies that
fieldsplit matches direct, and writes the result tree.

```bash
cd examples/saturnStokes
python3 validate.py --dim 2d --cli saturnCPU
python3 validate.py --dim 3d --cli saturnCPU
python3 validate.py --dim both --cli saturnCPU
```

Results:

```text
results/
  summary.txt
  2d/config/  data/  logs/  figures/
  3d/config/  data/  logs/  figures/
```

Each case has a validation figure and a solver-comparison figure; the wall MMS
also has a convergence-order figure. The summary table lists iterations, wall
time, peak RSS, analytic error and the fieldsplit-vs-direct difference.

Options:

```bash
--quick       use the case meshes instead of the larger comparison meshes
--ranks N     run with N MPI ranks (fieldsplit switches to production GAMG/GAMG)
--cli PATH    solver executable (default: saturnCFD)
```
