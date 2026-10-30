# 0015 — Parallel testing, benchmarking and profiling

- **Status:** proposed
- **Date:** 2026-10-31
- **Owner:** saturnCFD v1
- **Supersedes:** none
- **Protocol:** `docs/protocols/testing.md`, `docs/plan/parallelTesting.md`
- **Tests:** `tests/parallel/...`, `benchmarks/...`

## Context

MPI and GPU code can be correct on one rank and wrong on two. A halo that
passes on a face can miss an edge or corner. A GPU kernel can be fast and read
out of bounds. The project needs a test and profiling strategy before the
first parallel FVM kernel, not after.

The available hardware is one GPU and a multi-core CPU; a cluster is available
later. The testing plan must distinguish correctness on one GPU from scaling
on many GPUs.

## Decision

1. **MPI correctness tests run under `mpirun`** with 1, 2 and 4 ranks on small
   cases. The test binary checks global sums/norms and fails on any rank.
2. **CTest labels:** `[unit]`, `[mesh]`, `[config]`, `[mpi]`, `[gpu]`,
   `[regression]`, `[benchmark]`, `[memcheck]`.
3. **Analytic halo test:** periodic field, 1 and 2 ghost layers, all 26
   directions, 1/2/4 ranks; compare ghosts to the exact field.
4. **Rank-equality test:** 1-rank and 2-rank results agree to a stated
   tolerance; no cell is updated twice.
5. **GPU parity:** the same test on CPU and one GPU, within the precision floor.
6. **Benchmark harness:** `benchmarks/haloStencil` and later
   `benchmarks/diffusion`, with strong/weak scaling, halo fraction and
   Mcell/s/GB/s output.
7. **Sanitizers:** ASan/UBSan CPU build option; `compute-sanitizer` for GPU.
8. **Valgrind** for memory correctness on small serial/single-rank cases;
   `perf`/callgrind for CPU hotspots; `nsys`/`ncu` for GPU kernels; MPI
   profiling (`mpiP` or Score-P/TAU) later.
9. **Valgrind is not a scaling tool.** Scaling claims come from benchmark
   timings, not memcheck.

## Alternatives considered

| alternative | why not chosen |
|---|---|
| Test only on 1 rank | misses halo edges/corners and ownership bugs |
| Test only on GPU | hides CPU/MPI correctness |
| Valgrind only | slow, not a parallel scaling tool, noisy with MPI/UCX |
| `printf` timing | fences and barriers missing; meaningless on GPU |
| Benchmark before correctness | cannot distinguish a slow bug from a fast wrong answer |

## Consequences

- **CI:** CPU MPI job with 2/4 ranks; GPU self-hosted later.
- **Runtime:** MPI tests are small; valgrind runs are manual/nightly.
- **Code:** test mains initialize MPI/Kokkos and aggregate failures.
- **Docs:** every benchmark result records hardware, backend, rank count and
  mesh size.

## Validation / exit criteria

- `mpirun -np 1/2/4` halo tests pass.
- 1-rank and 2-rank FVM results agree.
- CPU/GPU parity passes.
- Strong/weak scaling data are recorded for the halo/stencil benchmark.
- ASan/UBSan and `compute-sanitizer` are clean on the test suite.
- Valgrind memcheck is clean on a small serial case (with documented MPI
  suppressions).

## References

- `docs/plan/parallelTesting.md`
- `docs/guides/profiling.md`
- ADR `0014`
