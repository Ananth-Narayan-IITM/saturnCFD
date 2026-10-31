# saturnBurger — 1D scalar Burgers solver

**Target:** `saturnBurger`  
**Class:** `saturn::SaturnBurger`  
**Equation:** 1D scalar Burgers

$$
\frac{\partial u}{\partial t}
+ \frac{\partial}{\partial x}\left(\frac{u^2}{2}\right)
= \nu\frac{\partial^2 u}{\partial x^2} + s .
$$

This is the first production solver. The 2D/3D extension will keep the
`saturnBurger` name and add the transverse directions.

## Files

```text
src/solvers/saturnBurger/
  CMakeLists.txt       target saturnBurger
  SaturnBurger.hpp     options and solver class
  SaturnBurger.cpp     FVM assembly, boundary fill, RHS
  README.md            this page
```

## Mesh and layout

Requirements:

- a structured 1D block: `mesh.dimension == 1`, `ny = nz = 1`;
- `computeGeometry()` and `detectStructured()` already called;
- at least two cells per rank when `nGrow = 2`.

The solver constructs:

```text
MeshPartition  (nx, 1, 1, periodic, false, false)
LocalMesh      nGrow = 2
CellField u    nGrow = 2
FaceField flux, lapFace
CellField div, lap, grad
```

The shared FVM layer supplies `LocalMesh`, `FieldHalo`, `fillBoundaryGhosts`,
`divergence`, and `laplacian`.

## Discretisation

### Convective flux

Burgers is a scalar conservation law with flux

$$
f(u) = \frac{u^2}{2}, \qquad f'(u)=u.
$$

`burgersFlux` in `src/fv/Convection.cpp` computes the **outward normal flux**.
The canonical face orientation is "lower-coordinate cell on the left,
higher-coordinate cell on the right", independent of which cell owns the face.
That makes the flux antisymmetric across a shared face, which is required for
conservation.

Schemes:

| scheme | face states | flux |
|---|---|---|
| `Rusanov` | cell values | local Lax–Friedrichs |
| `Muscl` | MUSCL-reconstructed left/right | local Lax–Friedrichs |
| `Upwind` | cell values | Godunov (shock/rarefaction) |

The default is `Muscl` with the van Leer limiter. The MUSCL reconstruction
needs two ghost layers; the solver enforces that.

### Diffusion

Diffusion uses the shared non-orthogonal corrected `laplacian` on `LocalMesh`:

$$
\frac{1}{V_P}\sum_f \nu A_f
\frac{(\phi_N-\phi_P)-\mathbf{d}\cdot\nabla_t\phi_f}{d_\perp}.
$$

For the 1D block this reduces to the three-point stencil. `nu = 0` is allowed
for the inviscid equation.

### Source

`BurgersOptions::source` is a constant source per unit volume. A spatially
varying manufactured source is a later extension.

## Time integration

`SaturnBurger` implements `Solver::rhs`; `Simulation` owns the time loop.
Implemented schemes are forward Euler, SSPRK2 and SSPRK3. The recommended
choice is **SSPRK3** with a CFL around 0.3.

`TimeControl::stableDt` uses

$$
\Delta t_{\mathrm{conv}} = \mathrm{CFL}\frac{h_{\min}}{\max|u|},
\qquad
\Delta t_{\mathrm{diff}} = \frac{s h_{\min}^2}{2d\nu}.
$$

For $\nu>0$ the diffusive limit can dominate on fine meshes. The explicit
diffusion solve is the current implementation; implicit/IMEX diffusion through
`saturnLinAlg` is the next step.

The solver implements `setTime`, so time-dependent Dirichlet data at a moving
exact boundary is refreshed after every RK stage.

## Boundary conditions

- **Periodic:** `BurgersOptions::periodic = true`; only `FieldHalo`.
- **Dirichlet:** set `dirichlet(x, time, component)`; the callback is evaluated
  at the face centroid and the ghost is the mirror `2g - u_P`.
- **Neumann:** not exposed in the first solver; add it by extending
  `BurgersOptions` and `fillBoundaryGhosts`.

The 1D mesh may still carry `ymin`/`ymax`/`zmin`/`zmax` patches. The solver
loops only the active direction, so those patches are ignored.

## Initial condition

`BurgersOptions::initial(x)` is required and is evaluated at cell centroids.
`applyInitialConditions` fills the host view and refreshes ghosts.

## Validation

`tests/TestSaturnBurger.cpp`:

| case | result |
|---|---|
| viscous traveling wave, $N=64,128$, $\nu=0.02$, $t=0.5$ | L2 **0.002284 → 0.000529**, observed order **2.11** |
| constant field, periodic, inviscid | stays exactly 1 |
| periodic inviscid shock, $u_0=-\sin(\pi x)$, $t=0.5$ | bounds **[-0.9954, 0.9954]**, integral **1.5e-16** |
| Burgers constant flux | zero divergence, face flux $-0.5/+0.5$ |

The traveling wave uses the exact solution

$$
u(x,t)=\frac{U}{2}\left[
1-\tanh\left(\frac{U(x-Ut/2)}{4\nu}\right)\right],
\qquad U=1.
$$

The shock test checks the TVD/boundedness property and discrete conservation.

## Why this solver first

- It has an exact viscous solution.
- It exercises nonlinear convection, limiters, time integration, diffusion,
  boundary ghosts, conservation and MPI.
- It is the bridge to Navier–Stokes: same convective and diffusive terms, no
  pressure or continuity.

## Limitations and next steps

- 1D only; `ny` and `nz` must be 1.
- Constant source only.
- Explicit diffusion; implicit/IMEX BE/CN/BDF2 waits for the FV Poisson matrix
  assembler and `saturnLinAlg`.
- The shock test is conservative and bounded but does not yet report a
  pointwise shock-speed order.
- The traveling-wave L2 error is small and order-consistent, but differs
  slightly between 1, 2 and 4 ranks. Partition-face MUSCL parity is a pending
  refinement.
- 2D/3D extensions will add transverse flux components and a vector or
  direction-dependent Burgers flux.

## Using the solver

```cpp
BurgersOptions options;
options.nu = 0.02;
options.periodic = false;
options.convection = ConvectiveScheme::Muscl;
options.limiter = Limiter::VanLeer;
options.initial = [](const Vec3& x) { return exact(x.x, 0); };
options.dirichlet = [](const Vec3& x, Real t, int) {
  return exact(x.x, t);
};

SaturnBurger solver(std::move(mesh), options);
TimeControl control;
control.tEnd = 0.5;
control.cfl = 0.3;

Simulation simulation(solver, control, TimeScheme::SSPRK3);
const SimulationResult result = simulation.run();
```

CLI/YAML wiring for `saturnCFD case.yaml` is a later step; the solver is
currently exercised through `testSaturnFV`.
