# FVM layer with Kokkos-backed fields

**Status:** proposed with ADR `0016`.

This is the plan for the first Kokkos field/FVM increment. It builds directly
on the MPI partition/halo layer already implemented.

## 1. Goal

Replace the host `std::vector` halo test field with a real ghosted
`CellField`/`FaceField` backed by Kokkos, and write the first FVM operators
against it.

The same source must build and run as:

- CPU MPI: OpenMP/Serial Kokkos, `mpirun -np N`;
- GPU MPI: CUDA Kokkos, one rank per GPU;
- one GPU now: `mpirun -np 1` for performance, `-np 2` only for correctness.

## 2. Build model: CPU and GPU are separate builds

CMake fixes the compiler and backend at configure time. CPU and GPU must be
different build directories:

```bash
cmake --preset cpu        # build-cpu, OpenMP
cmake --build --preset cpu -j

cmake --preset gpu        # build-gpu, CUDA
cmake --build --preset gpu -j
```

Planned cache variables:

```text
SATURN_BACKEND=openmp|cuda
SATURN_ENABLE_KOKKOS=ON
SATURN_ENABLE_MPI=ON
SATURN_PRECISION=auto|float|double
```

`auto` means float on CUDA, double elsewhere. `Accum` remains double for
reductions.

Kokkos is pinned and built by CMake (vendored or FetchContent). The CUDA build
must use the Kokkos `nvcc_wrapper`.

`mpirun` is not CPU-only: it launches MPI ranks. The backend compiled into each
rank decides CPU vs GPU. One rank per GPU is the performance model.

## 3. Field layout

`CellField<Real>`:

```cpp
template <class T>
class CellField {
 public:
  Kokkos::View<T*> data;   // local ghosted storage
  int ncomp, nGrow;
  int i0, j0, k0;          // local valid origin (global index)
  int nx, ny, nz;          // local valid extents

  KOKKOS_INLINE_FUNCTION
  T& operator()(int i, int j, int k, int c = 0) const;
};
```

Storage is local and ghosted:

```text
nxg = nx + 2*nGrow
nyg = ny + 2*nGrow
nzg = nz + 2*nGrow
sc  = nxg*nyg*nzg
index = c*sc + kk*sz + jj*sy + ii
```

Global indices are passed to `operator()`; the field subtracts
`(i0 - nGrow, j0 - nGrow, k0 - nGrow)`.

`VectorCellField` is a three-component `CellField` or a small wrapper around
one. It is not a separate memory layout.

`FaceField` is a later slice. The first operators compute face fluxes into
temporary face arrays; once pressure correction needs a persistent face flux,
it gets a first-class `FaceField`.

## 4. Local geometry on device

A `LocalMesh`/`DeviceGeometry` owns:

- `cellVolume`, `cellCentroid` (3 components);
- face area, face normal, face centroid for faces of owned cells;
- local point coordinates including ghost layers;
- patch/BC tags for boundary faces.

The host `Mesh`/`checkMesh` remains the source of geometry construction. The
first implementation may extract the local block from the global mesh on each
rank; the production path is rank 0 read + scatter, then device copies.

Geometry lives in Kokkos views and is copied once before the time loop.

## 5. Halo adapter

Extend `HaloExchange` with a Kokkos path:

```cpp
void execute(CellField<Real>& field);
```

- pack kernel writes a contiguous device `View<Real*>`;
- `deep_copy` to host mirror;
- `MPI_Isend`/`Irecv`;
- `deep_copy` back;
- unpack kernel.

`Kokkos::fence()` before host copy/MPI and after unpack.

The existing host `execute(Real*)` remains for tests and CPU fallback. The
pack/unpack operation and the 26-direction op list are shared.

## 6. Kernels

Use `Kokkos::MDRangePolicy` or a flat range over owned cells. Start with
MDRange for clarity; benchmark flat range later.

Rules:

- loop over owned cells/faces only;
- read ghost values, write owned values;
- never capture `this` in a lambda; pass fields and geometry by value;
- no allocation inside a kernel;
- no MPI call in a kernel.

## 7. First FVM operators

1. `gradient` — cell-centred gradient from face values.
2. `divergence` — sum face fluxes / cell volume.
3. `interpolate` — cell to face.
4. `laplacian` — orthogonal part first; non-orthogonal correction next.
5. `convectiveFlux` — upwind/Rusanov, then MUSCL/TVD.

Boundary conditions are applied to ghost cells before an operator reads them.

The first validation set:

- constant field: gradient zero, divergence of constant flux zero;
- linear field: gradient exact on a uniform hex block;
- divergence theorem: sum of face fluxes equals volume integral;
- manufactured Poisson: second-order convergence;
- advection of a smooth pulse: correct speed and no overshoot with a limiter.

## 8. Tests

- `tests/fv/` unit tests on CPU;
- `tests/TestParallel.cpp` extended with a `CellField` halo test;
- `saturn_fvm_tests` with MPI main for 1/2/4 ranks;
- CPU/GPU parity test, one GPU;
- 1-rank vs 2-rank equality;
- no double-count on partition faces.

CTest labels: `fv`, `mpi`, `gpu`, `parity`.

## 9. Performance

First benchmark: `benchmarks/haloStencil`.

- pack/unpack time;
- halo fraction;
- Mcell/s and GB/s;
- strong/weak scaling on CPU;
- one-GPU CPU vs GPU comparison.

Do not claim multi-GPU scaling from one GPU.

## 10. Order of work

1. Kokkos dependency and `cpu`/`gpu` presets.
2. `CellField` + host/device mirror + fill/reduce tests.
3. `LocalMesh` device geometry.
4. `HaloExchange::execute(CellField&)` with pack/unpack kernels.
5. `CellField` halo tests on 1/2/4 ranks.
6. gradient/divergence/interpolation/Laplacian.
7. manufactured Poisson and advection tests.
8. CPU/GPU parity and halo/stencil benchmark.

## 11. Open decisions

1. Kokkos acquisition: pinned FetchContent vs vendored source.
2. First kernel policy: MDRange (recommended) vs flat range.
3. Face field now or after pressure correction (recommended later).
4. GPU precision: float (recommended for the P620) vs double.
5. `LocalMesh` construction: local extraction first vs rank-0 scatter
   (recommended production path).

My recommendation: pinned Kokkos, MDRange first, temporary face fluxes,
float on the P620, rank-0 scatter hidden behind `LocalMesh`.
