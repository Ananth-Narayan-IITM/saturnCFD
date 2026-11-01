# Stokes

**Status:** design accepted for `saturnStokes`. The implementation starts with
the 3D core; the 2D and 3D examples are independent validation cases.

## 1. Governing equations

Steady Stokes flow:

$$
-\nabla p + \mu\nabla^2\mathbf{u} = \mathbf{f},
\qquad
\nabla\cdot\mathbf{u}=0,
$$

with $p$ [Pa], $\mu$ [Pa·s] and $\mathbf{f}$ [N/m³]. For constant
viscosity the vector Laplacian is

$$
\nabla^2\mathbf{u}
= \nabla(\nabla\cdot\mathbf{u}) - \nabla\times(\nabla\times\mathbf{u})
= -\nabla\times(\nabla\times\mathbf{u})
$$

when the flow is divergence-free. The solver uses the component-wise Laplacian
for simplicity; the vector identity is recorded for the theory discussion.

The Stokes problem is a saddle-point system: the pressure is the Lagrange
multiplier that enforces continuity. It has no time derivative, so it tests the
elliptic operators and the pressure coupling without nonlinear convection or
time accuracy.

## 2. Solution method

The primary method is a **fully coupled saddle-point solve**:

$$
\left[\begin{array}{cc}
A & B^{T}\\
B & 0
\end{array}\right]
\left[\begin{array}{cc}
\mathbf{u}_h\\ p_h
\end{array}\right]
=
\left[\begin{array}{cc}
\mathbf{f}_h\\ \mathbf{g}_h
\end{array}\right].
$$

The derivation, collocated Rhie–Chow stabilisation, PETSc fieldsplit/Schur
preconditioner and Navier–Stokes extension are in
[`coupledSaddlePoint.md`](coupledSaddlePoint.md).

SIMPLE and SIMPLEC are deliberately not the Stokes default. They add an outer
pressure-correction iteration and under-relaxation that a linear steady problem
does not need. They remain useful as an optional segregated path for
incompressible Navier–Stokes and as a fallback if a coupled preconditioner is
not yet available.

## 3. Discretisation

- Cell-centred velocity and pressure on the structured `LocalMesh`.
- Component-wise viscous Laplacian with Dirichlet wall data.
- Cell-centred pressure gradient.
- Rhie–Chow face-normal velocity in the continuity row to suppress the
  collocated odd–even pressure mode.
- PETSc `MPIAIJ` matrix with `u,v,w,p` degrees of freedom per cell.
- GMRES/FGMRES outer solve with fieldsplit/Schur block preconditioning.
- Pressure null space attached with `MatNullSpace` for all-Neumann cases.

## 4. Boundary conditions

- no-slip walls: $\mathbf{u}=0$;
- moving wall: $\mathbf{u}=\mathbf{U}_{\mathrm{wall}}$;
- inlet: fixed velocity profile;
- outlet: fixed pressure or zero-gradient velocity;
- periodic patches: mapped neighbours.

The pressure is determined up to a constant for all-Neumann boundaries. The
solver keeps the null space, removes the mean for error norms, and reports the
mean pressure.

## 5. Analytical validation

### Plane Poiseuille flow

Between plates at $y=0$ and $y=H$, driven by a constant pressure gradient
$G=-dp/dx$:

$$
u(y) = \frac{G}{2\mu}y(H-y),
\qquad v=0,
\qquad p(x)=p_0-Gx .
$$

For the first regression the flow can also be driven by a constant body force
$f_x=G$ with periodic streamwise boundaries; then the pressure is periodic and
the velocity is the same parabola.

### Plane Couette flow

Lower plate at rest, upper plate moving at $U$:

$$
u(y) = U\frac{y}{H},\qquad v=0,\qquad p=\mathrm{constant}.
$$

### Manufactured solution

Choose a smooth divergence-free $\mathbf{u}$ and derive $p$ and $\mathbf{f}$
from the momentum equation. Verify second-order convergence of velocity and
pressure on 2D and 3D hex meshes.

### Exit-boundary-condition cases

The BEM study of Sankhla & Das (2025) provides 3D steady Stokes cases with exit
boundary conditions. We use its geometries, boundary data and reported
quantities as inspiration for later 3D verification; the present solver uses a
finite-volume coupled saddle-point discretisation, not BEM.

## 6. Validation plan

| case | quantity | criterion |
|---|---|---|
| Poiseuille 2D/3D | velocity profile | matches the parabola to discretisation error |
| Couette 2D/3D | velocity profile | matches the linear profile |
| MMS 2D/3D | velocity/pressure order | second order in $h$ |
| null space | mean pressure | stable, no drift |
| continuity | discrete divergence | below solver tolerance |
| checkerboard | pressure field | no odd–even mode with Rhie–Chow |
| exit boundary | BEM paper cases | quantitative comparison |

## 7. Implementation plan

1. **3D core first** — `SaturnStokes`, matrix assembly, PETSc fieldsplit,
   pressure nullspace, output.
2. **2D/3D examples** — independent cases under
   `examples/saturnStokes/saturnStokes2D/` and `saturnStokes3D/`, with
   analytic comparison and plot scripts.
3. **Tests** — Poiseuille and Couette first, then MMS and 1/2/4-rank parity.
4. **Presentation and validation record** — update after the first converged
   cases.

## 8. Observations (to be filled from runs)

Record:

- fieldsplit/Schur iteration counts versus mesh and viscosity;
- velocity and pressure error norms for Poiseuille/Couette/MMS;
- discrete divergence after the coupled solve;
- pressure null-space and mean-pressure behaviour;
- sensitivity to the Rhie–Chow coefficient;
- the cost of the coupled solve versus a segregated fallback.

## References

- Benzi, Golub & Liesen (2005).
- Elman, Silvester & Wathen (2014).
- Kay, Loghin & Wathen (2002).
- Elman et al. (2006).
- Rhie & Chow (1983).
- Sankhla & Das (2025).
- Balay et al., *PETSc/TAO Users Manual*.
