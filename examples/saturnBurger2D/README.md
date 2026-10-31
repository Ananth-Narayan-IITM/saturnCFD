# saturnBurger 2D standalone example

This is a separate 2D example. The 1D example in `examples/saturnBurger/` is
left unchanged.

## Equation

The solver advances the conservative 2D scalar Burgers equation

$$
\frac{\partial u}{\partial t}
+ \frac{\partial}{\partial x}\left(\frac{u^2}{2}\right)
+ \frac{\partial}{\partial y}\left(\frac{u^2}{2}\right)
= \nu\left(\frac{\partial^2 u}{\partial x^2}
        + \frac{\partial^2 u}{\partial y^2}\right).
$$

The flux vector is \(\mathbf{F}(u)=(u^2/2,\,u^2/2)\). This is the natural
multi-dimensional extension of the 1D conservative flux used by
`saturnBurger`.

## Exact solution

A plane wave along the diagonal \(x+y\) is exact:

$$
u(x,y,t)=\frac{U}{2}\left[
1-\tanh\left(\frac{U(x+y-x_0-y_0-Ut)}{4\nu}\right)\right].
$$

The derivation is the same balance as in 1D, except both flux components
contribute. At \(t=0\) the front is the line \(x+y=x_0+y_0\); it moves with
normal speed \(U/\sqrt{2}\) while the parameter \(U\) sets the amplitude and
the front speed in the \(x+y\) coordinate.

## Case

| parameter | value |
|---|---|
| domain | \([-1,1]^2\) |
| cells | `nx = ny = 128`, `nz = 1` |
| viscosity | \(\nu = 0.02\) |
| wave speed | \(U = 1\) |
| initial front | \(x_0=y_0=0\) |
| end time | \(t = 0.5\) s, front at \(x+y=0.5\) |
| convection | MUSCL, van Leer |
| time | SSPRK3, CFL 0.3 |
| boundaries | Dirichlet, exact wave |

## Run

```bash
cd examples/saturnBurger2D
saturnCFD saturnBurger2D.yaml
python3 compareAnalytic.py
```

`compareAnalytic.py` runs the case itself by default and recomputes the
analytic solution from the YAML parameters. Options:

```bash
python3 compareAnalytic.py --cli /path/to/saturnCFD
python3 compareAnalytic.py --no-run
python3 compareAnalytic.py --tolerance 1e-3
```

## Expected result

CPU build, 128 x 128 cells, \(t=0.5\) s:

```text
L1 error    : 2.090090e-04
L2 error    : 5.968173e-04
Linf error  : 8.196225e-03
max |u_exact column - independent analytic| : 0.000e+00
PASS: L2 5.968173e-04 <= tolerance 1.0e-03
```

The convergence measured by the test suite is:

| N x N | \(L^2\) | order |
|---:|---:|---:|
| 32 x 32 | 9.909e-3 | — |
| 64 x 64 | 2.555e-3 | 1.96 |
| 128 x 128 | 5.968e-4 | 2.10 |

The small error spikes at the two corners of the error plot are boundary-corner
effects where the initial diagonal front meets the domain corner; they do not
affect the second-order \(L^2\) convergence.

## Where the results are stored

Running from this directory writes:

```text
results/saturnBurger2D/traveling.csv
results/saturnBurger2D/traveling.json
results/saturnBurger2D/comparison.png
results/saturnBurger2D/comparison.pdf
```

The CSV is the final-time field, one row per cell:

```text
x,y,u_numerical,u_exact,error
```

The JSON contains `wallSeconds`, the state norms and the exact-error norms.

Only the final time is stored. `output.vtkEvery` is reserved and does not
write VTK fields yet.

## Threads

For a single rank the CLI keeps Kokkos' default thread count. To control it:

```bash
OMP_NUM_THREADS=4 saturnCFD saturnBurger2D.yaml
```

For `mpirun -np 4`, the CLI chooses `max(1, cores/ranks)` automatically.

## Files

```text
saturnBurger2D.yaml      case
compareAnalytic.py       independent analytic comparison and plot
README.md                this page
```
