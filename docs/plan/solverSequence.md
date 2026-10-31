# Solver implementation sequence

**Status:** accepted. Phase 0 (PETSc linear algebra) implemented. Burgers is
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

- `Solver` interface and registry.
- `Simulation` loop: initialize, step, diagnose, write.
- `TimeControl`: dt, CFL, adaptive limits, active-limit diagnostic.
- Time schemes:
  - backward Euler;
  - Crank–Nicolson;
  - BDF2 (started with BE);
  - SSP-RK2/RK3 for explicit convection.
- Diagnostics: L1/L2/Linf, conservation, divergence, CFL, iterations.

Exit: a trivial advection/diffusion case runs through the loop on 1/2/4 ranks.

## Phase 2 — Burgers

**Start with 1D scalar conservative Burgers:**

$$
\partial_t u + \partial_x\left(\frac{u^2}{2}\right)
= \nu\partial_{xx}u + s .
$$

Method:

- MUSCL/TVD reconstruction of $u$; face flux $\frac12 u_f^2$;
- Rusanov fallback for robustness;
- explicit SSP-RK3 first;
- then IMEX BE/CN: convection explicit, diffusion implicit with PETSc;
- Dirichlet, Neumann and periodic boundaries.

Validation:

| case | criterion |
|---|---|
| viscous traveling wave, $\nu=10^{-2},10^{-3}$ | L1/L2/Linf decrease; order near scheme order |
| refinement $N=32,64,128,256$ | convergence table |
| inviscid smooth data before shock | characteristics agreement |
| inviscid shock | shock within one cell; conservation error small |
| limiter comparison | upwind vs minmod/MC/van Leer/superbee |
| 1/2/4 ranks | same result within roundoff |
| regression `traveling64` | stored norms |

Then extend the same solver to 2D and 3D scalar Burgers. Vector Burgers comes
after scalar validation and before Navier–Stokes.

Exit: traveling wave and shock pass; `ctest --preset burgers` passes.

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
