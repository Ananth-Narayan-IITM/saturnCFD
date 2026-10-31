# Solver implementation sequence

**Status:** accepted. Phase 0 (PETSc) and the explicit part of Phase 1
(solver lifecycle, registry, time loop, SSP-RK) are implemented. Implicit
BE/CN/BDF2 arrive with the PETSc diffusion solve in Burgers/heat. Burgers is
next.

This page records the order, decisions and exit criteria for the solver work.
It is the working checklist; the theory pages contain the derivations.

## Decision summary

1. **Burgers first**, then heat, then Stokes, then incompressible
   Navier–Stokes.
2. **PETSc first** as a small linear-algebra phase, before any solver depends
   on it. The library target is **`saturnLinAlg`**.
3. **Burgers starts 1D scalar**, then 2D and 3D scalar, then vector Burgers.
4. **Advanced convection waits** until Burgers is validated. Current schemes
   are upwind, Rusanov and MUSCL/TVD with minmod, van Leer and superbee.
5. **Implicit diffusion uses PETSc**; explicit Burgers is the first numerical
   validation, then IMEX/backward-Euler diffusion.
6. Stokes and Navier–Stokes use **pressure–velocity coupling**; that is not
   part of the Burgers/heat phases.
7. The PETSc solver wrapper is `saturn::KspSolver`; no home-grown Krylov
   solver.

## Why Burgers first

- It has an exact viscous traveling-wave solution.
- It exercises nonlinear convection, limiters, time integration, diffusion,
  boundary ghosts, conservation and MPI.
- It is the bridge to Navier–Stokes: same convective term and diffusion, no
  pressure or continuity.
- A failure is local to the scalar operator/time/BC layer, not ambiguous with
  saddle-point pressure coupling.
- The project documentation already defines it as the first solver and the
  `traveling64` regression.

Heat comes second because it is linear and isolates implicit diffusion and
PETSc. Stokes comes after both because it adds the saddle-point system,
pressure nullspace and fieldsplit/Schur preconditioning.

## Phase 0 — PETSc linear algebra

**Status:** done.

Scope:

- CMake PETSc discovery: `find_package(PETSc)` then `pkg-config` fallback.
- `saturnLinAlg` with `PetscScope`, `petscCheck` and `KspSolver`.
- PETSc Poisson smoke tests on 1/2/4 ranks:
  - 1D Dirichlet, order 2;
  - 3D Dirichlet, CG + GAMG;
  - all-Neumann nullspace.
- No finite-volume matrix assembly yet; `DMDA` is a smoke-test only.

Exit criteria: `ctest --preset petsc` passes; PETSc version reported.

## Phase 1 — Solver lifecycle and time loop

**Status:** explicit part done.

Implemented:

- `Solver` interface: state, `rhs`, boundary refresh, wave speed, diffusivity,
  minimum cell size, dimension.
- `SolverRegistry` with factory registration and name lookup.
- `Simulation` loop: initialize, initial conditions, boundary refresh, stage
  updates, diagnostics, final state.
- `TimeControl`: dt, CFL, diffusion safety, adaptive stability limit, max/min
  dt, max steps.
- Explicit schemes: forward Euler, SSPRK2, SSPRK3.
- Diagnostics: L1, L2, Linf, CFL, diffusive number, step, time, dt.
- Test solver: periodic scalar advection–diffusion, MUSCL + local Laplacian.
- Tests: constant-field preservation, cosine diffusion decay, bounded sine
  advection, stability-limit selection, registry, implicit-scheme rejection
  on 1/2/4 ranks.

Pending:

- backward Euler, Crank–Nicolson and BDF2 need the implicit diffusion solve;
  add them with `saturnLinAlg` during Burgers/heat.
- Output cadence/checkpoint and run manifest.

Exit for the explicit slice is met: a periodic advection–diffusion case runs
through the loop on 1/2/4 ranks.

## Phase 2 — Burgers

**Status:** 1D scalar solver `saturnBurger` implemented and tested. 2D/3D and
implicit diffusion are next.

**Start with 1D scalar conservative Burgers:**

$$
\partial_t u + \partial_x\left(\frac{u^2}{2}\right)
= \nu\partial_{xx}u + s .
$$

Implemented:

- `src/solvers/saturnBurger/` target `saturnBurger`;
- `burgersFlux` with canonical face orientation and Rusanov/local
  Lax–Friedrichs;
