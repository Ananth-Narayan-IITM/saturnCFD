# Burgers equation

**Status:** first solver in the v1 sequence (ADR `0001`).

## 1. Governing equations

### Scalar Burgers

\[
\frac{\partial u}{\partial t}
+ \nabla\cdot\left(u\,\mathbf{a}\right)
= \nu\nabla^2 u + s,
\]

where \(u\) is the transported scalar, \(\mathbf{a}\) a prescribed advecting
velocity and \(\nu\) the diffusivity. In one dimension with \(a=u\):

\[
\frac{\partial u}{\partial t}
+ \frac{\partial}{\partial x}\left(\frac{u^2}{2}\right)
= \nu\frac{\partial^2 u}{\partial x^2}.
\]

The inviscid form is the simplest nonlinear hyperbolic conservation law; it
steepens smooth data into a shock in finite time.

### Vector Burgers

\[
\frac{\partial \mathbf{u}}{\partial t}
+ \nabla\cdot\left(\mathbf{u}\otimes\mathbf{u}\right)
= \nu\nabla^2\mathbf{u} + \mathbf{s}.
\]

This is the bridge to Navier–Stokes: same nonlinear convective term and same
diffusion, but without pressure and continuity. It is used to exercise the
vector form and the shared finite-volume operators before Stokes.

## 2. Exact solution used for validation

For the 1D viscous equation, the traveling wave

\[
u(x,t) = \frac{U}{2}\left[
1 - \tanh\left(\frac{U(x-Ut/2)}{4\nu}\right)
\right]
\]

is an exact solution. The argument is dimensionless. It moves with speed
\(U/2\), tends to \(U\) as \(x\to-\infty\) and to \(0\) as \(x\to+\infty\), and
has a transition thickness of order \(\nu/U\). This is the primary code
verification case: it exercises convection, diffusion and the boundary data
simultaneously.

For the inviscid equation, use a smooth periodic initial condition such as

\[
u_0(x) = -\sin(\pi x),\qquad x\in[-1,1],
\]

which forms a shock at \(t=1/\pi\). Before shock formation, compare with the
method of characteristics; after it, check the shock speed and the conservation
of \(\int u\,dx\).

## 3. Finite-volume discretisation

Cell-centred finite volume on the structured mesh:

\[
V_P\frac{du_P}{dt}
+ \sum_f \hat F_f A_f
= \sum_f \nu\frac{u_N-u_P}{d_{PN}}A_f
+ V_P s_P,
\]

where the convective flux is Rusanov/local Lax–Friedrichs:

\[
\hat F_f = \frac{1}{2}\left(\frac{u_L^2}{2}+\frac{u_R^2}{2}\right)
- \frac{1}{2}\alpha\left(u_R-u_L\right),
\qquad
\alpha=\max(|u_L|,|u_R|).
\]

Second-order reconstruction uses MUSCL with a TVD limiter (minmod, MC, van
Leer or superbee). The face value is reconstructed from the upwind side; the
limiter falls back to first-order upwind at extrema and shocks.

## 4. Time integration

Explicit SSP-RK2 or SSP-RK3. The convective step is limited by

\[
\Delta t \le \mathrm{CFL}\,\frac{\Delta x_{\min}}{\max_P |u_P|},
\]

with \(\mathrm{CFL}\le 0.5\) for SSP-RK2 and \(\le 1\) for SSP-RK3 in 1D. The
explicit diffusion limit is

\[
\Delta t \le \frac{\Delta x_{\min}^2}{2d\nu}
\]

for a \(d\)-dimensional central Laplacian. The solver takes the minimum and
reports which limit is active.

An implicit diffusion option is planned; it is not required for the first
validation.

## 5. Validation plan

| case | quantity | criterion |
|---|---|---|
| viscous traveling wave, \(\nu=10^{-2}\) | \(L^1,L^2,L^\infty\) | error decreases under refinement |
| viscous traveling wave, \(\nu=10^{-3}\) | observed order | close to the scheme order in smooth regions |
| mesh refinement \(N=32,64,128,256\) | order | table and plot |
| inviscid smooth data | pre-shock characteristics | pointwise agreement before shock |
| inviscid shock | shock position/speed | within one cell; conservation error small |
| limiter comparison | oscillation and smearing | upwind vs minmod vs MC documented |

## 6. Observations (to be filled from runs)

Record:

- the measured order in smooth regions and the order reduction at the shock;
- the effective numerical diffusion versus \(\nu\);
- the sensitivity to the limiter and the CFL number;
- the conservation error for the inviscid case;
- whether the exact traveling wave is reproduced at the outflow boundary or
  reflected;
- the cost per step and the active stability limit.

## References

- Burgers (1948).
- Hopf (1950).
- Cole (1951).
- LeVeque (2002).
- Toro (2009).
