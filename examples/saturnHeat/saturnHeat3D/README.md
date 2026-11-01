# saturnHeat 3D standalone example

This is the 3D validation of `saturnHeat`: a periodic Fourier mode of the
scalar convection–diffusion equation, computed on a three-dimensional box and
compared with the exact solution.

## Equation

$$
\frac{\partial T}{\partial t} + \nabla\cdot(\mathbf{a}T)
= \alpha\nabla^2 T + S,
$$

with prescribed uniform velocity \(\mathbf{a}\).

## Exact solution

For constant \(\mathbf{a}\), \(\alpha\) and \(S=0\) on a periodic box,

$$
T(\mathbf{x},t)
= A\sin\!\left(\mathbf{k}\cdot\mathbf{x} - (\mathbf{a}\cdot\mathbf{k})t\right)
\exp\!\left(-\alpha|\mathbf{k}|^2 t\right)
$$

satisfies the equation exactly. This is the cleanest 3D test of the combined
convective and diffusive operators: the convection scheme controls the phase
error and the Laplacian controls the amplitude error.

This example uses

| parameter | value |
|---|---|
| domain | \([0,1]^3\), periodic |
| cells | `nx = ny = nz = 48` |
| velocity | \(\mathbf{a} = (1,\;0.5,\;0.25)\) |
| diffusivity | \(\alpha = 0.01\) |
| wave vector | \(\mathbf{k} = 2\pi(1,1,1)\) |
| amplitude | \(A = 1\) |
| end time | \(t = 0.1\) s |
| convection | MUSCL, van Leer |
| time | SSPRK3, CFL 0.3 |

At \(t=0.1\) the decay factor is
\(\exp(-0.01\cdot 12\pi^2\cdot 0.1)=0.888\).

## Run

```bash
cd examples/saturnHeat/saturnHeat3D
saturnCFD saturnHeat3D.yaml
python3 compareAnalytic.py
```

`compareAnalytic.py` recomputes the analytic field from the YAML parameters
and compares it with the CSV. The default tolerance is \(5\times10^{-3}\):

```bash
python3 compareAnalytic.py --cli /path/to/saturnCFD
python3 compareAnalytic.py --no-run
python3 compareAnalytic.py --tolerance 5e-3
```

## Expected result

CPU build, 48 x 48 x 48 cells, \(t=0.1\) s:

```text
L1 error    : 1.183809e-03
L2 error    : 1.478989e-03
Linf error  : 4.082673e-03
PASS: L2 1.478989e-03 <= tolerance 5.0e-03
```

Convergence measured by the CTest case at fixed \(t=0.1\) s:

| \(N^3\) | \(L^2\) | order |
|---:|---:|---:|
| 16 | 1.932e-2 | — |
| 32 | 3.973e-3 | 2.28 |
| 48 | 1.479e-3 | 2.44 |

## Plot

`comparison.png` / `comparison.pdf` contain four panels:

1. the `saturnHeat` field on the plane nearest \(z=0.5\),
2. the independent analytic field on the same plane,
3. the pointwise error on that plane,
4. the diagonal cut \(x=y=z\).

The plane and diagonal views show both the travelling wave and the decayed
amplitude.

## Where the results are stored

Running from this directory writes:

```text
results/saturnHeat3D/fourier.csv
results/saturnHeat3D/fourier.json
results/saturnHeat3D/fourier.pvd
results/saturnHeat3D/fourier_*.vtu
results/saturnHeat3D/comparison.png
results/saturnHeat3D/comparison.pdf
```

The CSV is the final-time field, one row per cell:

```text
x,y,z,u_numerical,u_exact,error
```

`vtkEvery: 25` writes `.vtu` frames and a `.pvd` collection for ParaView.

## Threads

```bash
OMP_NUM_THREADS=8 saturnCFD saturnHeat3D.yaml
mpirun -np 4 saturnCFD saturnHeat3D.yaml
```

The same case is run on 1/2/4 ranks by the `heat_fourier32_np*` CTest targets.

## Files

```text
saturnHeat3D.yaml      case
compareAnalytic.py     independent analytic comparison and plot
README.md              this page
```
