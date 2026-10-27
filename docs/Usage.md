# Building, running and visualising saturnCFD

Everything on this page is copy-pasteable. If a command fails, the failure is a
bug in this document.

---

## 1. Build

### The one switch

Everything about the build route is decided by a single parameter:

```bash
-DSATURN_BACKEND=<serial|openmp|cuda|hip|sycl>
```

It selects the Kokkos backend, the compiler (CUDA uses the bundled
`nvcc_wrapper`, with a host compiler nvcc actually supports), the GPU
architecture (auto-detected from `nvidia-smi`), and a sensible precision
(`float` on CUDA, `double` otherwise). You do not pass compilers, prefixes or
Kokkos flags by hand.

### Where Kokkos comes from

Kokkos is **vendored** in `external/kokkos` and built as part of this project,
so its build tree lands in `saturnCFD/build-*/` and nothing outside the
repository is required. The first configure/build compiles Kokkos too, which
takes a few minutes (longer with CUDA); afterwards it is incremental.

To reuse a prebuilt Kokkos instead — which is what you want on a cluster — add
`-DSATURN_KOKKOS_ROOT=/path/to/kokkos-install`.

### Prerequisites

- CMake >= 3.24
- A C++20 compiler
- For CUDA: a CUDA toolkit; a host `g++` that nvcc supports (the shim picks the
  newest of `g++-13`, `g++-12`, `g++-11`, `g++` it can find)

### Commands

```bash
# CPU, OpenMP, double precision  (the default backend)
cmake -S . -B build-cpu -DSATURN_BACKEND=openmp -DCMAKE_BUILD_TYPE=Release
cmake --build build-cpu -j

# NVIDIA GPU (CUDA, float, arch detected from the GPU)
cmake -S . -B build-gpu -DSATURN_BACKEND=cuda -DCMAKE_BUILD_TYPE=Release
cmake --build build-gpu -j

# single-threaded serial, useful for debugging
cmake -S . -B build-serial -DSATURN_BACKEND=serial -DCMAKE_BUILD_TYPE=Release

# 3D
cmake -S . -B build-3d -DSATURN_BACKEND=openmp -DSATURN_NDIM=3

# MPI
cmake -S . -B build-mpi -DSATURN_BACKEND=openmp -DSATURN_ENABLE_MPI=ON

# override the defaults if you really need to
cmake -S . -B build-x -DSATURN_BACKEND=cuda \
      -DSATURN_PRECISION=double -DSATURN_CUDA_ARCH=HOPPER90 \
      -DSATURN_KOKKOS_ROOT=/opt/kokkos
```

> **Use a separate build directory per backend.** CMake fixes the compiler at
> configure time, so switching between `openmp` and `cuda` in the same directory
> will not work. The configure output always echoes what it chose:
>
> ```
> -- saturnCFD 0.3.0
> --   backend    : cuda
> --   kokkos     : vendored (built into this build tree)
> --   precision  : float
> --   dimension  : 2D
> ```

### Verify the build

```bash
ctest --test-dir build-cpu --output-on-failure
# or directly:
./build-cpu/saturn_tests
```

Expected: `80/80 checks passed`. The tests cover the box algebra, the patch
decomposition, the field container, periodic halo exchange (against an analytic
field, to machine precision), the YAML parser, and the Poisson solver against
manufactured solutions.

---

## 2. Running a case

One binary, many case files. The case file says which solver to use:

```yaml
solver: cavity      # or: substrate
```

```bash
# quick smoke test (~10 s)
./build-cpu/saturnCFD examples/cavity/demo.yaml

# Re = 100 at 128^2, the Ghia benchmark point
./build-cpu/saturnCFD examples/cavity/re100.yaml

# Re = 1000, the classical validation point (minutes to tens of minutes)
./build-cpu/saturnCFD examples/cavity/re1000.yaml

# Re = 10000 at 256^2 -- see the warning in the file, this is a long run
./build-cpu/saturnCFD examples/cavity/re10000.yaml

# parallel substrate benchmark
./build-cpu/saturnCFD examples/substrate/benchmark.yaml --threads 8
```

Useful flags:

```bash
./build-cpu/saturnCFD --list-solvers            # what solvers exist
./build-cpu/saturnCFD --dump-config cavity      # every option, with defaults
./build-cpu/saturnCFD --dump-config substrate
./build-cpu/saturnCFD --threads 8 <case.yaml>   # override thread count
```

### Why `--dump-config` matters

