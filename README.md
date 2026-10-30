# saturnCFD v1

A fresh rewrite of saturnCFD: finite-volume incompressible CFD with
block-structured AMR, performance-portable through Kokkos.

**Status: design.** This branch intentionally contains no solver code yet. The
validated v0 implementation is preserved unchanged on the `v0-archive` branch
(tag `v0.3.0`) and is the reference for physics, performance and regression
tests.

## Why a v1

v0 proved the architecture and collected the hard lessons: the flat
`MultiFab` layout, the team/flat kernel dispatch, the two-phase halo exchange,
the staggered projection solver, and the CUDA portability traps. It also
exposed the limits worth fixing in a clean design:

- conjugate gradients is the pressure-solve bottleneck — multigrid is required;
- the flow solver bypasses the multi-box / MPI substrate;
- the physics is 2D-only despite the 3D-capable data structures;
- there is no AMR hierarchy yet;
- several latent ghost/periodicity issues are hidden by the single-box cavity.

v1 starts from those lessons rather than from the v0 source tree. Code is
ported deliberately, module by module, with the v0 implementation available as
an oracle and benchmark.

## Start here

- [`docs/Design-v1.md`](docs/Design-v1.md) — goals, principles, milestones and
  open decisions.
- [`docs/Reference-v0.md`](docs/Reference-v0.md) — what to reuse, rewrite or
  leave behind, with exact v0 paths and commit.

## Inspecting v0

The old tree is one command away; nothing was deleted.

```bash
git show v0-archive:src/amr/MultiFab.hpp
git worktree add ../saturnCFD-v0 v0-archive   # full v0 tree side by side
```

## License

Apache-2.0. See [`LICENSE`](LICENSE).
