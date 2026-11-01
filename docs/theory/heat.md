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
| periodic 2D/3D Fourier mode | order | second order for central diffusion + TVD |
| central vs upwind vs minmod | cell Péclet | oscillation threshold and smearing |

## 6. Implementation and measured results

`saturnHeat` is registered as a `ScalarSolver` and advances
`dT/dt + div(a T) = alpha lap(T) + source` with a prescribed uniform
velocity on a structured 1D/2D/3D block. It reuses the Burgers
finite-volume operators (`convectiveFlux`, `divergence`, `laplacian`) and the
generic scalar CSV/VTK/checkpoint/exact-error output. Periodic and
Dirichlet boundaries are supported; `heat.initial: fourier` builds both the
initial condition and the exact solution from

$$
T(\mathbf{x},t)
= A\sin\!\left(\mathbf{k}\cdot\mathbf{x}-(\mathbf{a}\cdot\mathbf{k})t\right)
\exp\!\left(-\alpha|\mathbf{k}|^2 t\right).
$$

### Periodic Fourier convergence, $t=0.1$ s

$\alpha=0.01$, $\mathbf{a}=(1,0.5,0.25)$, $\mathbf{k}=2\pi(1,1,1)$,
MUSCL/van Leer, SSPRK3:

| $N^3$ | $L^2$ | order |
|---:|---:|---:|
| 16 | 1.932e-2 | — |
| 32 | 3.973e-3 | 2.28 |
| 64 | 7.365e-4 | 2.43 |

These are the `TestSaturnHeat.cpp` values (`dtMax = 0.01`). The 48$^3$
user example uses `dtMax = 0.002` and reaches $L^2=1.479\times10^{-3}$.

### Periodic Fourier convergence, 2D, $t=0.2$ s

$\alpha=0.01$, $\mathbf{a}=(1,0.5,0)$, $\mathbf{k}=2\pi(1,1,0)$:

| $N^2$ | $L^2$ | order |
|---:|---:|---:|
| 32 | 5.733e-3 | — |
| 64 | 1.112e-3 | 2.37 |
| 128 | 2.425e-4 | 2.20 |

The same operator is checked on 1, 2 and 4 ranks with periodic halos by the
`heat_fourier32_np*` CTest targets. The examples
`examples/saturnHeat/saturnHeat2D` and `saturnHeat3D` regenerate the
comparison figures from the YAML parameters. The CUDA build reproduces the
CPU results on the local Quadro P620 (48$^3$ example L2
$1.478989\times10^{-3}$), and `heat_checkpoint` verifies the generic scalar
restart path.

### Observations

- The MUSCL reconstruction with the van Leer limiter gives clean second-order
  convergence for the smooth Fourier mode; no clipping is visible in the
  error field.
- Advection sets the phase error and the corrected Laplacian sets the
  amplitude error; the amplitude decay is captured to the same order.
- The explicit stability limit is the usual
  `dt <= min(CFL*h/|a|, safety*h^2/(2*alpha*d))`, and SSPRK3 keeps the
  diffusion term stable at the default `diffusionSafety`.
- The steady boundary-layer and cell-Péclet oscillation studies from the
  original plan remain open; they need the non-periodic 1D case and a
  central-differencing option.

## References

- Patankar (1980).
- Versteeg & Malalasekera (2007).
- Ferziger & Perić (2002).
- LeVeque (2002).
