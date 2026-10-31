# Burgers validation — 1D scalar `saturnBurger`

**Solver:** `saturnBurger` (class `saturn::SaturnBurger`)

**Equation:** $\partial_t u + \partial_x(u^2/2) = \nu\partial_{xx}u$

**Case:** `tests/regression/burgers/traveling64.yaml`

**Reproduction:** `python3 tools/validateBurgers.py`

This page records the analytic validation of the 1D solver. The reference is
the exact viscous traveling wave

$$
u(x,t)=\frac{U}{2}\left[
1-\tanh\left(\frac{U(x-x_0-Ut/2)}{4\nu}\right)\right],
\qquad U=1,\ \nu=0.02,\ x_0=0,
$$

on $x\in[-1,1]$ up to $t=0.5$ s. Dirichlet boundary data follow the wave;
the initial state is the same wave at $t=0$.

![Burgers validation figure](figures/burgersValidation.png)

## Measured convergence

CPU OpenMP build, MUSCL/van Leer, SSPRK3, CFL 0.3:

| N | $h$ | $L^1$ | $L^2$ | $L^\infty$ | observed order |
|---:|---:|---:|---:|---:|---:|
| 32 | 0.0625 | 3.911e-3 | 9.015e-3 | 2.981e-2 | — |
| 64 | 0.03125 | 9.965e-4 | 2.284e-3 | 7.977e-3 | 1.98 |
| 128 | 0.015625 | 2.259e-4 | 5.291e-4 | 2.009e-3 | 2.11 |
| 256 | 0.0078125 | 5.174e-5 | 1.236e-4 | 5.063e-4 | 2.10 |
| 512 | 0.00390625 | 1.226e-5 | 2.973e-5 | 1.271e-4 | 2.06 |
| 1024 | 0.001953125 | 2.977e-6 | 7.287e-6 | 3.173e-5 | 2.03 |

The observed order is close to two at every refinement. The error at the
finest grid is $7.3\times10^{-6}$ in $L^2$ and $3.2\times10^{-5}$ in
$L^\infty$, out of a unit-amplitude wave. This is the expected truncation error
of a second-order scheme, not a rank or boundary artefact.

## Rank independence

At $N=256$, 1, 2 and 4 ranks produce the same discrete solution:

| ranks | $L^1$ | $L^2$ | $L^\infty$ |
|---:|---:|---:|---:|
| 1 | 5.173840745110388e-05 | 1.2363783241939333e-04 | 5.062617877850117e-04 |
| 2 | 5.173840745110388e-05 | 1.2363783241939333e-04 | 5.062617877850117e-04 |
| 4 | 5.173840745110388e-05 | 1.2363783241939333e-04 | 5.062617877850117e-04 |

The relative $L^2$ difference is exactly zero. The earlier $\sqrt{n_{\rm ranks}}$
growth was caused by the validation diagnostic dividing by the local owned-cell
count instead of the global count; the solver RHS, halo exchange and reduction
were already rank independent.

## Other checks

| check | result |
|---|---|
| constant field, periodic, inviscid | preserved exactly |
| periodic inviscid shock, $u_0=-\sin(\pi x)$, $N=128$, $t=0.5$ | bounds $[-0.9954,0.9954]$, integral $1.5\times10^{-16}$ |
| constant Burgers flux | zero divergence; face flux $-0.5/+0.5$ |
| YAML CLI path | `saturnCFD case.yaml` writes CSV and JSON; CTest checks the norms |

## Reproduce

```bash
cmake --preset default
cmake --build buildCPU -j

# one case through the registered CLI
./buildCPU/saturnCFD -checkConfig \
  tests/regression/burgers/traveling64.yaml
./buildCPU/saturnCFD tests/regression/burgers/traveling64.yaml

# the full table, rank sweep and figure
python3 tools/validateBurgers.py

# CTest slice
ctest --preset burgers
```

Outputs:

```text
results/burgers/validation/convergence.csv
results/burgers/validation/rankIndependence.csv
docs/validation/figures/burgersValidation.{png,pdf}
```

## Verdict

The 1D scalar `saturnBurger` solver is fully validated for this phase:

- it matches the analytic traveling-wave solution with a clean second-order
  convergence rate;
- the error is rank independent and, in exact arithmetic terms, identical on
  1/2/4 ranks;
- it conserves and bounds the periodic inviscid shock;
- it is registered and runs through a proper YAML case.

The remaining loop-related item is to extend the same solver to 2D/3D, which is
deliberately deferred.
