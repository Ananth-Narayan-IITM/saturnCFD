# Finite-volume operators on a structured collocated mesh

This page is the shared derivation for Burgers, heat, Stokes and incompressible
flow. It fixes the signs and the face conventions once, so the solver pages do
not re-derive them inconsistently.

## 1. Cell and face geometry

A cell $P$ has volume $V_P$, centroid $\mathbf{x}_P$, and faces $f$ with
area $A_f$, centroid $\mathbf{x}_f$, outward unit normal $\mathbf{n}_f$
and area vector $\mathbf{S}_f=A_f\mathbf{n}_f$. A face between cells $P$
and $N$ has owner $P$ and neighbour $N$; the normal points from $P$ to
$N$. A boundary face has only an owner.

The distance vector is

$$
\mathbf{d}_{PN} = \mathbf{x}_N - \mathbf{x}_P,\qquad
d_{PN} = |\mathbf{d}_{PN}|.
$$

For a structured mesh, $P$ and $N$ differ by one index in one direction, so
the neighbour lookup is $O(1)$.

## 2. Generic conservation law

For a scalar $\phi$,

$$
\frac{\partial}{\partial t}\int_{V_P}\rho\phi\,dV
+ \int_{\partial V_P}\rho\phi\,\mathbf{u}\cdot\mathbf{n}\,dA
= \int_{\partial V_P}\Gamma\nabla\phi\cdot\mathbf{n}\,dA
+ \int_{V_P}S_\phi\,dV.
$$

Discretely,

$$
V_P\frac{d(\rho_P\phi_P)}{dt}
+ \sum_f \dot m_f\,\phi_f
= \sum_f \Gamma_f\,\nabla\phi_f\cdot\mathbf{S}_f
+ V_P S_P,
$$

where $\dot m_f=\rho_f\mathbf{u}_f\cdot\mathbf{S}_f$ is the mass flux.

## 3. Face interpolation

For a face between $P$ and $N$,

$$
\phi_f = w_P\phi_P + w_N\phi_N,\qquad
w_P = \frac{|\mathbf{x}_f-\mathbf{x}_N|}{d_{PN}},\quad
w_N = 1-w_P.
$$

On a uniform mesh $w_P=w_N=1/2$. On a stretched mesh the weights follow the
face centroid.

## 4. Gradient

Two consistent options:

- **Gauss:** $\nabla\phi_P \approx \frac{1}{V_P}\sum_f \phi_f\mathbf{S}_f$.
- **Least squares:** fit a linear field to the neighbour values; more robust on
  skewed meshes.

v1.0 uses Gauss for the primary operators and least squares only if the
non-orthogonality check demands it. The choice is recorded in the case file.

## 5. Divergence

For a face flux $F_f = \mathbf{u}_f\cdot\mathbf{S}_f$,

$$
(\nabla\cdot\mathbf{u})_P \approx \frac{1}{V_P}\sum_f F_f.
$$

The discrete continuity equation is exactly this expression set to zero.

## 6. Laplacian and non-orthogonal correction

Let $\mathbf{d}=\mathbf{x}_N-\mathbf{x}_P$, $d_\perp=\mathbf{d}\cdot\mathbf{n}_f$
and $\overline{\nabla\phi}_f$ the interpolated cell gradient. The implemented
normal gradient is

$$
\nabla\phi_f\cdot\mathbf{n}_f
= \frac{(\phi_N-\phi_P)
- \mathbf{d}\cdot\left(\overline{\nabla\phi}_f
- (\overline{\nabla\phi}_f\cdot\mathbf{n}_f)\mathbf{n}_f\right)}
{d_\perp}.
$$

The correction is included in one gradient pass; deferred/iterated correction
is a later refinement. For a structured but stretched mesh, the correction is
small; for a strongly non-orthogonal mesh it dominates. `checkMesh` reports the
maximum non-orthogonality so the choice is explicit.

On a physical boundary face the same expression uses the interpolated face
value and the owner gradient, with $d_\perp$ from the owner centroid to the
face centroid. On a partition/periodic ghost face the owner gradient is used;
that is exact for a linear field.

## 7. Convection

For Burgers, heat and Navier–Stokes, the face value $\phi_f$ is reconstructed
from the upwind side. First-order upwind:

