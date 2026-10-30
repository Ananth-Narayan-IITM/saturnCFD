# Stokes

**Status:** third solver in the v1 sequence (ADR `0001`, `0004`).

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
elliptic operators and the pressure coupling without the complication of
nonlinear convection or time accuracy.

## 2. Boundary conditions

- no-slip walls: $\mathbf{u}=0$;
- moving wall: $\mathbf{u}=\mathbf{U}_{\text{wall}}$;
- inlet: fixed velocity profile;
- outlet: fixed pressure or zero-gradient velocity;
- periodic patches: mapped neighbours.

The pressure is determined up to a constant for all-Neumann boundaries. The
solver fixes the null space (PETSc `MatNullSpace`), and the diagnostics report
the mean pressure.

## 3. Solution method: SIMPLE

1. Guess $p^*$ and $\mathbf{u}^*$.
2. Solve the vector momentum equation:
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
5. Correct velocity and pressure:
   $$
   \mathbf{u} = \mathbf{u}^* - \frac{\Delta t}{\rho}\nabla p',
   \qquad
   p = p^* + \alpha_p p'.
   $$
6. Under-relax and repeat until the momentum residual, continuity residual and
   pressure-correction residual are below tolerance.

For steady Stokes there is no physical $\Delta t$; it is a pseudo-time factor
in the pressure-correction equation. The choice of $\Delta t$ and the
under-relaxation factors is documented in the case file and in the validation
record.

## 4. Analytical validation

### Plane Poiseuille flow

Between plates at $y=0$ and $y=H$, driven by a constant pressure gradient
$G=-dp/dx$:

$$
u(y) = \frac{G}{2\mu}y(H-y),
\qquad v=0.
$$

### Plane Couette flow

Lower plate at rest, upper plate moving at $U$:

$$
u(y) = U\frac{y}{H},\qquad v=0,\qquad p=\text{constant}.
$$

### Lid-driven cavity at $Re\to0$

The Stokes limit of the cavity. It tests the pressure coupling in a closed
domain with a singular lid corner. Compare against a converged reference or a
very-low-Reynolds Navier–Stokes solution.

### Manufactured solution

Choose a smooth divergence-free $\mathbf{u}$, compute $p$ from the momentum
equation, and verify second-order convergence on 2D/3D hex meshes.

## 5. Validation plan

| case | quantity | criterion |
|---|---|---|
| Poiseuille | profile | machine-precision match to the parabola |
| Couette | profile | linear profile |
| cavity $Re\to0$ | centreline | matches reference within tolerance |
| MMS 2D/3D | order | second order |
| null space | mean pressure | stable, no drift |
| checkerboard | pressure field | no odd/even mode with Rhie–Chow |

## 6. Observations (to be filled from runs)

Record:

- SIMPLE convergence rate versus under-relaxation and pseudo-time step;
- the effect of mesh stretching on the non-orthogonal correction;
- the pressure null-space behaviour;
- the divergence norm after each correction;
- whether Rhie–Chow is required on the stretched mesh (it is; document the
  failure without it).

## References

- Patankar & Spalding (1972).
- Patankar (1980).
- Rhie & Chow (1983).
- Ferziger & Perić (2002).
