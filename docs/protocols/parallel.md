# P4 — Parallel protocol: MPI decomposition, ghosts and Kokkos

**Status:** accepted (ADR `0014`)

## Purpose

Define the ownership, ghost and halo contract before any FVM operator is
written. The same contract must work for 1 rank, many CPU ranks and one rank per
GPU.

## Vocabulary

- **Owner rank** — the only rank that writes a global cell.
- **Ghost cell** — a read-only copy of an owner cell on another rank.
- **Halo exchange** — refresh of ghost cells from their owners.
- **Partition** — the Cartesian block owned by one rank.
- **Process grid** — `(px,py,pz)`, the MPI rank layout.
- **Block** — local valid index range `[i0,i1] x [j0,j1] x [k0,k1]`.

## Decomposition contract

```cpp
struct MeshPartition {
  int rank, nranks;
  int px, py, pz;              // process grid
  int i0, i1, j0, j1, k0, k1;  // local valid range
  int nGrow;                   // ghost width
  // neighbour rank for each of 26 directions, or MPI_PROC_NULL
  int neighbour[3][3][3];
};
```

- `MPI_Dims_create(nranks, 3, dims)` chooses `px,py,pz`.
- `MPI_Cart_create` sets periodic dimensions from `Geometry::periodic`.
- Local extents differ by at most one; remainders go to low indices.
- 2D and 1D meshes set the unused process dimensions to 1.

## Ownership invariant

1. Every global cell has exactly one owner.
2. Only the owner writes its valid value.
3. A ghost cell is read-only on the receiver.
4. A ghost is refreshed before any operator reads it.
5. A face on a partition boundary is geometrically shared; the cell is not.
6. Each rank updates only its own cells, so a shared face is never
   double-counted.

## Halo contract

`HaloExchange::build(partition, geometry, fields)` resolves:

- 6 face, 12 edge, 8 corner neighbours;
- periodic images;
- local same-rank copies (periodic wrap onto self);
- physical boundary faces are **not** filled here.

`HaloExchange::execute(fields)`:

1. local copies;
2. pack remote buffers;
3. `MPI_Isend`/`MPI_Irecv`/`Waitall`;
4. unpack;
5. return with ghosts valid.

The exchange is field-specific: `nGrow` is taken from the field, not from a
global constant.

## Field contract

```cpp
template <class T>
class CellField {
 public:
  Kokkos::View<T*> data;   // local ghosted storage
  int ncomp, nGrow;
  int i0, j0, k0;          // local valid origin
  Array4<T> array();
};
```

- Storage includes `nGrow` layers on every side.
- `Array4` takes global `(i,j,k,c)` and subtracts the local origin.
- Kernels iterate owned cells only.
- No kernel writes a ghost cell as a final result.

## Host/device contract

- `Kokkos::fence()` before a host copy or MPI send.
- Pack/unpack are Kokkos kernels.
- Host-staged halo first: device → host mirror → MPI → device.
- GPU-aware MPI is a later implementation of the same interface.
- No MPI call inside a Kokkos kernel.

## Initialization contract

```text
MPI_Init_thread(MPI_THREAD_FUNNELED)
  local_rank = rank in MPI_COMM_TYPE_SHARED
  Kokkos::initialize(device_id = local_rank if available)
  run
  Kokkos::finalize()
MPI_Finalize()
```

- If `local_rank >= device_count`, clamp to 0 and warn.
- One rank per GPU is the performance model.
- Multiple ranks on one GPU are allowed only for correctness tests.

## PETSc contract

- `Mat`/`Vec` use `MPI_COMM_WORLD`.
- Global row numbers follow the global cell numbering.
- Neumann pressure uses `MatNullSpace`.
- Vector momentum uses block size 3 or multiple RHS.
- CPU PETSc first; GPU PETSc is an enabled build, not a data-model change.

## Tests

- extents sum to global, no overlap;
- halo analytic periodic field, 1 and 2 ranks, 1 and 2 ghost layers;
- 26-neighbour coverage;
- periodic and non-periodic;
- 1-rank vs 2-rank equality;
- GPU/CPU parity;
- no double-counted cell update;
- `mpirun -np 1` GPU smoke test.

## References

- ADR `0014`.
- AMReX `FillPatch`/`MultiFab` design (methodology reference).
- Open MPI Cartesian topology documentation.
