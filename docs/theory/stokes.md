# Stokes

**Status:** implemented for 2D and 3D on 1/2/4 MPI ranks. Independent examples are in
`examples/saturnStokes/`.

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

The derivation, collocated pressure-Laplacian stabilisation, PETSc
fieldsplit/Schur preconditioner and Navier–Stokes extension are in
[`coupledSaddlePoint.md`](coupledSaddlePoint.md).

`stokes.linearSolver` defaults to `fieldsplit`, which needs only `O(N)`
memory: outer FGMRES, `selfp` Schur, GMRES + GAMG on velocity and
preonly + GAMG on pressure. It gives 11–20 outer iterations in 2D and 3–13 in
3D on 1/2/4/8 ranks. The validation reference uses PETSc's coupled direct LU
(`stokes.linearSolver: direct`), preferring SuperLU_DIST when the PETSc build
provides it. Direct LU is retained for validation and for users with enough
memory.
SIMPLE and SIMPLEC are deliberately not the Stokes default; they remain an
optional segregated path for incompressible Navier–Stokes.

## 3. Discretisation

- Cell-centred velocity and pressure on the structured `LocalMesh`.
- Component-wise viscous Laplacian with Dirichlet wall data.
- Cell-centred pressure gradient.
- Brezzi–Pitkäranta pressure Laplacian in the continuity row to suppress the
  collocated odd–even pressure mode.
- PETSc `MPIAIJ` matrix with `u,v,w,p` degrees of freedom per cell, inserted
  through `MatSetValuesStencil` so the DMDA global numbering is respected on
  any rank layout.
- All equations are assembled per unit volume: `mu/h^2` diffusion, `0.5/h`
  pressure gradient and divergence, `beta/h^2` stabilisation with
  `beta = stabilization * h_min^2 / mu`. This keeps the pressure physical and
  the Schur complement well conditioned.
- PETSc coupled direct LU for the validation cases; GMRES/FGMRES + fieldsplit
  as the production path being tuned.
- One pressure reference row fixes the constant gauge for the direct solve.

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
| checkerboard | pressure field | no odd–even mode with pressure-Laplacian stabilisation |
| exit boundary | BEM paper cases | quantitative comparison |

## 7. Implemented results

| case | mesh | $L^2$ | notes |
|---|---|---|---|
| Poiseuille 2D | 16$^2$ | 4.88e-4 | second order |
| Poiseuille 2D | 32$^2$ | 1.22e-4 | second order |
| Poiseuille 2D | 64$^2$ | 3.05e-5 | example |
| Couette 2D | 64$^2$ | 7.5e-13 | exact line |
| Poiseuille 3D | 24$^2	imes8$ | 2.17e-4 | extruded channel |
| Couette 3D | 24$^2	imes8$ | 5.7e-12 | exact line |

Both 2D and 3D cases pass on 1/2/4 ranks; 2D also passes on 8 ranks.

## 8. MMS status

The periodic manufactured solution is source-consistent and second order in
velocity (2.01 in 2D, 2.03 in 3D); the measured pressure order is 1.89 and
1.71. The wall-bounded variant is implemented but its wall pressure order is
still being investigated.

## 9. Next steps

1. Resolve the wall-bounded MMS pressure order.
2. Add the exit-boundary case from an imported Gmsh `.msh` mesh.
3. Rebuild the validation figures and presentation.

## 10. Observations

- The direct coupled LU solve converges in one iteration on the small
  validation meshes.
- Poiseuille is second order; Couette is at roundoff.
- The pressure reference row fixes the constant gauge; the mean-removed
  pressure error is at roundoff for both channel cases.
- The fieldsplit/Schur path works on the small linear-algebra block test but
  stalls on refined FV matrices; it is the main solver follow-up.
- The 3D DMDA multi-rank velocity assembly is the other open item.

## References

- Benzi, Golub & Liesen (2005).
- Elman, Silvester & Wathen (2014).
- Kay, Loghin & Wathen (2002).
- Elman et al. (2006).
- Rhie & Chow (1983).
- Sankhla & Das (2025).
- Balay et al., *PETSc/TAO Users Manual*.
