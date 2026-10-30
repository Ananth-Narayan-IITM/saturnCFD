# Finite-volume operators on a structured collocated mesh

This page is the shared derivation for Burgers, heat, Stokes and incompressible
flow. It fixes the signs and the face conventions once, so the solver pages do
not re-derive them inconsistently.

## 1. Cell and face geometry

A cell \(P\) has volume \(V_P\), centroid \(\mathbf{x}_P\), and faces \(f\) with
area \(A_f\), centroid \(\mathbf{x}_f\), outward unit normal \(\mathbf{n}_f\)
and area vector \(\mathbf{S}_f=A_f\mathbf{n}_f\). A face between cells \(P\)
and \(N\) has owner \(P\) and neighbour \(N\); the normal points from \(P\) to
\(N\). A boundary face has only an owner.

The distance vector is

\[
\mathbf{d}_{PN} = \mathbf{x}_N - \mathbf{x}_P,\qquad
d_{PN} = |\mathbf{d}_{PN}|.
\]

For a structured mesh, \(P\) and \(N\) differ by one index in one direction, so
the neighbour lookup is \(O(1)\).

## 2. Generic conservation law

For a scalar \(\phi\),

\[
\frac{\partial}{\partial t}\int_{V_P}\rho\phi\,dV
+ \int_{\partial V_P}\rho\phi\,\mathbf{u}\cdot\mathbf{n}\,dA
= \int_{\partial V_P}\Gamma\nabla\phi\cdot\mathbf{n}\,dA
+ \int_{V_P}S_\phi\,dV.
\]

Discretely,

\[
V_P\frac{d(\rho_P\phi_P)}{dt}
+ \sum_f \dot m_f\,\phi_f
= \sum_f \Gamma_f\,\nabla\phi_f\cdot\mathbf{S}_f
+ V_P S_P,
\]

where \(\dot m_f=\rho_f\mathbf{u}_f\cdot\mathbf{S}_f\) is the mass flux.

## 3. Face interpolation

For a face between \(P\) and \(N\),

\[
\phi_f = w_P\phi_P + w_N\phi_N,\qquad
w_P = \frac{|\mathbf{x}_f-\mathbf{x}_N|}{d_{PN}},\quad
w_N = 1-w_P.
\]

On a uniform mesh \(w_P=w_N=1/2\). On a stretched mesh the weights follow the
face centroid.

## 4. Gradient

Two consistent options:

- **Gauss:** \(\nabla\phi_P \approx \frac{1}{V_P}\sum_f \phi_f\mathbf{S}_f\).
- **Least squares:** fit a linear field to the neighbour values; more robust on
  skewed meshes.

v1.0 uses Gauss for the primary operators and least squares only if the
non-orthogonality check demands it. The choice is recorded in the case file.

## 5. Divergence

For a face flux \(F_f = \mathbf{u}_f\cdot\mathbf{S}_f\),

\[
(\nabla\cdot\mathbf{u})_P \approx \frac{1}{V_P}\sum_f F_f.
\]

The discrete continuity equation is exactly this expression set to zero.

## 6. Laplacian and non-orthogonal correction

The face-normal gradient is split into an orthogonal and a correction part:

\[
\nabla\phi_f\cdot\mathbf{S}_f
= \underbrace{\Gamma_f\frac{\phi_N-\phi_P}{d_{PN}}A_f}_{\text{orthogonal}}
+ \underbrace{\Gamma_f\left(\overline{\nabla\phi}_f
- \frac{\phi_N-\phi_P}{d_{PN}}\mathbf{n}_f\right)\cdot\mathbf{S}_f}_{\text{correction}}.
\]

The correction is either deferred to the next iteration or included in the
matrix assembly. For a structured but stretched mesh, the correction is small;
for a strongly non-orthogonal mesh it dominates. `checkMesh` reports the
maximum non-orthogonality so the choice is explicit.

## 7. Convection

For Burgers, heat and Navier–Stokes, the face value \(\phi_f\) is reconstructed
from the upwind side. First-order upwind:

