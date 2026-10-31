# Burgers equation

**Status:** 1D scalar solver `saturnBurger` implemented and validated
(ADR `0001`). The 2D/3D extension and implicit diffusion are next.

## 1. Governing equations

### Scalar Burgers

$$
\frac{\partial u}{\partial t}
+ \nabla\cdot\left(u\,\mathbf{a}\right)
= \nu\nabla^2 u + s,
$$

where $u$ is the transported scalar, $\mathbf{a}$ a prescribed advecting
velocity and $\nu$ the diffusivity. In one dimension with $a=u$:

$$
\frac{\partial u}{\partial t}
+ \frac{\partial}{\partial x}\left(\frac{u^2}{2}\right)
= \nu\frac{\partial^2 u}{\partial x^2}.
$$

The inviscid form is the simplest nonlinear hyperbolic conservation law; it
steepens smooth data into a shock in finite time.

### Vector Burgers

$$
\frac{\partial \mathbf{u}}{\partial t}
+ \nabla\cdot\left(\mathbf{u}\otimes\mathbf{u}\right)
= \nu\nabla^2\mathbf{u} + \mathbf{s}.
$$

This is the bridge to Navier–Stokes: same nonlinear convective term and same
diffusion, but without pressure and continuity. It is used to exercise the
vector form and the shared finite-volume operators before Stokes.

## 2. Exact solution used for validation

For the 1D viscous equation, the traveling wave

$$
u(x,t) = \frac{U}{2}\left[
1 - \tanh\left(\frac{U(x-Ut/2)}{4\nu}\right)
\right]
$$

is an exact solution. The argument is dimensionless. It moves with speed
$U/2$, tends to $U$ as $x\to-\infty$ and to $0$ as $x\to+\infty$, and
has a transition thickness of order $\nu/U$. This is the primary code
verification case: it exercises convection, diffusion and the boundary data
simultaneously.

For the inviscid equation, use a smooth periodic initial condition such as

$$
u_0(x) = -\sin(\pi x),\qquad x\in[-1,1],
$$

which forms a shock at $t=1/\pi$. Before shock formation, compare with the
method of characteristics; after it, check the shock speed and the conservation
of $\int u\,dx$.

## 3. Finite-volume discretisation

Cell-centred finite volume on the structured mesh:

$$
V_P\frac{du_P}{dt}
+ \sum_f \hat F_f A_f
= \sum_f \nu\frac{u_N-u_P}{d_{PN}}A_f
+ V_P s_P,
$$

where the convective flux is Rusanov/local Lax–Friedrichs:

$$
\hat F_f = \frac{1}{2}\left(\frac{u_L^2}{2}+\frac{u_R^2}{2}\right)
- \frac{1}{2}\alpha\left(u_R-u_L\right),
\qquad
\alpha=\max(|u_L|,|u_R|).
$$

Second-order reconstruction uses MUSCL with a TVD limiter (minmod, MC, van
Leer or superbee). The face value is reconstructed from the upwind side; the
limiter falls back to first-order upwind at extrema and shocks.

## 4. Time integration

Explicit SSP-RK2 or SSP-RK3. The convective step is limited by

$$
\Delta t \le \mathrm{CFL}\,\frac{\Delta x_{\min}}{\max_P |u_P|},
$$

with $\mathrm{CFL}\le 0.5$ for SSP-RK2 and $\le 1$ for SSP-RK3 in 1D. The
explicit diffusion limit is

$$
\Delta t \le \frac{\Delta x_{\min}^2}{2d\nu}
$$

for a $d$-dimensional central Laplacian. The solver takes the minimum and
reports which limit is active.

An implicit diffusion option is planned; it is not required for the first
validation.

## 5. Validation plan

| case | quantity | criterion |
|---|---|---|
| viscous traveling wave, $\nu=10^{-2}$ | $L^1,L^2,L^\infty$ | error decreases under refinement |
| viscous traveling wave, $\nu=10^{-3}$ | observed order | close to the scheme order in smooth regions |
| mesh refinement $N=32,64,128,256,512$ | order | table and plot |
| 1/2/4 ranks at $N=256$ | $L^1,L^2,L^\infty$ | identical within roundoff |
| inviscid smooth data | pre-shock characteristics | pointwise agreement before shock |
| inviscid shock | shock position/speed | within one cell; conservation error small |
| limiter comparison | oscillation and smearing | upwind vs minmod vs MC documented |

