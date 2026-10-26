# Building, running and visualising saturnCFD

Everything on this page is copy-pasteable. If a command fails, the failure is a
bug in this document.

---

## 1. Build

### Prerequisites

- CMake >= 3.24
- A C++20 compiler (Kokkos >= 5 requires C++20)
- Kokkos, built for the backend you want (Serial / OpenMP / CUDA / HIP / SYCL)

If Kokkos is not in a standard location, point CMake at it.

```bash
# CPU (OpenMP), the default development build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_PREFIX_PATH=/usr/local
cmake --build build -j

# 3D instead of 2D
cmake -S . -B build3d -DCMAKE_BUILD_TYPE=Release \
      -DSATURN_NDIM=3 -DCMAKE_PREFIX_PATH=/usr/local
cmake --build build3d -j

# single precision
cmake -S . -B buildf -DCMAKE_BUILD_TYPE=Release \
      -DSATURN_PRECISION=float -DCMAKE_PREFIX_PATH=/usr/local

# with MPI (needed for multi-rank runs)
cmake -S . -B buildmpi -DCMAKE_BUILD_TYPE=Release \
      -DSATURN_ENABLE_MPI=ON -DCMAKE_PREFIX_PATH=/usr/local

# CUDA: Kokkos must have been built with CUDA, and the compiler must be
# nvcc_wrapper, which Kokkos installs. Adjust the arch to your GPU.
cmake -S . -B buildcuda -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_CXX_COMPILER=$(which nvcc_wrapper) \
      -DKokkos_ARCH_AMPERE86=ON \
      -DCMAKE_PREFIX_PATH=$HOME/PhD/Kokkos/kokkosNVIDIA
```

### Verify the build

```bash
ctest --test-dir build --output-on-failure
# or directly:
./build/saturn_tests
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
./build/saturnCFD examples/cavity_demo/case.yaml

# Re = 100 at 128^2, the Ghia benchmark point
./build/saturnCFD examples/cavity_re100/case.yaml

# Re = 1000, the classical validation point (minutes to tens of minutes)
./build/saturnCFD examples/cavity_re1000/case.yaml

# Re = 10000 at 256^2 -- see the warning in the file, this is a long run
./build/saturnCFD examples/cavity_re10000/case.yaml

# parallel substrate benchmark
./build/saturnCFD examples/substrate/case.yaml --threads 8
```

Useful flags:

```bash
./build/saturnCFD --list-solvers            # what solvers exist
./build/saturnCFD --dump-config cavity      # every option, with defaults
./build/saturnCFD --dump-config substrate
./build/saturnCFD --threads 8 <case.yaml>   # override thread count
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
cp -r examples/cavity_re100 my_case
sed -i 's/reynolds: 100/reynolds: 500/' my_case/case.yaml
./build/saturnCFD my_case/case.yaml
```

### Convection schemes

`flow.scheme` selects the face reconstruction. The default, `minmod`, is the
best measured choice at 64^2 (see `docs/SOLVER.md` §7).

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
| `<prefix>/centreline_u.csv` | `y, u` along the vertical centreline `x = 0.5` |
| `<prefix>/centreline_v.csv` | `x, v` along the horizontal centreline `y = 0.5` |
| `<prefix>/field.pvd` | ParaView collection (only if `output.vtk_every > 0`) |
| `<prefix>/field_*.vti` | one XML VTK file per patch per output step |

### Comparing against Ghia

```bash
python3 tools/compare_cavity.py results/cavity_re100/centreline_u.csv --re 100
python3 tools/compare_cavity.py results/cavity_re1000/centreline_u.csv --re 1000
```

The script interpolates the computed profile onto Ghia's tabulated `y`
coordinates and reports per-point differences, RMS and max deviation.

> The Ghia values embedded in the script were transcribed for convenience.
> Check them against the paper before quoting them in a report.

---

## 4. Visualising in ParaView

Set `output.vtk_every` to a positive number in the case file:

```yaml
output:
  prefix: results/cavity_re1000
  vtk_every: 1000        # write a field snapshot every 1000 steps
```

Then:

```bash
./build/saturnCFD examples/cavity_re1000/case.yaml
paraview results/cavity_re1000/field.pvd
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
for line in open("results/cavity_re100/centreline_u.csv"):
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
