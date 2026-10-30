# saturnCFD v1 — design

**Status: draft. No architecture decision below is frozen.**

This document is the place to settle the v1 idea before code is written. It
records the constraints inherited from v0, the principles that should survive
the rewrite, and the decisions that still need an owner.

---

## 1. Mission

A single codebase that:

1. solves incompressible, variable-density-ready finite-volume flow on a
   block-structured AMR hierarchy;
2. runs from one source on CPU (Serial/OpenMP) and GPU (CUDA/HIP/SYCL);
3. is validated against published benchmarks, not only "it runs";
4. is readable enough that a new contributor can follow one timestep from the
   driver to the last kernel.

The coursework v0 is the reference implementation, not the starting point.

## 2. What v1 inherits from v0

Not code — **constraints and lessons**:

- Kokkos is the portability layer; there is no hand-written CUDA path.
- Field storage is patch-based and flat; kernels iterate patches, not an
  object graph.
- Ghost exchange is resolved once on the host into copy operations, then
  executed as kernels plus packed MPI.
- Staggered (MAC) velocities, cell-centred pressure, projection method.
- Device code cannot touch host objects: no `this`, no host-side `Array4`
  reads, no device-view indexing from the host.
- An explicit scheme must not sit exactly on its stability limit (the v0
  diffusion-factor bug).
- A "safety factor" at the limit is the boundary between decaying and
  undamped, not a conservative margin.

## 3. Design principles

1. **One source, many backends.** Any backend-specific code is a bug or a
   documented exception.
2. **Device-first.** The host builds metadata; the device does arithmetic.
   Host/device crossings are explicit and counted.
3. **No hidden allocation in the time loop.** Workspaces are owned by the
   solver and reused. Allocation is a setup-time event.
4. **Explicit ownership.** Every field says which level, which patches, how
   many ghosts, and which rank owns it.
5. **Validation gates.** A module is not "done" until it passes its
   manufactured-solution or analytic test on every supported backend.
6. **Small interfaces.** A solver depends on fields, a halo operator and a
   linear solver — not on the driver, the file format or the AMR machinery.

## 4. Candidate layering

```
core/      types, configuration, timers, error handling
amr/       Box, BoxArray, DistributionMapping, MultiFab, Array4, hierarchy
comm/      halo exchange, reductions, MPI wrappers
mesh/      geometry, boundary conditions, cut-cell / EB hooks (later)
linalg/    multigrid, Krylov fallback, vector operations
solver/    projection solver, advection/diffusion operators
io/        VTK, diagnostics, checkpoints
apps/      one driver, case-file driven
```

This is close to v0's layering on purpose: v0's decomposition was sound. The
change is in what lives behind the interfaces, not in the layer names.

## 5. Milestones

| id | milestone | exit criterion |
|---|---|---|
| M0 | design freeze | this document has no open decisions |
| M1 | substrate parity | Box/MultiFab/halo pass the v0 tests on CPU and GPU |
| M2 | multigrid pressure | V-cycle beats CG in iterations and wall time on 128²–512² |
| M3 | multi-box/MPI flow | cavity runs on ≥2 boxes and ≥2 ranks, same answer |
| M4 | 3D flow | 3D cavity or Taylor–Green at second-order convergence |
| M5 | two-level AMR | tagging, regrid, prolong/restrict, reflux on one benchmark |

M2 is the highest-value item: v0's GPU is launch-bound because CG needs ~200
iterations per timestep. M3 is the highest-credibility item: the MPI substrate
exists in v0 but no application drives it.

## 6. Validation gates

Every milestone must keep these green:

- `Box` algebra and tiling invariants;
- halo exchange against an analytic periodic field, including edges/corners;
- Poisson manufactured solutions: Dirichlet second-order, Neumann zero-mean;
- lid-driven cavity Re = 100 against Ghia et al. (1982), RMS and max deviation;
- substrate stencil bandwidth, so performance regressions are visible;
- CPU/GPU agreement within the precision floor.

## 7. Open decisions

These need answers before M1 starts:

1. **Target physics.** Incompressible single-phase only, or variable density /
   multiphase hooks from day one?
2. **AMR.** Two-level Berger–Colella in v1, or single-level v1 with AMR left
   to v1.1? This changes the field and halo interfaces.
3. **MPI.** Required in the first release, or "single rank first, MPI behind
   the same interface"?
4. **Dimension.** 2D-first with compile-time 3D, or 3D-first?
5. **Pressure solver.** Geometric multigrid only, or MG-preconditioned CG with
   a CG fallback?
6. **API stability.** Is v0's `MultiFab`/`Array4` API allowed to change, or
   should v1 remain source-compatible for the examples?
7. **Deadline and audience.** Coursework submission, research code, or a
   public library? This decides how much polish versus how much exploration.

## 8. Non-goals for the first v1 release

- compressibility, heat transfer, phase change;
- LES/DNS turbulence modelling;
- cut-cells or embedded boundaries;
- a GUI or a Python front end;
- performance parity with a mature code on day one.

These can be added later, but the first release should be small enough to
validate completely.