## 6. Implemented 1D solver

**Library target:** `saturnBurger`

**Class:** `saturn::SaturnBurger`

**Directory:** `src/solvers/saturnBurger/`

The solver advances

$$
\partial_t u + \partial_x\left(\frac{u^2}{2}\right)
= \nu\partial_{xx}u + s
$$

on a structured 1D block. It uses:

- `burgersFlux` with Rusanov/local Lax–Friedrichs;
- MUSCL reconstruction with minmod, van Leer or superbee;
- the shared non-orthogonal `laplacian` for diffusion;
- `Simulation` with forward Euler, SSPRK2 or SSPRK3;
- `setTime` so Dirichlet data at a moving exact boundary is refreshed after
  every RK stage.

The face flux is stored with a **canonical orientation** (lower-coordinate
cell on the left), then multiplied by the outward normal component. This makes
the flux antisymmetric across shared faces and preserves the discrete
conservation law.

## 7. Observations from the validation runs

CPU OpenMP build, 1D, `U=1`, `nu=0.02`, `t=0.5`, MUSCL/van Leer, CFL 0.3,
SSPRK3. The YAML case is
`tests/regression/burgers/traveling64.yaml`; `tools/validateBurgers.py`
regenerates the table and the plot.

| grid | $L^1$ | $L^2$ | $L^\infty$ | observed order |
|---:|---:|---:|---:|---:|
| 32 | 3.911e-3 | 9.015e-3 | 2.981e-2 | — |
| 64 | 9.965e-4 | 2.284e-3 | 7.977e-3 | 1.98 |
| 128 | 2.259e-4 | 5.291e-4 | 2.009e-3 | 2.11 |
| 256 | 5.174e-5 | 1.236e-4 | 5.063e-4 | 2.10 |
| 512 | 1.226e-5 | 2.973e-5 | 1.271e-4 | 2.06 |
| 1024 | 2.977e-6 | 7.287e-6 | 3.173e-5 | 2.03 |

The error decreases by a factor of about four per refinement, as expected for a
second-order method. The small non-zero value at a finite grid is the
discretisation error, not a solver defect; it tends to zero as $h^2$.

At $N=256$, 1, 2 and 4 MPI ranks give **bitwise-identical** norms:

| ranks | $L^1$ | $L^2$ | $L^\infty$ |
|---:|---:|---:|---:|
| 1 | 5.173840745110388e-05 | 1.2363783241939333e-04 | 5.062617877850117e-04 |
| 2 | 5.173840745110388e-05 | 1.2363783241939333e-04 | 5.062617877850117e-04 |
| 4 | 5.173840745110388e-05 | 1.2363783241939333e-04 | 5.062617877850117e-04 |

An earlier apparent $\sqrt{n_{\rm ranks}}$ growth was a diagnostic bug: the test
divided the global squared error by the **local** owned-cell count. The solver
itself was already rank independent. The corrected diagnostic and the YAML
summary both use the global count.

Periodic inviscid shock, $u_0=-\sin(\pi x)$, $N=128$, $t=0.5$:

- bounds: $[-0.9954, 0.9954]$ — no new extrema;
- discrete integral: $1.5\times10^{-16}$ — conservative;
- the exact traveling wave is reproduced at the Dirichlet boundaries without
  visible reflection.

See [`../validation/burgers.md`](../validation/burgers.md) for the plot and the
reproduction commands.

Still to measure:

- effective numerical diffusion versus $\nu$;
- limiter sensitivity (minmod vs MC vs van Leer vs superbee);
- shock-speed error in cells;
- active stability limit versus CFL;
- cost per step.

## 8. Limitations

- 1D only; `ny = nz = 1`.
- Constant source only.
- Explicit diffusion; IMEX/implicit BE/CN/BDF2 waits for the FV Poisson
  matrix assembler and `saturnLinAlg`.
- The 2D/3D extension will add transverse directions and a vector or
  direction-dependent Burgers flux.
- The CLI is wired: `saturnCFD case.yaml`, `-listSolvers`,
  `-dumpConfig saturnBurger` and `-checkConfig` all use the registered solver.
- Neumann boundaries and a spatially varying source are not exposed yet.

## References

- Burgers (1948).
- Hopf (1950).
- Cole (1951).
- LeVeque (2002).
- Toro (2009).
