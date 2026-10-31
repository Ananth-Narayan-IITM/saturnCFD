# Reference: v0

The v0 implementation is preserved unchanged:

| | |
|---|---|
| branch | `v0-archive` |
| tag | `v0.3.0` |
| commit | `a5bf1fbcf0137445a5777bf7306a47a9d167fbb3` |
| remote | `origin/v0-archive`, `origin/v0.3.0` |
| status | validated cavity + substrate; CPU/GPU builds documented |

v1 is not a continuation of v0's structured AMR substrate. v1 uses a
structured hexahedral finite-volume mesh read from `.msh`, collocated
cell-centred variables and PETSc. Use v0 as a source of **ideas, algorithms and
validation data**, not as a tree to copy.

## How to inspect v0

```bash
git show v0-archive:src/amr/MultiFab.hpp
git log v0-archive --oneline
git worktree add ../saturnCFD-v0 v0-archive   # full v0 tree side by side
```

## Disposition of v0 modules

| v0 path | v1 disposition | reason |
|---|---|---|
| `src/core/Types.hpp` | **port**, review | `Real`/`Accum`/`NDIM`/Kokkos aliases are a good base |
| `src/core/Config.*` | **port the idea**, rewrite | self-registering options and `--dump-config` are right; v1 needs a schema and strict validation |
| `src/core/Timer.*` | **port** | fenced timing is correct for GPUs |
| `src/amr/Box.hpp` | **do not port as core** | v1 uses `StructuredMesh` cell/face ids, not AMR boxes |
| `src/amr/Array4.hpp` | **do not port as core** | v1 fields are cell/face indexed, not patch/ghost indexed |
| `src/amr/BoxArray.*` | **do not port** | no block-structured AMR in v1.0 |
| `src/amr/DistributionMapping.*` | **do not port** | no AMR; MPI distribution is a later concern |
| `src/amr/MultiFab.*` | **do not port** | structured mesh with `CellField`/`FaceField` replaces it |
| `src/comm/HaloExchange.*` | **reference only** | the two-phase buildCPU/execute idea is useful for MPI later |
| `src/mesh/Geometry.hpp` | **reference only** | v1 has a `StructuredMesh` with volumes/areas/face vectors |
| `src/linalg/Poisson.*` | **replace** | PETSc; no home-grown CG |
| `src/linalg/VecOps.hpp` | **reference only** | v1 uses PETSc vectors and explicit field kernels |
| `src/solver/CavitySolver.*` | **reference for validation only** | the Ghia numbers and the staggered/checkerboard lesson are useful |
| `src/solver/Limiters.hpp` | **port** | TVD face reconstruction is grid-independent |
| `src/io/VTKWriter.*` | **port the idea**, rewrite | v1 writes `.vtu` for hex meshes, not `.vti` patches |
| `apps/main.cpp` | **rewrite** | v1 needs a solver registry and rank-aware config |
| `tests/` | **port the harness and analytic tests** | reuse the test style; add mesh and PETSc tests |
| `CMakeLists.txt` | **rewrite** | add PETSc, yaml-cpp, VTU output |
| `external/kokkos` | **decide separately** | submodule/FetchContent/pinned vendored copy |
| `docs/` | **reference only** | v1 docs start from `Design-v1.md`, `plan/`, `theory/` |

## Known v0 issues — do not carry forward

1. **Halo ghost-count mismatch.** `mPressure` has 1 ghost, but the cavity's
   `HaloExchange` is built with 2. Single-box, non-periodic runs hide it.
2. **Periodic boundaries can be overwritten** by `fillBoundaryGhosts`, which
   does not skip periodic dimensions.
3. **The cavity is square-only.** `CavitySolver` uses `nCells(0)` for both
   directions while the driver accepts an independent `domain.ny`.
4. **`localSum` is serial inside a box.** It reduces over boxes, not cells.
5. **Allocations inside the time loop.** Three `MultiFab`s per Poisson solve,
   plus a `boxArrays()` view per vector operation.
6. **The Poisson result is ignored.** An unconverged pressure solve is silent.
7. **Float tolerance is silently clamped** to `100*epsilon`.
8. **`localMin`/`localMax` include ghosts**, unlike `localSum`.
9. **MPI is compiled but not driven.** The driver and solver hardcode rank 0.
10. **Version/doc drift.** CMake 0.3.0, `main.cpp` 0.2.0, README v0.2, Scope
    still describing a collocated scheme.

## What v1 should take from v0

- the validation discipline: analytic solutions, manufactured solutions,
  regression tables, observed order;
- the Ghia cavity data and the lessons about checkerboard modes and explicit
  diffusion limits;
- the TVD limiter implementation as a reference;
- the self-registering configuration idea, upgraded to a typed schema;
- the GPU portability lessons: functor storage, no `this` in device lambdas,
  no host access to device memory;
- the fenced `Timer`.

## What v1 must not take from v0

- the structured AMR `BoxArray`/`MultiFab` data model;
- the single-box, rank-0 solver assumptions;
- the home-grown CG solver;
- the per-solve allocations and serial reductions;
- the mixed-component staggered storage.