`--dump-config` builds the template by *calling the same option readers that
parse a real case file*, with an empty config. So the generated file is by
construction a complete and correct list of every key the solver reads. It
cannot go stale. If you add an option to the code, it appears here
automatically.

### Changing a case

Copy the example directory and edit the YAML. Nothing is compiled in:

```bash
cp -r examples/cavity/re100.yaml my_case
sed -i 's/reynolds: 100/reynolds: 500/' my_case/case.yaml
./build-cpu/saturnCFD my_case/case.yaml
```

### Convection schemes

`flow.scheme` selects the face reconstruction. The default, `minmod`, is the
best measured choice at 64^2 (see `docs/Solver.md` §7).

| name | order | bounded | note |
|---|---|---|---|
| `upwind` | 1 | yes | most diffusive; reference only |
| `minmod` | 2 | yes | most diffusive TVD; best measured here |
| `vanleer` | 2 | yes | smooth limiter |
| `superbee` | 2 | yes | least diffusive TVD; can steepen |
| `mc` | 2 | yes | monotonicised central, good general default |
| `quick` | 3 | **no** | can oscillate; comparison only |
| `central` | 2 | **no** | unstable at high Re; comparison only |

---

## 3. Output files

A run writes into the directory named by `output.prefix`:

| file | contents |
|---|---|
| `<prefix>/centrelineU.csv` | `y, u` along the vertical centreline `x = 0.5` |
| `<prefix>/centrelineV.csv` | `x, v` along the horizontal centreline `y = 0.5` |
| `<prefix>/field.pvd` | ParaView collection (only if `output.vtkEvery > 0`) |
| `<prefix>/field_*.vti` | one XML VTK file per patch per output step |

### Comparing against Ghia

```bash
python3 tools/compareCavity.py results/cavity/re100/centrelineU.csv --re 100
python3 tools/compareCavity.py results/cavity/re1000/centrelineU.csv --re 1000
```

The script interpolates the computed profile onto Ghia's tabulated `y`
coordinates and reports per-point differences, RMS and max deviation.

> The Ghia values embedded in the script were transcribed for convenience.
> Check them against the paper before quoting them in a report.

---

## 4. Visualising in ParaView

Set `output.vtkEvery` to a positive number in the case file:

```yaml
output:
  prefix: results/cavity/re1000
  vtkEvery: 1000        # write a field snapshot every 1000 steps
```

Then:

```bash
./build-cpu/saturnCFD examples/cavity/re1000.yaml
paraview results/cavity/re1000/field.pvd
```

**Open the `.pvd` file, not the individual `.vti` files.** The `.pvd` is the
time-series index; ParaView reads it as a single animated dataset with all
timesteps and all patches.

Fields written:

| name | meaning |
|---|---|
| `u` | x-velocity, interpolated from faces to cell centres |
| `v` | y-velocity, interpolated from faces to cell centres |

Notes and current limitations:

- The velocity is **staggered** on faces internally. It is averaged onto cell
  centres for output, so what you see in ParaView is cell-centred data.
- Pressure is **not** written yet. Adding `p` is a one-line change in
  `runCavity`, but it needs a decision about whether to write kinematic
  pressure (`p/rho`) or dynamic pressure. Until then, use the CSV output.
- Output is ASCII `.vti`. That is readable and diffable but large. Binary
  appended output is a drop-in replacement in `src/io/VTKWriter.cpp`.
- Only serial (rank 0) output is implemented. Multi-rank runs would need each
  rank to write its own parts, which the `.pvd` format supports but the code
  does not yet do.

### Quick look without ParaView

```bash
python3 - <<'EOF'
import matplotlib.pyplot as plt
ys, us = [], []
for line in open("results/cavity/re100/centrelineU.csv"):
    if line.startswith("#"): continue
    a, b = line.split(",")[:2]; ys.append(float(a)); us.append(float(b))
plt.plot(us, ys); plt.xlabel("u"); plt.ylabel("y"); plt.show()
EOF
```

---

## 5. Troubleshooting

| symptom | likely cause |
|---|---|
| `max|div u|` large (> 1e-4) | pressure solve not converged, or divergence taken from the wrong velocity field (must be the *predictor*) |
| velocities grow without bound | `flow.cfl` too high for the chosen scheme, or an unbounded scheme (`quick`, `central`) at high Re |
| run never converges | for Re > ~8000 the cavity is genuinely unsteady; compare a time average, not a steady state |
| `unknown convection scheme` | typo in `flow.scheme`; the parser fails loudly on purpose |
| ParaView opens an empty view | you opened a `.vti` instead of the `.pvd` |
