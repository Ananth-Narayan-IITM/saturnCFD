# Heat and convection–diffusion

**Status:** second solver in the v1 sequence (ADR `0001`).

## 1. Governing equation

$$
\frac{\partial T}{\partial t}
+ \nabla\cdot(\mathbf{u}T)
= \nabla\cdot(\alpha\nabla T) + S,
$$

with $T$ [K], $\mathbf{u}$ [m/s], $\alpha=k/(\rho c_p)$ [m²/s] and
$S$ [K/s]. For a divergence-free velocity field,

$$
\frac{\partial T}{\partial t}
+ \mathbf{u}\cdot\nabla T
= \alpha\nabla^2 T + S.
$$

The solver reuses the Burgers finite-volume operators with a prescribed
velocity and a scalar unknown. It is the clean test of scalar transport,
boundary layers and the Péclet number

$$
Pe = \frac{UL}{\alpha}.
$$

## 2. Exact solutions

### Steady 1D convection–diffusion

$$
u\frac{dT}{dx} = \alpha\frac{d^2T}{dx^2},
\qquad T(0)=0,\quad T(L)=1,
$$

with constant $u>0$. The solution is

$$
T(x) = \frac{1-e^{ux/\alpha}}{1-e^{uL/\alpha}}.
$$

At large $Pe$ the solution has a thin boundary layer at $x=L$. This is the
primary test for the convection scheme: central differencing oscillates when
the cell Péclet number exceeds 2; upwind is stable but diffusive; a TVD limiter
is the compromise.

### Gaussian pulse

For an unbounded domain with uniform advection $u$ and no source,

$$
T(x,t) = \frac{1}{\sqrt{4\pi\alpha t}}
\exp\left(-\frac{(x-ut)^2}{4\alpha t}\right).
$$

A finite domain version with a small initial pulse is used to measure the
advection speed and the diffusive spreading.

### Manufactured solution

Choose a smooth $T(x,y,z,t)$, substitute into the equation and add the
remaining term as $S$. Use it to measure the formal order in 2D and 3D.

## 3. Discretisation

The same cell-centred finite-volume form as Burgers:

$$
V_P\frac{dT_P}{dt}
+ \sum_f \dot m_f T_f
= \sum_f \alpha_f\frac{T_N-T_P}{d_{PN}}A_f
+ V_P S_P,
$$

with $\dot m_f=\mathbf{u}_f\cdot\mathbf{S}_f$. The face temperature $T_f$ is
upwind or TVD-reconstructed; the diffusive term uses the non-orthogonal
correction from `finiteVolume.md`.

## 4. Time integration

- Explicit SSP-RK for the first validation, with the same CFL/diffusion limits
  as Burgers.
- Backward Euler or Crank–Nicolson for the implicit diffusion option, once the
  PETSc path is in place.
- Steady cases solve the linear system directly instead of marching.

## 5. Validation plan

| case | quantity | criterion |
|---|---|---|
| 1D steady boundary layer, $Pe=10,100$ | profile | matches the analytic solution; oscillation documented |
| Gaussian pulse | speed and width | advection speed and $\sqrt{t}$ spreading |
| 2D/3D manufactured solution | order | second order for central diffusion + TVD |
| central vs upwind vs minmod | cell Péclet | oscillation threshold and smearing |

## 6. Observations (to be filled from runs)

Record:

- the cell Péclet number at which central differencing first oscillates;
- the boundary-layer resolution versus mesh;
- the effective diffusion of upwind versus TVD;
- the observed order on the manufactured solution;
- the cost of the implicit versus explicit diffusion path.

## References

- Patankar (1980).
- Versteeg & Malalasekera (2007).
- Ferziger & Perić (2002).
- LeVeque (2002).
