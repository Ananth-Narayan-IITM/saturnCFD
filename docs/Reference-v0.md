# Reference: v0

The v0 implementation is preserved unchanged:

| | |
|---|---|
| branch | `v0-archive` |
| tag | `v0.3.0` |
| commit | `a5bf1fbcf0137445a5777bf7306a47a9d167fbb3` |
| remote | `origin/v0-archive`, `origin/v0.3.0` |
| status | validated cavity + substrate; CPU/GPU builds documented |

Nothing needs to be copied wholesale. Port a module only after the v1 design
for that module is settled, and keep the v0 version as the oracle until the new
one passes the same tests.

## How to inspect v0

```bash
git show v0-archive:src/amr/MultiFab.hpp
git log v0-archive --oneline
git diff v0-archive -- src/               # v1 vs v0, once code exists

# full v0 tree side by side, for running benchmarks
git worktree add ../saturnCFD-v0 v0-archive
```

To port one file deliberately:

```bash
git checkout v0-archive -- src/core/Types.hpp
# then review it against docs/Design-v1.md before committing
```

## Disposition of v0 modules

| v0 path | v1 disposition | reason |
|---|---|---|
| `src/core/Types.hpp` | **port**, review | `Real`/`Accum`/`NDIM`/Kokkos aliases are the right foundation |
| `src/core/Config.*` | **port** | YAML subset and self-registering `--dump-config` work well |
| `src/core/Timer.*` | **port** | fenced timing is correct for GPUs |
| `src/amr/Box.hpp` | **port** | inclusive index-space box, device-callable |
| `src/amr/Array4.hpp` | **port**, review | global-index accessor is the right abstraction |
| `src/amr/BoxArray.*` | **port** | tiling and bounding box are stable |
| `src/amr/DistributionMapping.*` | **port**, extend | greedy LPT is fine for uniform patches; add space-filling curve later |
| `src/amr/MultiFab.*` | **rewrite behind the same idea** | flat patch storage is right; fix reductions, workspaces and the ghost contract |
| `src/comm/HaloExchange.*` | **port the design, rewrite details** | two-phase build/execute is right; fix the ghost-count and periodic-BC bugs |
| `src/mesh/Geometry.hpp` | **port**, extend | add a hierarchy descriptor for AMR |
| `src/linalg/Poisson.*` | **replace** | CG is the bottleneck; keep only as a reference/fallback |
| `src/linalg/VecOps.hpp` | **port**, revise | avoid per-operation `boxArrays()` allocation |
| `src/solver/CavitySolver.*` | **rewrite** | make it a general projection solver, not a single-box cavity class |
| `src/solver/Limiters.hpp` | **port** | TVD face reconstruction is independent of the grid layer |
| `src/io/VTKWriter.*` | **port**, extend | add pressure, binary output, multi-rank parts |
| `apps/main.cpp` | **rewrite** | make the driver rank-aware and solver-registry driven |
| `tests/` | **port** | harness and analytic tests are reusable; add regression cases |
| `CMakeLists.txt` | **rewrite** | keep the one-switch idea; decide Kokkos vendoring vs submodule |
| `external/kokkos` | **do not copy blindly** | pick a versioning strategy before the first build |
| `docs/` | **reference only** | v0 docs are useful evidence; v1 docs start from `Design-v1.md` |

## Known v0 issues — do not carry these forward

1. **Halo ghost-count mismatch.** `mPressure` has 1 ghost, but the cavity's
   `HaloExchange` is built with 2 (`src/solver/CavitySolver.cpp:205,209`).
   Single-box, non-periodic runs hide it; a multi-box run would write out of
   bounds through `Poisson.cpp:61`.
2. **Periodic boundaries are overwritten.** `fillBoundaryGhosts`
   (`src/linalg/Poisson.cpp:10-53`) reflects every ghost outside the domain and
   does not skip periodic dimensions, so it can corrupt a periodic halo.
3. **The cavity is square-only.** `CavitySolver` uses `nCells(0)` for both
   directions (`src/solver/CavitySolver.cpp:183`), while the driver accepts an
   independent `domain.ny` (`apps/main.cpp:307-310`).
4. **`localSum` is serial inside a box.** It reduces over boxes, not cells
   (`src/amr/MultiFab.cpp:154-185`), so a single-patch field sums on one
   thread. This is used for pressure mean removal and checksums.
5. **Allocations inside the time loop.** `solvePoisson` constructs three
   `MultiFab`s per call (`src/linalg/Poisson.cpp:87-89`), and each `VecOps`
   call creates a `boxArrays()` view. Use persistent workspaces in v1.
6. **The Poisson result is ignored.** `CavitySolver::step` discards
   `PoissonResult` (`src/solver/CavitySolver.cpp:338`), so an unconverged
   pressure solve is silent.
7. **Float tolerance is silently clamped.** `poissonTol` is raised to
   `100*epsilon` (`src/solver/CavitySolver.cpp:215-217`); in float that is
   ~1.2e-5, so a requested 1e-6 is not honored. Log it if v1 keeps the clamp.
8. **`localMin`/`localMax` include ghosts** (`src/amr/MultiFab.cpp:198-240`),
   unlike `localSum`.
9. **MPI is compiled but not driven.** The driver and solver hardcode rank 0
   and one rank (`apps/main.cpp:175,188`, `src/solver/CavitySolver.cpp:199,209`).
10. **Version/doc drift.** CMake says 0.3.0, `main.cpp` printed 0.2.0, the
    README said v0.2, and `docs/Scope.md` still described a collocated scheme.

## Porting order

Start with the pieces that have no design debt:

1. `core/Types.hpp`, `core/Timer.hpp`, `core/Config.*`
2. `amr/Box.hpp`, `amr/Array4.hpp`, `amr/BoxArray.*`
3. `amr/DistributionMapping.*`
4. `amr/MultiFab.*` — rewrite allocations and reductions while porting
5. `comm/HaloExchange.*` — fix the contract before adding MPI
6. `tests/` — keep the harness and move the analytic tests with each module

Do not port `Poisson` or `CavitySolver` until M2/M3 are designed.