$$
\phi_f =
\left\{
\begin{array}{ll}
\phi_P, & \dot m_f \ge 0,\\
\phi_N, & \dot m_f < 0.
\end{array}
\right.
$$

The Rusanov flux is implemented as

$$
F_f = \frac12 u_n(\phi_P+\phi_N)
    - \frac12 |u_n|(\phi_N-\phi_P).
$$

For a scalar with one wave speed this is algebraically the upwind flux, so it
is tested as an identity. Higher order uses a TVD reconstruction:

$$
\phi_f = \phi_U + \frac{1}{2}\psi(r)(\phi_D-\phi_U),
\qquad
r = \frac{\phi_U-\phi_{UU}}{\phi_D-\phi_U},
$$

where $U$ is upwind, $D$ downwind and $UU$ second-upwind. $\psi(r)$ is
minmod, van Leer, MC or superbee. The implemented scheme builds the limited
slope from the two cell-centred differences along the face direction and
reconstructs the upwind cell at the face centroid. The limiter is currently a
function argument; YAML selection comes with the boundary/solver schema.

For a smooth periodic sine field the measured L2 order is about 1.6 for
minmod/van Leer and 1.15--1.42 for superbee. The reduction from 2 is expected:
TVD limiters clip smooth extrema. A periodic step advection test verifies that
the reconstructed scheme stays within the initial bounds.

For a hyperbolic flux, the Rusanov/local Lax–Friedrichs flux is

$$
\hat{\mathbf{F}}_f
= \frac{1}{2}\left(\mathbf{F}_L+\mathbf{F}_R\right)
- \frac{1}{2}\alpha\left(\phi_R-\phi_L\right)\mathbf{n}_f,
\qquad
\alpha = \max\left(|\mathbf{u}_L\cdot\mathbf{n}_f|,
|\mathbf{u}_R\cdot\mathbf{n}_f|\right).
$$

## 8. Collocated pressure–velocity coupling

On a collocated grid, a naive face velocity $\overline{\mathbf{u}}_f$ and a
naive pressure gradient produce a checkerboard mode: a pressure field that
alternates sign from cell to cell has zero discrete gradient at cell centres.
Rhie–Chow replaces the face velocity with

$$
\mathbf{u}_f = \overline{\mathbf{u}}_f
- \frac{\Delta t}{\rho}\left(\nabla p_f - \overline{\nabla p}_f\right),
$$

where $\nabla p_f$ is the compact face-normal pressure gradient and
$\overline{\nabla p}_f$ is the interpolated cell gradient. The sign is chosen
so a linear pressure field is reproduced exactly. The implemented operator is
the first slice:

$$
u_{n,f} = \overline{u}_{n,f}
+ c\left(\frac{p_N-p_P}{d_\perp}
- \overline{\nabla p}_f\cdot\mathbf{n}_f\right),
$$

where the coefficient $c$ is caller-supplied until the momentum diagonal
$V/a_P$ exists. It is zero for a linear pressure field and nonzero for a
checkerboard pressure. The pressure Poisson solve, SIMPLE/PISO loop and PETSc
linear algebra are not yet implemented; the derivation is repeated in the
solver's `rhieChowFlux` documentation. The correction makes the face flux
see the compact pressure stencil and removes the odd/even decoupling.

The steady SIMPLE form drops the $\Delta t$ factor and uses the momentum
coefficient $a_P$ instead:

$$
\mathbf{u}_f = \overline{\mathbf{u}}_f
- \left(\frac{V_P}{a_P}\right)_f
\left(\nabla p_f - \overline{\nabla p}_f\right).
$$

## 9. SIMPLE (steady Stokes and steady incompressible)

1. Guess $p^*$, $\mathbf{u}^*$.
2. Solve the momentum equation with $p^*$:
   $$
   a_P\mathbf{u}_P = \sum_N a_N\mathbf{u}_N + \mathbf{b}_P
   - V_P(\nabla p^*)_P.
   $$
3. Compute face fluxes with Rhie–Chow.
4. Solve the pressure-correction equation:
   $$
   \nabla\cdot\left(\frac{\Delta t}{\rho}\nabla p'\right)
   = \nabla\cdot\mathbf{u}^*.
   $$
5. Correct:
   $$
   \mathbf{u} = \mathbf{u}^* - \frac{\Delta t}{\rho}\nabla p',
   \qquad
   p = p^* + \alpha_p p'.
   $$
6. Under-relax $\mathbf{u}$ and $p$, repeat until the residuals and the
   divergence norm are below tolerance.

The pressure-correction equation is a Poisson problem with homogeneous Neumann
boundary conditions on walls. It is singular up to a constant; PETSc's null
space handles it.

## 10. PISO (transient incompressible)

PISO is SIMPLE without the steady under-relaxation and with more than one
pressure corrector per time step:

1. momentum predictor with $p^n$;
2. pressure correction and velocity correction;
3. recompute the face flux;
4. a second pressure correction using the corrected flux;
5. advance to $n+1$.

Two correctors are the default; the number is a YAML option. The time term is
backward Euler first, Crank–Nicolson later.

## 11. Boundary conditions

- `fixedValue`: Dirichlet; the boundary face value is prescribed.
- `zeroGradient`: Neumann; the face value equals the owner cell value.
- `noSlip`: vector Dirichlet with zero value.
- `symmetry`: normal component zero, tangential gradient zero.
- `empty`: the direction is inactive (2D).
- `periodic`: owner and neighbour are on opposite patches; the face is treated
  as internal with a mapped neighbour.

Every boundary face belongs to exactly one patch. A missing BC is a
configuration error, not a default.

## 12. Parallel finite volume

The domain is partitioned into Cartesian blocks of the logical $(i,j,k)$
index space. Each cell has exactly one owner rank. A partition-boundary cell is
duplicated on the neighbouring rank as a **read-only ghost cell**. There are no
shared cells and no shared writes.

The halo exchange fills two ghost layers (face, edge and corner neighbours,
including periodic images). A rank computes fluxes only for its owned cells;
the two ranks adjacent to a partition-boundary face compute the same flux from
the same valid/ghost pair and update their own cells. The shared face is
therefore not double-counted.

Ghosts are communication buffers, not state. Operators refresh the fields they
read, apply physical boundary conditions, loop over owned cells only, and write
owned cells only. See `docs/protocols/parallel.md`.

## References

- Patankar (1980).
- Versteeg & Malalasekera (2007).
- Rhie & Chow (1983).
- Issa (1986).
- Ferziger & Perić (2002).
- LeVeque (2002) for hyperbolic fluxes and limiters.
