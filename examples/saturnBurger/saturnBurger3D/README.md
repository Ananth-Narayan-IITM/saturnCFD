# saturnBurger 3D standalone example

## Equation

The solver advances the conservative 3D scalar Burgers equation

$$
\frac{\partial u}{\partial t}
+ \frac{\partial}{\partial x}\left(\frac{u^2}{2}\right)
+ \frac{\partial}{\partial y}\left(\frac{u^2}{2}\right)
+ \frac{\partial}{\partial z}\left(\frac{u^2}{2}\right)
= \nu\left(\frac{\partial^2 u}{\partial x^2}
        + \frac{\partial^2 u}{\partial y^2}
        + \frac{\partial^2 u}{\partial z^2}\right),
$$

with flux vector \(\mathbf{F}(u)=(u^2/2,u^2/2,u^2/2)\).

## Exact solution

The exact diagonal plane wave is

$$
u(x,y,z,t)=\frac{U}{2}\left[
1-\tanh\left(
\frac{U(x+y+z-x_0-y_0-z_0-\frac32Ut)}{4\nu}
\right)\right].
$$

## Case

| parameter | value |
|---|---|
| domain | \([-1,1]^3\) |
| cells | `nx = ny = nz = 32` |
| viscosity | \(\nu = 0.2\) |
| wave speed | \(U = 1\) |
| initial centre | \(x_0=y_0=z_0=0\) |
| end time | \(t = 0.25\) s |
| convection | MUSCL, van Leer |
| time | SSPRK3, CFL 0.3 |

The diagonal wave is demanding on a coarse 3D grid; \(\nu=0.2\) keeps the
front resolved at this resolution. Use a denser grid and smaller viscosity on
the cluster.

## Run

```bash
cd examples/saturnBurger/saturnBurger3D
saturnCFD saturnBurger3D.yaml
python3 compareAnalytic.py
```

or the path-independent commands:

```bash
saturnCPU -threads 4 \
  examples/saturnBurger/saturnBurger3D/saturnBurger3D.yaml
saturnGPU \
  examples/saturnBurger/saturnBurger3D/saturnBurger3D.yaml
```

`compareAnalytic.py` recomputes the exact solution and writes a slice at
\(z=0\), an error slice, and a diagonal cut.

## Expected result

CPU build, 32 x 32 x 32 cells, \(t=0.25\) s:

```text
L1 error    : ...
L2 error    : 8.967e-04
Linf error  : ...
PASS: L2 ... <= tolerance 2.0e-03
```

The exact values are printed by the script for the current build.

## Output files

```text
results/saturnBurger3D/traveling.csv
results/saturnBurger3D/traveling.json
results/saturnBurger3D/comparison.png
results/saturnBurger3D/comparison.pdf
```

The CSV is the final field:

```text
x,y,z,u_numerical,u_exact,error
```

Set `output.vtkEvery: N` to write 3D `.vtu` frames and a `.pvd` collection.

## Validation in tests

- `tests/TestSaturnBurger3D.cpp`: plane-wave and diagonal-wave refinement.
- `tests/regression/burgers3d/traveling16.yaml`: stable 3D regression.
- `ctest --preset burgers3d`.
