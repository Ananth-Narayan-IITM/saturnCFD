# FVM layer with Kokkos-backed fields

**Status:** first slice implemented with ADR `0016` (Kokkos fields, halo
adapter, uniform Cartesian Laplacian, periodic MMS). The general `LocalMesh`
and the remaining operators are next.

This is the plan for the first Kokkos field/FVM increment. It builds directly
on the MPI partition/halo layer already implemented.

## 0. What is implemented now

- Kokkos 4.7.04 is fetched and pinned by default; `SATURN_BACKEND=openmp` is
  the CPU default. A CUDA build uses an installed Kokkos and its
  `nvcc_wrapper`.
- `CellField` is a ghosted Kokkos view with `ncomp` components.
- `FieldHalo` performs device pack, host mirror, MPI `Isend`/`Irecv`, device
  unpack, and local periodic copies. Buffers are allocated once.
- `CartesianGeometry` and `laplacian` implement the seven-point operator on a
  uniform block.
- Periodic 2D and 3D manufactured Laplacian tests pass on 1, 2 and 4 ranks
  with observed order 2.00 (within 0.03 at the finest level).
- GPU-labelled tests are compiled and run in a CUDA build; the local machine
  has the GPU but no CUDA toolkit, so that build is not yet verified here.

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
cmake --install build-cpu --prefix "$HOME/.local"

cmake --preset gpu        # build-gpu, CUDA
cmake --build --preset gpu -j
cmake --install build-gpu --prefix "$HOME/.local"
```

The installed `saturnCFD` carries the backend it was compiled with. Install the
CPU or GPU build last to choose which one runs. The run command is always:

```bash
mpirun -np 4 saturnCFD case.yaml
mpirun -np 1 saturnCFD case.yaml
```

Cache variables:

```text
SATURN_BACKEND=openmp|serial|cuda|hip   CPU default: openmp
SATURN_ENABLE_KOKKOS=ON                 default
SATURN_USE_SYSTEM_KOKKOS=OFF            CPU default: fetch 4.7.04
SATURN_ENABLE_MPI=ON                    default
SATURN_PRECISION=double                 float is not wired yet
```

The CPU build fetches a pinned Kokkos tag. The GPU build uses an installed,
CUDA-enabled Kokkos because `CMAKE_CXX_COMPILER` must be Kokkos'
`nvcc_wrapper` before the project is configured:

```bash
export KOKKOS_ROOT=/path/to/cuda-kokkos
cmake --preset gpu
```

The installed Kokkos must be built for the target architecture (for example
`PASCAL61` for the Quadro P620).

Doubles are used everywhere for now, including the GPU. Float CUDA is a later
optimisation; `Accum` must remain double for reductions when it lands.

`mpirun` is not CPU-only: it launches MPI ranks. The backend compiled into each
rank decides CPU vs GPU. One rank per GPU is the performance model.

## 3. Field layout

`CellField<Real>`:

```cpp
class CellField {
 public:
  using view_type =
      Kokkos::View<Real****, Kokkos::LayoutLeft, memory_space>;

  KOKKOS_INLINE_FUNCTION
  Real& operator()(int i, int j, int k, int c = 0) const;

