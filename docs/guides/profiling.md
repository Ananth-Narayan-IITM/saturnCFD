# Profiling and sanitizing

**Status:** proposed with ADR `0015`.

Profiling answers “where is the time going?”. Sanitizers answer “is the code
reading or writing memory it should not?”. They are different tools and should
not be confused.

## 1. Choose the question

| question | tool |
|---|---|
| invalid read/write, use-after-free, leak | Valgrind Memcheck, ASan |
| undefined behaviour | UBSan |
| GPU out-of-bounds / race | `compute-sanitizer` |
| CPU hotspot (sampling) | `perf` |
| CPU call counts / cache | Callgrind / Cachegrind |
| heap growth | Massif |
| MPI time by call site | mpiP, Score-P/TAU |
| GPU kernels / timeline | Nsight Systems, Nsight Compute |
| Kokkos kernel timing | Kokkos Tools |
| scaling | benchmark harness, not a profiler |

## 2. Build types

- `Release` for benchmarks (`-O3 -DNDEBUG`).
- `RelWithDebInfo` for profiling (`-O2 -g`).
- `Debug` plus sanitizers for memory correctness.

Planned CMake options:

```text
SATURN_ENABLE_SANITIZERS=ON   # ASan + UBSan
SATURN_BUILD_BENCHMARKS=ON
SATURN_ENABLE_MPI=ON
SATURN_ENABLE_KOKKOS=ON
```

## 3. Valgrind Memcheck

Small serial or one-rank case first:

```bash
cmake -S . -B build-dbg -DCMAKE_BUILD_TYPE=Debug -DSATURN_BUILD_TESTS=ON
cmake --build build-dbg -j
valgrind --tool=memcheck \
  --leak-check=full --show-leak-kinds=definite \
  --track-origins=yes --error-exitcode=1 \
  ./build-dbg/saturn_tests
```

One MPI rank:

```bash
mpirun -np 1 valgrind --log-file=valgrind.%p.log \
  --leak-check=full --track-origins=yes \
  ./build-dbg/saturn_tests
```

Two ranks:

```bash
mpirun --oversubscribe -np 2 valgrind --log-file=valgrind.%p.log \
  --leak-check=full --track-origins=yes \
  ./build-dbg/saturn_tests
```

Caveats:

- Valgrind serialises execution; it is 10–100× slower.
- Open MPI/UCX may report “still reachable” memory. Use
  `--show-leak-kinds=definite` and a suppression file; do not hide real
  definite leaks.
- Do not run Valgrind on CUDA kernels. Use `compute-sanitizer`.
- Use a small mesh and few iterations.

## 4. AddressSanitizer / UBSan

Planned build:

```bash
cmake -S . -B build-asan \
  -DCMAKE_BUILD_TYPE=Debug \
  -DSATURN_ENABLE_SANITIZERS=ON
cmake --build build-asan -j
ctest --test-dir build-asan --output-on-failure
```

ASan catches stack/heap buffer overflows and use-after-free. UBSan catches
signed overflow, misaligned access and invalid casts. They are much faster than
Valgrind and should run in CI on the unit tests.

## 5. CPU profiling

Sampling:

```bash
perf record -g ./build/saturnCFD case.yaml
perf report
```

Callgrind:

```bash
valgrind --tool=callgrind --callgrind-out-file=callgrind.out \
  ./build/saturnCFD case.yaml
kcachegrind callgrind.out
```

For OpenMP/Kokkos threads, Valgrind needs `--fair-sched=yes`; otherwise thread
interleaving distorts the profile. Use one thread for the first callgrind run.

Memory growth:

```bash
valgrind --tool=massif ./build/saturnCFD case.yaml
ms_print massif.out.*
```

## 6. MPI profiling

Start with timing inside the benchmark:

- `MPI_Barrier` before a timed region;
- `Kokkos::fence` before/after;
- measure halo pack, MPI wait, unpack separately.

Later:

- **mpiP** — per-call-site MPI time and bytes, low overhead;
- **Score-P / TAU** — tracing and profiling, more setup;
- **Intel VTune** — MPI and CPU counters on Intel clusters.

Do not use `printf` timing for scaling claims. It changes the code and misses
asynchronous completion.

## 7. GPU profiling

Correctness:

```bash
compute-sanitizer --tool memcheck ./build/saturnCFD case.yaml
compute-sanitizer --tool racecheck ./build/saturnCFD case.yaml
```

Timeline:

```bash
nsys profile -o report ./build/saturnCFD case.yaml
nsys stats report.nsys-rep
```

Kernel metrics:

```bash
ncu --set full ./build/saturnCFD case.yaml
```

For MPI + GPU, run `nsys` around `mpirun -np 1` first. Multi-rank GPU
profilers are a cluster topic.

## 8. Kokkos Tools

Kokkos Tools connectors can report kernel names and times without changing the
solver. Build Kokkos with tools enabled (or use the vendored build option) and
load a connector such as a simple timing connector. This is the least invasive
way to see kernel-level time on CPU and GPU.

## 9. What not to do

- Do not profile a `Debug` build for performance.
- Do not compare timings without fences and barriers.
- Do not use Valgrind output as a scaling measurement.
- Do not run a GPU case under Valgrind.
- Do not hide definite leaks with a broad suppression.
- Do not profile one rank and claim parallel scaling.

## 10. First profiling milestones

1. Valgrind Memcheck clean on `saturn_tests` (serial).
2. ASan/UBSan clean on `saturn_tests`.
3. `perf` hotspot report for the serial FVM stencil.
4. `compute-sanitizer` clean on the one-GPU halo test.
5. `nsys` timeline for the one-GPU stencil.
6. Halo fraction from the MPI benchmark.

These are correctness and understanding milestones, not optimisation claims.
