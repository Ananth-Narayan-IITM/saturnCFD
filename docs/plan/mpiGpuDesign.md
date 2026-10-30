# MPI and GPU design

**Status:** proposed. This page is the design to approve before the FVM layer
is written.

## 1. Goal

The solver must be **MPI-parallel and Kokkos performance-portable from the
first FVM implementation**, not single-core first. The same code must run:

- CPU: MPI ranks, each using Kokkos Serial or OpenMP;
- GPU: MPI ranks, each using Kokkos CUDA/HIP, with one rank per GPU;
- one GPU for the first benchmark, with `mpirun -np 1` for performance and
  `-np 2` only for correctness/oversubscription tests;
- multiple GPUs later, one rank per GPU, without changing the field or halo
  interfaces.

This is a design for the **cell-centred finite-volume** solver, not the
unstructured-mesh future.

## 2. Current status and the earlier v0 plan

v0 did have an MPI design:

- `DistributionMapping` assigned boxes to ranks;
- `HaloExchange` had `build()` (host combinatorics) and `execute()` (local
  kernel plus packed MPI);
- periodic images, edges and corners were handled;
- halo tests compared ghost cells against an analytic periodic field.

But v0's applications hardcoded rank 0 and one rank, so the remote path was
never exercised by a run. v1 currently has no MPI at all.

The v0 halo idea is still right. Its `BoxArray`/`MultiFab` data model is not the
v1 core, because v1 uses one structured hexahedral mesh and cell/face fields.
We reuse the **concept**, not the code.

## 3. Decomposition

The mesh is logically structured: global `(i,j,k)` for every cell. Decompose
that index space into Cartesian blocks:

```text
global grid: nx x ny x nz
process grid: px x py x pz          (MPI_Dims_create)
local block:  [i0,i1] x [j0,j1] x [k0,k1]
```

- `MPI_Dims_create(nranks, 3, dims)` chooses a balanced process grid.
- `MPI_Cart_create` provides periodic topology and `MPI_Cart_shift` neighbours.
- Remainders are distributed one cell at a time from the low index, so the
  local extents differ by at most one.
- 2D meshes have `nz=1` and `pz=1`. 1D meshes have `py=pz=1`.
- For one GPU, `-np 1` means `px=py=pz=1`; no halo traffic, but the GPU kernels
  and data layout are the production path.

No METIS/ParMETIS partition in v1.0. A structured mesh does not need a graph
partition, and logical blocks keep neighbour discovery deterministic.

## 4. Shared cells, shared faces, ghost cells

**Cells are never shared.** Every global cell has exactly one owner rank. Only
the owner allocates it as a valid cell and only the owner writes its new value.

At a partition boundary:

- the two ranks own the two cells adjacent to the face;
- each rank stores a **ghost copy** of the other rank's boundary cell;
- ghost cells are read-only on the receiving rank;
- the halo exchange refreshes them before an operator runs.

A partition-boundary **face** is geometrically shared, but it is not a shared
cell. Both ranks know the face geometry. Each rank computes the face flux from
the same pair of cell values (one valid, one ghost). Each rank updates only its
own cell, so the flux is not double-counted.

This is the cell-centred finite-volume equivalent of "owner computes": the
face is shared, the cell is not, and the ghost is a duplicate with a single
writer.

If a later phase introduces a persistent face-flux array, face ownership is
separate: the owner rank computes the flux and the neighbour applies the
opposite sign after a face-flux exchange. That is not needed for the first FVM
implementation, where each rank can recompute the flux for its own cells.

## 5. Ghost width

Use **two ghost layers** for cell fields:

- one layer is enough for a compact 5/7-point Laplacian;
- two layers are needed for the MUSCL/TVD stencil, which reaches two cells
  beyond the face it reconstructs;
- pressure needs only one layer, but the field container can support a
  per-field `nGrow`.

The halo exchange contract takes the field's `nGrow`; it is not globally fixed.

## 6. Halo exchange

For a structured block with two ghost layers, exchange all **26 neighbour
directions**:

```text
(dx,dy,dz) in {-1,0,1}^3 \ {(0,0,0)}
```

- face neighbours: 6;
- edge neighbours: 12;
- corner neighbours: 8.

For each direction, the sender packs the intersection of the destination's
ghost box with its own valid box (or a periodic image), then sends a contiguous
buffer. The receiver unpacks into the ghost region.

Order:

1. **local periodic wrap** — same-rank copies, no MPI;
2. **remote exchange** — pack, `MPI_Isend`/`MPI_Irecv`, `Waitall`, unpack;
3. **physical boundaries** — wall/inlet/outlet/symmetry, after the halo.

The local fill must happen before remote packing if a rank has periodic images
of its own block.

## 7. Mesh distribution

The scalable path:

1. rank 0 reads and validates the `.msh`;
2. rank 0 builds the global `StructuredMesh` metadata and geometry;
3. rank 0 scatters each rank's local block: cell/face/point ranges, geometry
   arrays and the local coordinate patch with ghosts;
4. every rank builds its local `CellField`/`FaceField` storage.