\[
\phi_f =
\begin{cases}
\phi_P, & \dot m_f \ge 0,\\
\phi_N, & \dot m_f < 0.
\end{cases}
\]

Higher order uses a TVD reconstruction:

\[
\phi_f = \phi_U + \frac{1}{2}\psi(r)(\phi_D-\phi_U),
\qquad
r = \frac{\phi_U-\phi_{UU}}{\phi_D-\phi_U},
\]

where \(U\) is upwind, \(D\) downwind and \(UU\) second-upwind. \(\psi(r)\) is
minmod, van Leer, MC or superbee. The limiter is selected in YAML.

For a hyperbolic flux, the Rusanov/local Lax–Friedrichs flux is

\[
\hat{\mathbf{F}}_f
= \frac{1}{2}\left(\mathbf{F}_L+\mathbf{F}_R\right)
- \frac{1}{2}\alpha\left(\phi_R-\phi_L\right)\mathbf{n}_f,
\qquad
\alpha = \max\left(|\mathbf{u}_L\cdot\mathbf{n}_f|,
|\mathbf{u}_R\cdot\mathbf{n}_f|\right).
\]

## 8. Collocated pressure–velocity coupling

On a collocated grid, a naive face velocity \(\overline{\mathbf{u}}_f\) and a
naive pressure gradient produce a checkerboard mode: a pressure field that
alternates sign from cell to cell has zero discrete gradient at cell centres.
Rhie–Chow replaces the face velocity with

\[
\mathbf{u}_f = \overline{\mathbf{u}}_f
- \frac{\Delta t}{\rho}\left(\nabla p_f - \overline{\nabla p}_f\right),
\]

where \(\nabla p_f\) is the compact face-normal pressure gradient and
\(\overline{\nabla p}_f\) is the interpolated cell gradient. The sign is chosen
so a linear pressure field is reproduced exactly; the derivation is repeated in
the solver's `rhieChowFlux` documentation. The correction makes the face flux
see the compact pressure stencil and removes the odd/even decoupling.

The steady SIMPLE form drops the \(\Delta t\) factor and uses the momentum
coefficient \(a_P\) instead:

\[
\mathbf{u}_f = \overline{\mathbf{u}}_f
- \left(\frac{V_P}{a_P}\right)_f
\left(\nabla p_f - \overline{\nabla p}_f\right).
\]

## 9. SIMPLE (steady Stokes and steady incompressible)

1. Guess \(p^*\), \(\mathbf{u}^*\).
2. Solve the momentum equation with \(p^*\):
   \[
   a_P\mathbf{u}_P = \sum_N a_N\mathbf{u}_N + \mathbf{b}_P
   - V_P(\nabla p^*)_P.
   \]
3. Compute face fluxes with Rhie–Chow.
4. Solve the pressure-correction equation:
   \[
   \nabla\cdot\left(\frac{\Delta t}{\rho}\nabla p'\right)
   = \nabla\cdot\mathbf{u}^*.
   \]
5. Correct:
   \[
   \mathbf{u} = \mathbf{u}^* - \frac{\Delta t}{\rho}\nabla p',
   \qquad
   p = p^* + \alpha_p p'.
   \]
6. Under-relax \(\mathbf{u}\) and \(p\), repeat until the residuals and the
   divergence norm are below tolerance.

The pressure-correction equation is a Poisson problem with homogeneous Neumann
boundary conditions on walls. It is singular up to a constant; PETSc's null
space handles it.

## 10. PISO (transient incompressible)

PISO is SIMPLE without the steady under-relaxation and with more than one
pressure corrector per time step:

1. momentum predictor with \(p^n\);
2. pressure correction and velocity correction;
3. recompute the face flux;
4. a second pressure correction using the corrected flux;
5. advance to \(n+1\).

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

## References

- Patankar (1980).
- Versteeg & Malalasekera (2007).
- Rhie & Chow (1983).
- Issa (1986).
- Ferziger & Perić (2002).
- LeVeque (2002) for hyperbolic fluxes and limiters.
