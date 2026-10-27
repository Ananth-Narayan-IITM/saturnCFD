# saturnCFD — Frozen Scope, v0.1 (coursework deliverable)

**Deadline: 31 Oct 2026.** Owner: solo. Graded on quality of the *coding* for an
Advanced CFD course.

## What v0.1 IS

| Aspect | Decision |
|---|---|
| Physics | Single-phase, incompressible, constant density, Newtonian |
| Dimension | 2D primary; 3D supported by the same code (`SATURN_NDIM=3`) |
| Grid | Uniform Cartesian **within** a block, block-structured AMR hierarchy |
| AMR | Block-structured (Berger–Colella), 2:1 ratio, patch-based. v0.1: levels implemented, 2 levels demonstrated |
| Discretisation | Collocated cell-centred + face fluxes, Bell–Colella–Glaz style projection (Basilisk lineage) |
| Pressure | Geometric multigrid on the patch hierarchy (v0.1 may ship PCG fallback) |
| Parallel | Kokkos (CUDA / HIP / SYCL / OpenMP / Serial). MPI optional at build time |
| Precision | Templated on `Real`; default `double`; switch at CMake configure time |
| I/O | XML VTK (`.vti`) + `.pvd` collection for ParaView; raw binary for Python |
| Config | YAML subset, with `--dump-config` emitting a fully commented template |
| Boundaries | Cut-cells **deferred to v0.2**. v0.1: rectangular domain, periodic / wall / inflow-outflow / symmetry |

## What v0.1 is NOT (explicitly out of scope)

Multiphase / VOF, surface tension, heat transfer, cut-cells, embedded or moving
bodies, LES, compressibility, phase change, stretched or curvilinear grids.
These are v0.2+ and the architecture must not preclude them — but they are not
built now.

## Acceptance criteria

1. **Compiles** with `Kokkos` Serial, OpenMP and CUDA backends from one source.
2. **Correct:** lid-driven cavity vs Ghia et al. (1982) at Re = 100 and 1000,
   within a stated tolerance; Taylor–Green vortex shows ~2nd-order convergence.
3. **Parallel:** weak and strong scaling data on the cluster (and OpenMP
   thread scaling on the laptop), with efficiency curves.
4. **Comparable:** side-by-side against Basilisk and/or OpenFOAM for at least
   one case, same resolution, same quantities.
5. **Engineered:** unit tests, no leaks, deterministic output, YAML template
   dump, README that a stranger can follow.

## Explicit non-goal

Beating Basilisk on 31 Oct 2026. v0.1 exists to prove the *architecture* is
sound and the *code* is good. Performance parity is a v1.0 target.

## Licensing

Apache-2.0. Chosen so saturnCFD stays permissive and Kokkos-compatible.
**Consequence: no code may be copied from Basilisk (GPL).** Algorithms may be
reimplemented from papers and from reading the source. Any borrowed formula
gets a citation in the source comment.