- MUSCL/TVD reconstruction with minmod, van Leer and superbee;
- explicit SSPRK3 (forward Euler and SSPRK2 also available);
- Dirichlet and periodic boundaries; Neumann is next;
- time-dependent Dirichlet data via `Solver::setTime`;
- shared non-orthogonal Laplacian for diffusion;
- constant source;
- registered CLI solver with a YAML case, CSV profile and JSON summary.

Validation (viscous traveling wave, `U=1`, `nu=0.02`, `t=0.5`):

| case | result |
|---|---|
| travelling wave, N=32 -> 1024 | L2 9.015e-3 -> 7.287e-6, observed orders 1.98/2.11/2.10/2.06/2.03 |
| constant field | preserved exactly |
| periodic inviscid shock, $u_0=-\sin(\pi x)$, N=128, t=0.5 | bounds [-0.9954, 0.9954], integral 1.5e-16 |
| 1/2/4 ranks at N=256 | bitwise-identical L2 1.236378e-4 |
| YAML CLI path | `ctest --preset burgers` passes on 1/2/4 ranks |

The earlier rank-dependent traveling-wave error was a test-diagnostic bug: the
test divided by the local owned-cell count. The solver was already rank
independent. See [`../validation/burgers.md`](../validation/burgers.md) and the
figure `docs/validation/figures/burgersValidation.png`.

Still to add:

- limiter comparison table (upwind/minmod/MC/van Leer/superbee);
- shock-speed error in cells;
- Neumann boundary condition;
- 2D and 3D scalar Burgers;
- vector Burgers before Navier–Stokes;
- IMEX BE/CN with implicit diffusion through `saturnLinAlg`.

Exit for the 1D slice is met: traveling wave and shock tests pass on 1/2/4
ranks, the YAML regression runs, and the analytic comparison is plotted. The
full Phase 2 exit waits for 2D/3D.

## Phase 3 — Heat equation

$$
\partial_t T + \nabla\cdot(\mathbf{a}T)
= \nabla\cdot(\alpha\nabla T) + S .
$$

- Prescribed velocity field.
- Implicit diffusion via PETSc; MUSCL convection.
- BE first, then CN/BDF2.
- Dirichlet/Neumann, transient and steady.

Validation: 1D erf solution, 2D/3D MMS, boundary-layer profile, 1/2/4 ranks.

Exit: second-order heat tests; `ctest --preset heat` passes.

## Phase 4 — Stokes and pressure–velocity coupling

$$
-\nabla p + \mu\nabla^2\mathbf{u}=\mathbf{f},
\qquad \nabla\cdot\mathbf{u}=0.
$$

- Collocated FVM and Rhie–Chow face flux.
- Coupled saddle-point system or Schur complement with PETSc fieldsplit.
- Pressure nullspace for all-Neumann.
- GMRES + fieldsplit/Schur; pressure PC GAMG/HYPRE.

Validation: Poiseuille/Couette exact, cavity $Re\to0$, discrete divergence,
pressure mean/nullspace, 1/2/4 ranks.

Exit: Stokes tests; fieldsplit converges; nullspace handled.

## Phase 5 — Incompressible Navier–Stokes

- PISO transient, SIMPLE steady; Rhie–Chow.
- Cavity $Re=100,1000$ against Ghia et al.
- Taylor–Green energy decay and order.

## Numerical choices

| term | first choice | later |
|---|---|---|
| diffusion matrix | orthogonal part in PETSc matrix; non-orthogonal correction deferred | full correction in matrix/iterated |
| time | BE for first implicit step | CN, BDF2 |
| explicit convection | SSP-RK3 | — |
| IMEX | BE/CN convection-explicit, diffusion-implicit | higher-order IMEX |
| convection | MUSCL + minmod/van Leer/MC | WENO5 after Burgers validation |
| pressure | — | PETSc fieldsplit/Schur |

Advanced convection (WENO5, MUSCL-Hancock, HLLC/AUSM) is deliberately deferred
until Burgers is validated. Adding schemes before a validated baseline makes
debugging harder.

## Open decisions

1. Burgers 1D scalar first (recommended) versus 3D vector first.
2. IMEX BE versus fully implicit Newton for the first implicit Burgers step.
3. Whether the first production matrix assembler uses a contiguous local
   numbering with an owner map or PETSc `DMDA` for the structured block.
4. Pressure coupling for Stokes: coupled fieldsplit (recommended) versus a
   projection/SIMPLE first implementation.
