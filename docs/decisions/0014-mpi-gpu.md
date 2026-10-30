# 0014 — MPI decomposition and GPU execution model

- **Status:** proposed
- **Date:** 2026-10-31
- **Owner:** saturnCFD v1
- **Supersedes:** none
- **Protocol:** `docs/protocols/parallel.md`
- **Tests:** `tests/parallel/...`, `tests/regression/parallel/...`

## Context

The FVM layer must be designed for MPI and GPUs from the first line of code.
Implementing a single-rank CPU version first and retrofitting parallelism later
would change the field layout, halo contract and linear-system assembly — the
three most expensive parts to redo.

The hardware available now is one NVIDIA Quadro P620 (2 GB, compute capability
6.1), Open MPI 4.1.6, and a multi-core CPU. Multi-GPU benchmarking is deferred
until more than one device is available, but the design must not assume one GPU.

v0 had a halo-exchange design and a box distribution, but its applications
hardcoded one rank. The v0 code is a reference for the **halo concept**, not a
data model to copy.

## Decision

1. **Logical Cartesian decomposition.** Partition global `(i,j,k)` into blocks
   with `MPI_Dims_create` and `MPI_Cart_create`. No METIS in v1.0.
2. **No shared cells.** Each global cell has one owner rank. Partition-boundary
   cells are duplicated as read-only ghost cells.
3. **Two ghost layers** for cell fields, per-field configurable. One layer for
   pressure is allowed.
4. **26-neighbour halo exchange**: faces, edges and corners, periodic images
   handled before physical boundaries.
5. **Cell-centred flux recomputation.** Each rank computes the flux for its own
   cells from valid + ghost values. The shared face is duplicated; the cell
   update is not.
6. **MPI + Kokkos from the start.** `MPI_Init_thread` before
   `Kokkos::initialize`; rank-to-device mapping by local rank; one rank per GPU
   for performance.
7. **Host-staged halo first.** Device pack → host mirror → MPI → device unpack.
   GPU-aware MPI is a later optimisation behind the same interface.
8. **PETSc MPI objects** for the pressure/momentum solves. Host PETSc first;
   GPU PETSc is a separately enabled build.
9. **One-GPU benchmark policy.** `mpirun -np 1` for performance; `-np 2` on one
   GPU only for correctness/oversubscription, never for a scaling claim.

## Alternatives considered

| alternative | why not chosen |
|---|---|
| Single-rank CPU first, MPI later | field layout, halo and linear algebra would be redesigned |
| Unstructured graph partition (METIS/ParMETIS) | the mesh is structured; logical blocks are simpler and deterministic |
| Shared cells via shared memory | races, ownership ambiguity, no multi-node path |
| Face-owned flux arrays | more communication; unnecessary for cell-centred updates |
| One layer only | MUSCL/TVD stencil reaches two cells beyond the face |
| GPU-aware MPI from day one | not portable across all builds; host staging is correct first |
| Multiple ranks per GPU for production | oversubscription; one rank per GPU is the performance model |
| Host-only fields with GPU copies | defeats the Kokkos purpose and adds copies |

## Consequences

- **Code:** `MeshPartition`, `HaloExchange`, ghosted `CellField`/`FaceField`,
  MPI-aware `Simulation`, rank-to-device runtime.
- **Performance:** halo fraction is measurable; host staging is a known cost.
- **Testing:** every FVM operator is tested on 1 and 2 ranks and on CPU/GPU.
- **Build:** `SATURN_ENABLE_MPI=ON` and Kokkos backend are first-class; MPI and
  GPU are not post-v1.0 items.
- **Risk:** one GPU cannot validate MPI+GPU scaling; that benchmark waits for a
  second GPU. The design still supports it.

## Validation / exit criteria

- Partition extents sum to the global grid with no overlap.
- Halo exchange matches an analytic periodic field on 1 and 2 ranks, including
  edges and corners.
- 1-rank and 2-rank FVM results agree on a small case.
- GPU and CPU agree within precision.
- `mpirun -np 1` GPU runs the halo/stencil benchmark.
- No cell is written by two ranks.

## References

- Berger & Oliger (1984); Berger & Colella (1989) for block-structured
  decomposition.
- AMReX `MultiFab`/`FillPatch` design (methodology reference).
- Open MPI `MPI_Cart_create` and `MPI_Comm_split_type` documentation.
