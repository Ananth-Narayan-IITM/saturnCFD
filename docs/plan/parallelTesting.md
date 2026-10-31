# Parallel testing plan

**Status:** proposed with ADR `0015`.

This page answers: *how do we know the MPI/GPU FVM code is correct and how do
we measure it?*

## 1. Test pyramid

| level | what it catches | how it runs |
|---|---|---|
| serial unit | field math, box algebra, config | 1 process |
| mesh/config | reader, schema, SI-only | 1 process |
| MPI integration | ownership, halo, edges/corners, periodic | `mpirun -np 1/2/4` |
| GPU parity | device indexing, memory space, fences | 1 rank, CPU vs GPU |
| regression | accepted physics results | 1 and 2 ranks |
| benchmark | scaling, halo fraction, bandwidth | 1/2/4/8 ranks |
| sanitizer | invalid memory, races, leaks | ASan/UBSan, valgrind, compute-sanitizer |

## 2. MPI test binary

The test main:

```text
MPI_Init_thread
Kokkos::initialize
run doctest cases
MPI_Allreduce(local failures) -> global failures
Kokkos::finalize
MPI_Finalize
return global failures ? 1 : 0
```

Every rank must exit non-zero if any rank failed. A test that only passes on
rank 0 is not a parallel test.

## 3. Core MPI tests

### Partition invariants

- local extents sum to the global extent in each direction;
- no overlap;
- remainder distribution is contiguous;
- 2D/1D process grids set unused dimensions to 1.

### Halo analytic test

Fill a periodic analytic field, exchange ghosts, compare every ghost cell
against the exact field:

- 1, 2 and 4 ranks;
- 1 and 2 ghost layers;
- face, edge and corner neighbours;
- periodic and non-periodic boundaries;
- local periodic wrap onto the same rank.

**Pass:** maximum ghost error near machine precision.

### Rank-equality test

Run the same small FVM step on 1 rank and on 2 ranks; compare:

- global sum and max norm;
- selected field values;
- divergence norm;
- no cell updated twice.

**Pass:** agreement within the precision-dependent tolerance.

### Double-count test

A partition-boundary face is duplicated by design. Verify that each cell is
updated once, not twice, by checking a uniform field remains uniform and a
linear field is preserved.

## 4. GPU parity

- Run the same small case with CPU and one GPU.
- Compare field values and norms within the precision floor.
- Check that every `Kokkos::fence()` around host copies/MPI is present.
- Run `compute-sanitizer` on the GPU test binary.

One GPU is enough for parity. Multi-GPU scaling is a cluster test, not a
laptop test.

## 5. CTest integration

Planned test presets/labels:

```bash
ctest --preset mpi            # MPI tests
ctest --preset gpu            # GPU tests
ctest --preset benchmark      # benchmark smoke tests
ctest --preset memcheck       # optional valgrind run
```

MPI tests use a generated test command:

```cmake
add_test(NAME halo_np2
  COMMAND ${MPIEXEC_EXECUTABLE} ${MPIEXEC_NUMPROC_FLAG} 2
          ${MPIEXEC_PREFLAGS} $<TARGET_FILE:testSaturnMPI>
          ${MPIEXEC_POSTFLAGS})
```

Open MPI on a workstation may need `--oversubscribe` when the requested rank
count exceeds available slots. CMake sets that in `MPIEXEC_PREFLAGS` for local
CI, not for the cluster.

## 6. Cluster testing (later)

- Slurm: `srun --ntasks=... --gpus-per-task=1`;
- one rank per GPU;
- modules for CUDA, MPI, Kokkos/PETSc;
- strong/weak scaling across nodes;
- no `--oversubscribe` on the cluster.

Do not claim multi-GPU scaling from a single GPU. The benchmark table records
the hardware and rank layout.

## 7. Benchmark harness

First target: `benchmarks/haloStencil`.

```text
mpirun -np N build-cpu/benchmarks/haloStencil \
  --nx 512 --ny 512 --nz 512 --iters 20
```

Outputs:

```text
ranks, px, py, pz, cells, Mcell/s, GB/s, halo_ms, stencil_ms, halo_fraction
```

Measurements:

- strong scaling: fixed global grid, N = 1,2,4,8;
- weak scaling: fixed local grid, N = 1,2,4,8;
- CPU vs one GPU;
- halo-only vs stencil-only time;
- pack/unpack time.

Use `MPI_Barrier` and `Kokkos::fence` around timed regions. Report best-of-N
and the median, not one sample.

## 8. What to compare

| run | compare |
|---|---|
| 1 rank CPU vs 2 rank CPU | field equality, checksum |
| CPU vs GPU | field equality within precision |
| 1 rank GPU vs 2 rank GPU on one device | correctness only, not speed |
| strong scaling | time per step vs ranks, efficiency |
| weak scaling | time per step at fixed local size |
| halo benchmark | halo fraction, bandwidth |

## 9. Exit criteria for the first parallel increment

- partition invariants pass on 1/2/4 ranks;
- analytic halo passes on 1/2/4 ranks and 1/2 ghost layers;
- 1-rank vs 2-rank FVM equality passes;
- CPU/GPU parity passes;
- `mpirun -np 1` GPU halo/stencil benchmark records time and bandwidth;
- no rank-only passes: failures are globally reduced.
