# Running on the GPU

saturnCFD is backend-agnostic through Kokkos. This page covers building and
running with CUDA on an NVIDIA GPU, the measured performance, and the CUDA
pitfalls that shaped the code.

---

## 1. Build

Two things must match: the Kokkos install must have been built **with CUDA**, and
the compiler must be Kokkos' `nvcc_wrapper` (not `nvcc` directly).

```bash
export NVCC_WRAPPER_DEFAULT_COMPILER=g++-13   # must be a host compiler nvcc supports

cmake -S . -B buildcuda \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER=$HOME/PhD/Kokkos/NVIDIA/bin/nvcc_wrapper \
  -DCMAKE_PREFIX_PATH=$HOME/PhD/Kokkos/kokkosNVIDIA \
  -DSATURN_PRECISION=float

cmake --build buildcuda -j
./buildcuda/saturn_tests        # expect: 80/80 checks passed
```

Check that the backend really is CUDA:

```bash
./buildcuda/saturnCFD examples/cavity/demo.yaml
#   backend   : Cuda
```

### Why float, not double, on a consumer GPU

This matters more than any code change.

| GPU class | FP64 : FP32 throughput |
|---|---|
| RTX 3050 / 3060 / 4060 (consumer) | **1 : 64** |
| A100 / H100 / MI300 (datacenter) | 1 : 2 |

On an RTX 3050 a double-precision build gets roughly 1/64 of the FP32 rate, so
it is *slower than the CPU*. For development and for timing a consumer card,
build with `-DSATURN_PRECISION=float`. Build with `double` on the cluster, where
the hardware does not punish it.

The code is templated on `Real`, so this is a configure-time switch, not a fork.
Tolerances adapt automatically: a relative pressure tolerance below ~100 ulp can
never be reached, and asking for it makes CG run to its iteration cap every
timestep. The solver clamps it.

---

## 2. Measured performance

RTX 3050 Laptop (4 GB, SM 8.6, FP64 1:64), Ryzen 7 5800H (8 cores), 2D,
float precision, pressure CG capped at 200 iterations.

### Cavity solver

| grid | cells | GPU (ms/step) | CPU 8 threads (ms/step) | GPU speedup |
|---:|---:|---:|---:|---:|
| 128² | 16 k | 72 | 34 | **0.47×** (slower) |
| 256² | 65 k | 72 | 106 | 1.48× |
| 512² | 262 k | 148 | 409 | **2.8×** |

### Pure stencil (substrate benchmark, 1024²)

| | Mcell/s | GB/s |
|---|---:|---:|
| GPU | 3 057 | 24.5 |
| CPU (8 threads) | 279 | 2.2 |
| **speedup** | **11×** | **11×** |

### How to read that

The GPU time per step is almost **independent of grid size**: 72, 72, 148 ms for
16 k, 65 k, 262 k cells. A 16× increase in work costs 2× in time. That is the
signature of being **kernel-launch bound**, not compute bound.

The pressure solve is conjugate gradients with ~200 iterations, and each
iteration is ~8 small kernels. That is roughly **1600 kernel launches per
timestep**, each ~45 µs of latency. At 128² the grid is far too small to
amortise them, so the GPU loses to the CPU. A raw stencil with no such structure
runs 11× faster on the same hardware, which is what the GPU is actually capable
of.

**The fix is not more GPU tuning. It is fewer, larger kernels.** Geometric
multigrid cuts the iteration count from ~200 to ~10 per solve, which removes
most of the launch overhead outright. That is why multigrid is the top of the
roadmap.

---

## 3. Verifying the GPU gives the right answer

```bash
./buildcuda/saturnCFD examples/cavity/demo.yaml
```

GPU and CPU (both float) agree to **1e-7** on all 64 centreline points, reach the
same `max|u| = 1.26778` in the same 3 950 steps.

One caveat worth knowing: in float the continuity error floors at ~1e-4
(`max|div u|`), versus ~1e-9 in double. That is arithmetic precision, not a bug.
If you need tight mass conservation, use double — on hardware that does not
penalise it.

---

## 4. CUDA portability lessons

These were all real bugs found while making the code run on the GPU. They are
recorded because each one is a trap that costs an afternoon.

**Extended lambdas cannot live in private member functions.** nvcc rejects a
`__host__ __device__` lambda defined inside a private method. The kernels in
`CavitySolver.cpp` therefore live in an anonymous namespace, taking their data as
arguments. The class interface stays small.

**An extended lambda cannot capture another lambda.** `KOKKOS_LAMBDA { f(...); }`
fails to compile. The field visitors are stored in **functor structs** instead.

**A functor must store the visitor by value.** `forEachCellAuto` forwards its
argument, so `F` can deduce to an lvalue *reference* type. A member declared
`F f;` then becomes a reference member pointing at a local that dies when the
launch helper returns. On CPU the stack memory is still intact and everything
works; on GPU the dangling pointer is shipped to the device and read there,
producing `cudaErrorIllegalAddress` at a host stack address. The functors store
`std::decay_t<F>` by value for exactly this reason.

**Never touch member variables inside a device lambda.** Using `mCells` inside a
`KOKKOS_LAMBDA` implicitly captures `this`, a host pointer, and the device
dereferences it. Hoist to a local first. This one is silent: it "works" until the
kernel actually runs.

**Never read field data on the host.** `Array4` returned by `MultiFab::array()`
holds a *device* pointer in a CUDA build. `VTKWriter` and one unit test both did
this and segfaulted. Field data is copied to a host mirror before any host-side
loop.

**Never index a device View from the host.** `mDeviceCounts(nb)` in host code
raises `Kokkos::View ERROR: attempt to access inaccessible memory space`. Values
needed on the host are cached as plain members.

---

## 5. Reproducing the numbers

```bash
# cavity, fixed step count (no steady-state stopping)
cat > /tmp/bench.yaml <<'YAML'
solver: cavity
domain:
  nx: 512
  ny: 512
flow:
  reynolds: 1000
  scheme: minmod
time:
  endTime: 1.0e9
  maxSteps: 50
  steadyTol: 0.0
  checkEvery: 1000000
output:
  prefix: /tmp/bench
  centreLines: false
  vtkEvery: 0
pressure:
  maxIter: 200
  tolerance: 1.0e-6
YAML

./buildcuda/saturnCFD /tmp/bench.yaml     # GPU
./build/saturnCFD /tmp/bench.yaml --threads 8   # CPU float build
```

Profiling: `compute-sanitizer --tool memcheck ./buildcuda/saturnCFD <case>` was
what located the illegal-access bugs above.