 private:
  view_type mData;
  int mNGrow, mNcomp;
  int mOrigin[3], mExtent[3];
};
```

A four-dimensional view with extents `(nxg, nyg, nzg, ncomp)` and
`LayoutLeft` has exactly the flat layout used by the host halo path, so no
transpose is needed between the Kokkos and MPI buffers.

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

The first implemented geometry is `CartesianGeometry`: a uniform block with
spacing `h_d`, cell centres `x_d = origin_d + (i_d + 1/2) h_d`, and the
seven-point coefficient `1/h_d^2`. It is enough to verify the halo, the
operator and the MMS order without mixing in mesh-quality corrections.

The general `LocalMesh`/`DeviceGeometry` is the next slice. It owns:

- `cellVolume`, `cellCentroid` (3 components);
- face area, face normal, face centroid for faces of owned cells;
- local point coordinates including ghost layers;
- patch/BC tags for boundary faces.

The host `Mesh`/`checkMesh` remains the source of geometry construction. The
first implementation may extract the local block from the global mesh on each
rank; the production path is rank 0 read + scatter, then device copies.

Geometry lives in Kokkos views and is copied once before the time loop.

## 5. Halo adapter

The Kokkos path is `FieldHalo`, not a method on `HaloExchange`, so `comm`
stays backend-agnostic:

```cpp
FieldHalo(const HaloExchange& halo, int ncomp);
void execute(CellField& field);
```

- `HaloExchange` still builds the 26-direction op list and owns the host path.
- `FieldHalo` owns per-op device and host buffers, allocated once.
- pack kernel writes a contiguous device `View<Real*>`;
- `deep_copy` to host mirror;
- `MPI_Isend`/`Irecv`;
- `deep_copy` back;
- unpack kernel.
- Local periodic images are copied device-to-device, also on one rank.

`Kokkos::fence()` separates pack/unpack from the host copy.

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

1. **Done (uniform).** `laplacian` — orthogonal seven-point stencil.
2. `gradient` — cell-centred gradient from face values.
3. `divergence` — sum face fluxes / cell volume.
4. `interpolate` — cell to face.
5. Non-orthogonal Laplacian correction on general hexahedra.
6. `convectiveFlux` — upwind/Rusanov, then MUSCL/TVD.

Boundary conditions are applied to ghost cells before an operator reads them.

The first validation set:

- constant field: gradient zero, divergence of constant flux zero;
- linear field: gradient exact on a uniform hex block;
- divergence theorem: sum of face fluxes equals volume integral;
- **done:** manufactured Laplacian: second-order convergence in 2D/3D;
- advection of a smooth pulse: correct speed and no overshoot with a limiter.

## 7a. Manufactured-solution methodology

MMS is the primary verification method for this layer, not an afterthought.
The full derivation and pitfalls are in
[`../theory/manufacturedSolutions.md`](../theory/manufacturedSolutions.md).

The first MMS cases are:

1. **2D/3D periodic Laplacian** — **implemented and passing**.
   $$
   \phi=\sin(2\pi x)\sin(2\pi y)\,[\sin(2\pi z)],
   \quad
   s=-d(2\pi)^2\phi.
   $$
   Run on 1, 2 and 4 ranks. Expected order: 2. Observed on the CPU OpenMP
   build: 3D `1.978, 1.994`; 2D `1.994, 1.999` for the two refinement pairs.
2. **Dirichlet Laplacian** with exact boundary values; verifies boundary
   ghosts. Next.
3. **Neumann Laplacian** with $\phi=\cos(\pi x)\cos(\pi y)[\cos(\pi z)]$;
   verifies zero-normal-gradient ghosts. Next.
4. **Steady advection–diffusion** with a smooth manufactured field; verifies
   the convection sign, interpolation and limiter branch.
5. **One-step transient advection–diffusion**; refines $h$ and $\Delta t$
   separately to separate spatial and temporal order.
6. **CPU/GPU parity** and **1-rank/2-rank parity** on the same MMS case.

For every case record $L^1$, $L^2$, $L^\infty$, observed order, rank count,
backend and precision. A test passes only when the order is in the asymptotic
range and matches the expected order within about 0.2.

Why MMS, not just a benchmark: a physical benchmark cannot tell whether a
second-order scheme is actually second order, and a unit test cannot catch a
halo or face-orientation bug in the assembled operator. MMS tests the assembled
discrete operator against the continuous PDE.

## 8. Tests

Implemented:

- `tests/TestFv.cpp` — CellField fill/reduce, Kokkos halo with two periodic
  ghost layers, 2D/3D MMS order, and a stored CPU reference norm;
- `tests/TestFvMain.cpp` — MPI then Kokkos initialization, global failure
  reduction;
- `saturn_fv_tests` runs under `mpirun -np 1/2/4` and as a serial binary when
  MPI is off;
- the same binary carries `gpu`-labelled CUDA cases (`TestFv.cpp`,
  `SATURN_BACKEND_CUDA`): active CUDA space, 32^3 MMS against the CPU
  reference, and periodic halo roundoff.
- The reference test is the parity gate: the GPU double build must match the
  stored CPU `L2` to `1e-9`; a wrong stencil or halo is orders larger.

CTest labels: `fv`, `mpi`, `gpu`, `parity`, `regression`.

Still to add: 1-rank vs 2-rank *cross-run* equality (the current MMS/order and
reference checks run identically on 1/2/4 ranks), Dirichlet/Neumann MMS,
advection-diffusion MMS, and the halo/stencil benchmark.

## 9. Performance

First benchmark: `benchmarks/haloStencil`.

- pack/unpack time;
- halo fraction;
- Mcell/s and GB/s;
- strong/weak scaling on CPU;
- one-GPU CPU vs GPU comparison.

Do not claim multi-GPU scaling from one GPU.

## 10. Order of work

1. **Done.** Kokkos dependency and `cpu`/`gpu` presets.
2. **Done.** `CellField` + host mirror + fill/reduce tests.
3. **Done (uniform).** `CartesianGeometry` device geometry.
4. **Done.** `FieldHalo` pack/unpack adapter.
5. **Done.** `CellField` halo tests on 1/2/4 ranks.
6. **Partially done.** `laplacian` (orthogonal); gradient/divergence/
   interpolation next.
7. **Partially done.** Periodic 2D/3D MMS; Dirichlet/Neumann and
   advection-diffusion next.
8. **Pending.** GPU execution verification and the halo/stencil benchmark.

## 11. Open decisions

Resolved:

1. Kokkos acquisition: pinned FetchContent for CPU; installed Kokkos for CUDA
   because `nvcc_wrapper` must be the compiler at configure time.
2. First kernel policy: MDRange with an explicit signed `IndexType<int>`.
3. Face field: later; temporary face arrays first.
4. GPU precision: double for the first verified slice; float later.
5. LocalMesh construction: local extraction first, rank-0 scatter hidden
   behind `LocalMesh` for production.

Open:

1. `LocalMesh` face/BC storage layout for non-uniform hexahedra.
2. Cross-run rank-parity comparison (1 vs 2 vs 4) versus the current
   per-run norm check.
3. GPU-aware MPI versus host staging once the CUDA build is available.