For a first implementation, reading the mesh on every rank is acceptable for
small tests, but it is not the production path. The `MeshPartition` object must
hide this choice so the solver does not care.

The local mesh stores:

- local `(i0,i1,j0,j1,k0,k1)`;
- local valid and ghosted point coordinates;
- cell volumes/centroids, face areas/normals/centroids for local faces;
- the rank of each of the 26 neighbours, or `MPI_PROC_NULL`;
- periodic source/destination maps.

## 8. Field and operator semantics

`CellField`:

- local ghosted `Kokkos::View<Real*>`;
- `ncomp`, `nGrow`, local/global index mapping;
- global-index `Array4` access, with the local block origin subtracted.

`FaceField`:

- faces of owned cells, including partition-boundary faces;
- partition-boundary faces are duplicated by the two ranks; this is intentional.

Every FV operator follows the same rule:

1. refresh the fields it reads with the halo exchange;
2. apply physical boundary conditions;
3. loop only over **owned** cells/faces;
4. write only owned cells/faces.

No operator may write a ghost cell as a final result. Ghosts are communication
buffers.

## 9. MPI + Kokkos initialization

```text
MPI_Init_thread(MPI_THREAD_FUNNELED)
  -> determine local rank / visible GPU
  -> Kokkos::initialize(settings)
  -> run
  -> Kokkos::finalize()
MPI_Finalize()
```

- `Kokkos::InitializationSettings::set_device_id(local_rank)` selects a GPU.
- Determine local rank with `MPI_Comm_split_type(..., MPI_COMM_TYPE_SHARED)`.
- If `local_rank >= device_count`, clamp to 0 and warn: multiple ranks are
  sharing one GPU (correctness only, not a performance benchmark).
- `Kokkos::fence()` before any device-to-host copy or MPI send.
- `Kokkos::fence()` after unpack before the next kernel.

## 10. GPU halo exchange

First implementation: **host-staged**.

- pack on device into a contiguous `Kokkos::View<Real*>`;
- `deep_copy` to a host mirror;
- `MPI_Isend`/`MPI_Irecv` on the host mirror;
- `deep_copy` back to device;
- unpack on device.

This is portable and works with any MPI build. GPU-aware MPI is a later
optimization behind the same `HaloExchange` interface. One GPU benchmark can
use host staging; the cost is visible in the halo fraction.

## 11. PETSc

PETSc is the linear-algebra backend and must be MPI-aware:

- `Mat`/`Vec` are MPI objects;
- pressure Poisson uses a scalar system;
- vector momentum uses block size 3 or three right-hand sides;
- Neumann pressure uses `MatNullSpace`;
- CPU: `MPIAIJ` + GAMG/HYPRE;
- GPU: use the PETSc build's CUDA/Kokkos vector/matrix backend where available;
  otherwise host-staged PETSc for the first benchmark and a documented gap.

The `LinearSolver` interface hides the PETSc vector storage from the FVM layer.
The FVM/stencil benchmark does not need PETSc; the pressure solve does.

## 12. One-GPU benchmark plan

Hardware available now: one Quadro P620, 2 GB, compute capability 6.1.
MPI: Open MPI 4.1.6.

| run | purpose |
|---|---|
| `mpirun -np 1` GPU | real GPU performance; no remote halo |
| `mpirun -np 2` GPU on one device | halo correctness / oversubscription, not a scaling claim |
| `mpirun -np 1,2,4,8` CPU | MPI strong scaling |
| CPU 1 rank vs GPU 1 rank | backend comparison |

Benchmark first the **partitioned halo/stencil kernel**, then the FVM operators,
then Burgers. Metrics:

- Mcell/s and GB/s;
- halo fraction of wall time;
- strong scaling efficiency;
- weak scaling efficiency;
- pack/unpack time;
- GPU kernel launch count.

Do not claim GPU MPI scaling from two ranks on one GPU. That measures
oversubscription, not interconnect scaling.

## 13. Tests

- partition: local extents sum to global; no overlap; remainder distribution;
- halo: analytic periodic field, 1/2/4 ranks, 1 and 2 ghost layers;
- 26-neighbour coverage: face, edge, corner;
- periodic vs non-periodic;
- 1-rank vs 2-rank result equality on a small case;
- GPU vs CPU parity within precision;
- no cell written by two ranks;
- FVM divergence theorem and manufactured Poisson in parallel.

The full test matrix and the profiling/sanitizer workflow are in
[`parallelTesting.md`](parallelTesting.md) and
[`../guides/profiling.md`](../guides/profiling.md).

## 14. Open decisions

1. **One rank per GPU** (recommended) vs multiple ranks per GPU for production?
2. **Two ghost layers** for all cell fields, or per-field?
3. **Host-staged halo first** (recommended) vs GPU-aware MPI required from day
   one?
4. **PETSc GPU** from day one, or host PETSc first for the pressure solve?
5. **Rank-to-device mapping** by local rank (recommended) or explicit
   `SATURN_DEVICE_ID`?

My recommendation: one rank per GPU, two ghost layers, host-staged halo first,
host PETSc first, local-rank device mapping. That gives a correct MPI+GPU path
on one GPU now and a clean route to multi-GPU later.
