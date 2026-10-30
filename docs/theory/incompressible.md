# Incompressible Navier–Stokes

**Status:** fourth solver in the v1 sequence (ADR `0001`, `0004`).

## 1. Governing equations

\[
\rho\left(
\frac{\partial \mathbf{u}}{\partial t}
+ \nabla\cdot(\mathbf{u}\otimes\mathbf{u})
\right)
= -\nabla p + \mu\nabla^2\mathbf{u} + \mathbf{f},
\qquad
\nabla\cdot\mathbf{u}=0,
\]

or, divided by \(\rho\),

\[
\frac{\partial \mathbf{u}}{\partial t}
+ \nabla\cdot(\mathbf{u}\otimes\mathbf{u})
= -\frac{1}{\rho}\nabla p + \nu\nabla^2\mathbf{u}
+ \frac{\mathbf{f}}{\rho},
\qquad
\nabla\cdot\mathbf{u}=0.
\]

The pressure is dynamic pressure [Pa]. For constant density, the kinematic
pressure \(p/\rho\) is an alternative; v1 stores dynamic pressure for the
two-phase future.

The Reynolds number is

\[
Re = \frac{\rho U L}{\mu}.
\]

## 2. Discretisation

Cell-centred finite volume on the structured hexahedral mesh:

\[
V_P\frac{d(\rho\mathbf{u}_P)}{dt}
+ \sum_f \dot m_f \mathbf{u}_f
= -\sum_f p_f\mathbf{S}_f
+ \sum_f \mu\nabla\mathbf{u}_f\cdot\mathbf{S}_f
+ V_P\mathbf{f}_P.
\]

The convective face value uses upwind or a TVD/MUSCL reconstruction; the
diffusive term uses the non-orthogonal correction. The face flux is
Rhie–Chow-corrected to suppress checkerboarding.

The discrete continuity equation is

\[
\frac{1}{V_P}\sum_f \mathbf{u}_f\cdot\mathbf{S}_f = 0.
\]

## 3. Time integration: PISO

1. **Momentum predictor** with the old pressure \(p^n\):
   \[
   a_P\mathbf{u}_P^* = \sum_N a_N\mathbf{u}_N^*
   + \mathbf{b}_P - V_P(\nabla p^n)_P.
   \]
2. Compute face fluxes with Rhie–Chow.
3. **Pressure correction** \(p'\):
   \[
   \nabla\cdot\left(\frac{\Delta t}{\rho}\nabla p'\right)
   = \nabla\cdot\mathbf{u}^*.
   \]
4. **Velocity correction:**
   \[
   \mathbf{u}^{**} = \mathbf{u}^* - \frac{\Delta t}{\rho}\nabla p',
   \qquad p^* = p^n + \alpha_p p'.
   \]
5. Recompute the face flux and apply a second pressure correction (PISO
   corrector). Two correctors are the default; the number is a YAML option.
6. Advance to \(n+1\).

Backward Euler is the first time integrator; Crank–Nicolson is a later option.
The convection can be explicit or semi-implicit; diffusion is implicit and
solved with PETSc. The vector momentum system uses block size 3 or three
right-hand sides.

A Chorin projection is the one-corrector limit of PISO; it is useful for
debugging but not the production path.

## 4. Boundary conditions

- no-slip and moving walls;
- inlet velocity profile;
- outlet pressure or zero-gradient velocity;
- symmetry;
- periodic patches;
- `empty` for the inactive direction of a 2D mesh.

Every boundary face has exactly one BC. The pressure-correction equation uses
homogeneous Neumann on walls and a fixed value or zero-gradient on open
boundaries.

## 5. Validation

### Lid-driven cavity

Unit square, top lid at \(U=1\), other walls no-slip. Compare the vertical
centreline \(u(y)\) and horizontal centreline \(v(x)\) against Ghia, Ghia &
Shin (1982) at \(Re=100\), then \(Re=1000\). Report RMS and maximum deviation,
the minimum location and the zero-crossing.

The v0 implementation reached RMS 0.0024 at \(Re=100\), 128², with a staggered
projection. v1 must reproduce or improve that on the collocated PISO path.

### Taylor–Green vortex

Periodic 2D/3D decaying vortex. Check the kinetic-energy decay rate and the
convergence under refinement. It is a clean test of convection, diffusion and
pressure coupling without walls.

### Channel flow

Plane channel at a target friction Reynolds number (later); first validate the
laminar parabolic profile.

## 6. Diagnostics

Every step or output interval:

- maximum velocity and kinetic energy;
- \(\|\nabla\cdot\mathbf{u}\|_2\) and \(\|\nabla\cdot\mathbf{u}\|_\infty\);
- pressure mean and range;
- momentum residual, pressure-correction residual, PETSc iterations;
- CFL and active time-step limit.

## 7. Observations (to be filled from runs)

Record:

- the divergence norm with and without Rhie–Chow;
- the PISO corrector count needed for a stable solution;
- the effect of the limiter on the cavity centreline;
- the cost of the pressure solve relative to one time step;
- the difference between backward Euler and Crank–Nicolson;
- any checkerboard or pressure drift at the lid corner.

## References

- Chorin (1968).
- Patankar & Spalding (1972).
- Rhie & Chow (1983).
- Issa (1986).
- Ghia, Ghia & Shin (1982).
- Bell, Colella & Glaz (1989).
- Ferziger & Perić (2002).
