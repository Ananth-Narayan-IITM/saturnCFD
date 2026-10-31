# Time integration

**Status:** explicit schemes implemented. Backward Euler, Crank–Nicolson and
BDF2 are specified here and arrive with the implicit PETSc diffusion solve.

## 1. Semi-discrete form

After the finite-volume spatial discretisation:

$$
\frac{d u_P}{dt} = L(u)_P,
$$

where $L$ contains convection, diffusion and sources. The cell volume is
already part of the operator. The solver exposes `rhs(state, rate)` and the
time loop combines stages.

## 2. Implemented explicit schemes

Forward Euler:

$$
u^{n+1} = u^n + \Delta t\,L(u^n).
$$

SSPRK2 (Heun/SSP form):

$$
u^{(1)} = u^n + \Delta t\,L(u^n),
$$

$$
u^{n+1} = \frac12 u^n
+ \frac12\left(u^{(1)} + \Delta t\,L(u^{(1)})\right).
$$

SSPRK3:

$$
u^{(1)} = u^n + \Delta t\,L(u^n),
$$

$$
u^{(2)} = \frac34 u^n
+ \frac14\left(u^{(1)} + \Delta t\,L(u^{(1)})\right),
$$

$$
u^{n+1} = \frac13 u^n
+ \frac23\left(u^{(2)} + \Delta t\,L(u^{(2)})\right).
$$

The stage updates are performed by one generic kernel:

$$
u_{\mathrm{out}} = a_0 u^n + a_1 u^{(\mathrm{stage})}
+ a_2 \Delta t\,L(u^{(\mathrm{stage})}).
$$

Boundary ghosts are refreshed after every stage. The time loop stores the
three stage fields and the rate field once, so there is no allocation inside
the time loop.

## 3. Stability limits

Convective CFL:

$$
\Delta t_{\mathrm{conv}} = \mathrm{CFL}\,
\frac{h_{\min}}{\max |\mathbf{u}|}.
$$

Explicit diffusion:

$$
\Delta t_{\mathrm{diff}} = \frac{s\,h_{\min}^2}{2d\,\alpha_{\max}},
$$

with $s\le 0.5$ and $d$ the spatial dimension. The active limit is the
smaller of the two and `dtMax`. The diagnostics report the convective CFL
$\max|u|\Delta t/h$ and the diffusive number $\alpha\Delta t/h^2$.

The time loop rejects a step below `dtMin`; this usually means the mesh is too
fine for explicit diffusion or the CFL is too large.

## 4. Planned implicit schemes

Backward Euler:

$$
\frac{u^{n+1}-u^n}{\Delta t} = L(u^{n+1}).
$$

Crank–Nicolson:

$$
\frac{u^{n+1}-u^n}{\Delta t}
= \frac12\left(L(u^n)+L(u^{n+1})\right).
$$

BDF2 (started with BE):

$$
\frac{3u^{n+1}-4u^n+u^{n-1}}{2\Delta t} = L(u^{n+1}).
$$

For diffusion, the implicit matrix contains the orthogonal face coefficients
and the non-orthogonal correction is deferred to the RHS. That keeps the
matrix symmetric positive definite for CG/GAMG. The linear solve uses
`saturnLinAlg`; no home-grown Krylov solver.

## 5. IMEX for Burgers and heat

Recommended first implicit form:

$$
\frac{u^{n+1}-u^n}{\Delta t}
+ C(u^n) = D(u^{n+1}) + s,
$$

where $C$ is explicit convection and $D$ is implicit diffusion. This removes
the explicit diffusion limit while keeping the nonlinear convection simple.
Backward Euler is the robust first choice; Crank–Nicolson is the second-order
upgrade. BDF2 follows after the BE/CN results are accepted.

## 6. Tests

`tests/TestSimulation.cpp` checks:

| case | criterion |
|---|---|
| constant field, all explicit schemes | stays exactly 1 |
| cosine diffusion, SSPRK3 | L2 matches the analytic decay within 5e-3 |
| sine advection, SSPRK3 | bounded, no NaN |
| `TimeControl` | selects convective/diffusive limit correctly |
| registry | add, lookup, duplicate and unknown-name errors |
| implicit scheme without a solver | clear `invalid_argument` |

All cases run under 1/2/4 ranks in `testSaturnFV`.

## References

- Gottlieb, Ketcheson & Shu (2011), *Strong Stability Preserving Runge–Kutta
  and Multistep Time Discretization*.
- Hairer & Wanner (1996), *Solving Ordinary Differential Equations II*.
- Patankar (1980), *Numerical Heat Transfer and Fluid Flow*.
- ADR `0005`, `0003`.
