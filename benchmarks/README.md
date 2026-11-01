# benchmarks

Standalone performance measurements. These scripts are not part of CTest: they
take minutes, need a GPU build for the GPU half, and produce timing tables
rather than pass/fail checks.

## Burgers CPU vs GPU

`benchmarkBurgers.py` generates one 2D scalar Burgers case, runs it through two
`saturnCFD` builds, and reads `wallSeconds` from each JSON summary. CSV,
analytic comparison and VTK output are disabled so the timing measures the
solver and time loop, not file I/O.

From the repository root:

```bash
python3 benchmarks/benchmarkBurgers.py \
    --cpu saturnCPU --gpu saturnGPU \
    --nx 256 --ny 256 --t-end 0.2
```

Use `--help` for the resolution, viscosity and output options. The results are
written under `results/burgers/benchmark` by default.

The GPU executable must be a CUDA build (`cmake --preset gpu` with a
CUDA-enabled Kokkos). See [`../docs/guides/gpu.md`](../docs/guides/gpu.md).
