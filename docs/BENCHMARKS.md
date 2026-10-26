# Benchmark notes — v0.1 substrate

These numbers characterise the **parallel substrate** (kernel dispatch, field
layout, halo exchange), not a flow solver. They exist to justify two design
choices before any physics is written.

## Machine

| | |
|---|---|
| CPU | AMD Ryzen 7 5800H, 8 cores / 16 threads, 16 MiB L3 |
| RAM | 5 GB (this is the binding constraint, not the CPU) |
| GPU | RTX 3050 Laptop, 4 GB, SM 8.6, FP64 = 1/64 FP32 |
| Backend | Kokkos 5.x, OpenMP |
| Build | `-O3 -DNDEBUG`, `SATURN_NDIM=2`, `SATURN_PRECISION=double` |

This is a **development laptop**, thermally throttled. Treat absolute numbers as
indicative only. Every measurement below is best-of-3, each sample being the
mean of 50 iterations of `lap = laplacian(f)` over 1024² cells (2 arrays
touched per cell: 16.8 MB of traffic per iteration).

## Result 1 — kernel dispatch strategy has opposite scaling

Two ways to run a kernel over a patch decomposition:

- **team** — `TeamPolicy(nPatches, AUTO)`: one Kokkos team per patch, one launch
  total. Advantantage: no per-cell lookup, good data locality per patch.
- **flat** — one kernel over all cells, owning patch found by binary search on a
  cumulative-count table (the AMReX approach). Advantage: no serialisation.

| patches | team (GB/s) | flat (GB/s) | winner |
|---:|---:|---:|---|
| 1 | 1.25 | **10.12** | flat, 8.1× |
| 4 | 5.23 | **7.59** | flat, 1.5× |
| 16 | **10.21** | 7.08 | team, 1.44× |
| 64 | **10.01** | 5.77 | team, 1.73× |
| 256 | **8.74** | 6.07 | team, 1.44× |
| 1024 | **8.94** | 5.16 | team, 1.73× |

**Why the flat kernel collapses at one patch, and why this is not a curiosity:**
with a single patch, `TeamPolicy(1, AUTO)` creates exactly one team. Kokkos cannot
parallelise across teams, so the whole 1M-cell patch runs inside one team and the
other cores idle. This is a *correctness-of-dispatch* failure, not a tuning
issue — an AMR level with a single patch is the normal case at level 0 before
refinement, so it would have silently crippled every coarse-grid solve.

**Why the team kernel degrades with many patches:** per-patch parallel regions
lose work-granularity at the edges, and the patch loop has more synchronisation
points. It is still dominated by memory once patches exceed ~16.

**Decision:** `MultiFab::forEachCellAuto` dispatches on patch count
(`kFlatKernelMaxPatches = 8`). The threshold is backend-dependent and **must be
re-measured on CUDA/HIP** — a GPU with 100k concurrent threads will shift the
crossover decisively toward flat.

## Result 2 — OpenMP thread scaling

1024², 16 patches, team path:

| threads | Mcell/s | GB/s | speedup |
|---:|---:|---:|---:|
| 1 | 81.5 | 1.31 | 1.00× |
| 2 | 162.0 | 2.59 | 1.99× |
| 4 | 323.3 | 5.17 | 3.97× |
| 8 | 613.8 | 9.82 | 7.53× |

Linear to 8 threads (7.53× on 8 physical cores — 94% efficiency). The 16-thread
data point is **not reportable**: repeated runs ranged from 1.09 ms to 3.05 ms
per iteration with no code change, consistent with thermal throttling on a
laptop. This is exactly why the scaling claims in the coursework must come from
the cluster, not from here.

## Result 3 — halo exchange correctness

`saturn_tests` fills a smooth periodic field analytically, exchanges ghosts, and
compares every ghost cell against the analytic extension:

- single patch, periodic wrap onto itself: **~1e-16**
- 4×4 patch tiling, periodic: **~1e-16**
- driver, 64² with 2 ghost layers: **4.16e-16**

Machine precision. Includes edges and corners in 3D by construction (the op
builder walks the full grown box, not just faces).

## What is deliberately not measured yet

- MPI strong/weak scaling (no MPI on the dev box; cluster task).
- GPU performance (Kokkos CUDA build exists at `~/PhD/Kokkos/kokkosNVIDIA`,
  built for `AMPERE86` — a separate tuning exercise).
- Halo exchange cost as a fraction of a timestep (needs the flow solver).

## Reproduction

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=/usr/local
cmake --build build -j
./build/saturnCFD apps/sample.yaml --threads 8
```

Vary `grid.block_x` / `grid.block_y` in the YAML to change the patch count.
