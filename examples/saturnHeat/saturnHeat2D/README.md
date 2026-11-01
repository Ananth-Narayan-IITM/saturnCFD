# saturnHeat 2D standalone example

This example validates the `saturnHeat` scalar convection–diffusion solver in
two dimensions against the analytic Fourier solution.

## Equation

`saturnHeat` advances

$$
\frac{\partial T}{\partial t} + \nabla\cdot(\mathbf{a}T)
= \alpha\nabla^2 T + S,
$$

with a prescribed uniform velocity \(\mathbf{a}\). For a divergence-free
constant velocity this is the advection–diffusion equation

$$
\frac{\partial T}{\partial t} + \mathbf{a}\cdot\nabla T
= \alpha\nabla^2 T + S .
$$

## Exact solution

On a periodic box with constant \(\mathbf{a}\), \(\alpha\) and \(S=0\), a
single Fourier mode is exact:

$$
T(\mathbf{x},t)
= A\sin\!\left(\mathbf{k}\cdot\mathbf{x} - (\mathbf{a}\cdot\mathbf{k})t\right)
\exp\!\left(-\alpha|\mathbf{k}|^2 t\right).
$$

The phase travels at the projected velocity \(\mathbf{a}\cdot\hat{\mathbf{k}}\)
and the amplitude decays at the diffusive rate \(\alpha|\mathbf{k}|^2\).

This example uses

| parameter | value |
|---|---|
| domain | \([0,1]^2\), periodic |
| cells | `nx = ny = 128`, `nz = 1` |
| velocity | \(\mathbf{a} = (1,\;0.5,\;0)\) |
| diffusivity | \(\alpha = 0.01\) |
| wave vector | \(\mathbf{k} = 2\pi(1,1,0)\) |
| amplitude | \(A = 1\) |
| end time | \(t = 0.2\) s |
| convection | MUSCL, van Leer |
| time | SSPRK3, CFL 0.3 |

At \(t=0.2\) the decay factor is
\(\exp(-0.01\cdot 8\pi^2\cdot 0.2)=0.855\) and the phase shift is
\((\mathbf{a}\cdot\mathbf{k})t=1.88\) rad.

## Run

```bash
cd examples/saturnHeat/saturnHeat2D
saturnCFD saturnHeat2D.yaml
python3 compareAnalytic.py
```

`compareAnalytic.py` runs the case itself by default and recomputes the
analytic solution from the YAML parameters, independently of the solver's
`u_exact` column. Options:

```bash
python3 compareAnalytic.py --cli /path/to/saturnCFD
python3 compareAnalytic.py --no-run
python3 compareAnalytic.py --tolerance 2e-3
```

## Expected result

CPU build, 128 x 128 cells, \(t=0.2\) s:

```text
L1 error    : 2.227586e-04
L2 error    : 2.424595e-04
Linf error  : 4.129531e-04
max |u_exact column - independent analytic| : 0.000e+00
PASS: L2 2.424595e-04 <= tolerance 2.0e-03
```

The observed convergence at fixed \(t=0.2\) s is:

| \(N\times N\) | \(L^2\) | order |
|---:|---:|---:|
| 32 x 32 | 5.733e-3 | — |
| 64 x 64 | 1.112e-3 | 2.37 |
| 128 x 128 | 2.425e-4 | 2.20 |

The `TestSaturnHeat.cpp` case in the CTest suite measures second-order
convergence in 3D for the same operator.

## Plot

`comparison.png` / `comparison.pdf` contain four panels:

1. the `saturnHeat` field,
2. the independent analytic field,
3. the pointwise error,
4. a cut at \(y=0.5\) comparing the numerical and analytic waves.

## Where the results are stored

Running from this directory writes:

```text
results/saturnHeat2D/fourier.csv
results/saturnHeat2D/fourier.json
results/saturnHeat2D/fourier.pvd
results/saturnHeat2D/fourier_*.vtu
results/saturnHeat2D/comparison.png
results/saturnHeat2D/comparison.pdf
```

The CSV is the final-time field, one row per cell:

```text
x,y,u_numerical,u_exact,error
```

`vtkEvery: 50` also writes `.vtu` frames and a `.pvd` ParaView collection.
`vtkEvery: 0` disables VTK output.

## Files

```text
saturnHeat2D.yaml      case
compareAnalytic.py     independent analytic comparison and plot
README.md              this page
```
