# Lid-driven cavity, Re = 100 — validation and benchmark

The reference case for saturnCFD: the smallest problem that still exercises
everything (convection, diffusion, pressure projection, boundary conditions) and
has a published benchmark to compare against.

```bash
./build-cpu/saturnCFD examples/cavity/re100.yaml     # CPU
./build-gpu/saturnCFD examples/cavity/re100.yaml     # GPU
```

---

## 1. Case definition

A unit square, all walls no-slip, the top lid sliding at `U = 1`. The only
physical parameter is the Reynolds number, `Re = U L / nu`.

| | |
|---|---|
| Domain | unit square, `L = 1` |
| Grid | 128 × 128 uniform (Ghia used 129 × 129) |
| `Re` | 100 (`nu = 1/100`) |
| Lid velocity | 1.0 |
| Convection scheme | `minmod` (TVD-limited, 2nd order where smooth) |
| Time stepping | explicit Euler, CFL 0.4 |
| Diffusion safety | 0.2 (see §5 — 0.25 is silently wrong) |
| Pressure solve | CG, 200 iterations max, relative tolerance 1e-6 |
| Convergence | stop when `max\|du\| < 1e-6` between checks |

The lid corners are singular (the boundary velocity is discontinuous there).
Every published cavity solution deals with that; it affects the local field, not
the centreline profiles below.

---

## 2. Validation against Ghia, Ghia & Shin (1982)

`u` along the vertical centreline `x = 0.5`:

| y | Ghia | saturnCFD | difference |
|---:|---:|---:|---:|
| 0.9766 | 0.84123 | 0.84355 | +0.0023 |
| 0.9688 | 0.78871 | 0.79172 | +0.0030 |
| 0.9609 | 0.73722 | 0.74022 | +0.0030 |
| 0.9531 | 0.68717 | 0.69076 | +0.0036 |
| 0.8516 | 0.23151 | 0.23645 | +0.0049 |
| 0.7344 | 0.00332 | 0.00405 | +0.0007 |
| 0.6172 | −0.13641 | −0.13871 | −0.0023 |
| 0.5000 | −0.20581 | −0.20881 | −0.0030 |
| 0.4531 | −0.21090 | −0.21361 | −0.0027 |
| 0.2813 | −0.15662 | −0.15746 | −0.0008 |
| 0.1719 | −0.10150 | −0.10163 | −0.0001 |
| 0.1016 | −0.06434 | −0.06437 | −0.0000 |
| 0.0703 | −0.04775 | −0.04658 | +0.0012 |
| 0.0625 | −0.04192 | −0.04194 | −0.0000 |
| 0.0547 | −0.03717 | −0.03720 | −0.0000 |

**RMS deviation 0.0024, maximum 0.0049** — 0.24 % and 0.49 % of the lid speed.
The profile shape is right, the minimum is at the right height (`y ≈ 0.45`), and
the zero crossing is where Ghia puts it (`y ≈ 0.73`).

Sanity checks from the same solution: `max|u| = 0.976` (below the lid speed, as
it must be) and `max|div u| = 5.3e-11`.

Reproduce with:

```bash
python3 tools/compareCavity.py results/cavity/re100/centrelineU.csv --re 100
```

> The reference values in `tools/compareCavity.py` were transcribed for
> convenience; check them against the paper before quoting them in a report.

---

## 3. Performance

Same case, run to convergence, so the step count is set by the physics and not
by a timer.

| | steps | wall time | ms/step | cell-updates/s |
|---|---:|---:|---:|---:|
| CPU, OpenMP 8 threads, double | 14 900 | 483.8 s | 32.5 | 0.50 M/s |
| GPU, CUDA, float | — | not measured (see below) | — | — |

The CPU figure is consistent with an independent fixed-step measurement
(500 steps at 30.4 ms/step).

**GPU: measurement blocked by a driver fault.** The run aborted after 761 s with
`cudaErrorUnknown`, and the device stayed unusable afterwards —
`cudaGetDeviceCount()` fails while `nvidia-smi` reports the card healthy, idle
and at 2 MiB used. That is a driver-level fault (it typically needs
`rmmod`/`modprobe` of the NVIDIA modules, or a reboot), not a code problem.
Re-run the command below after rebooting to fill in this row; nothing in the
case file needs to change.

**Expected result, so you know what to look for.** At 128² (16 k cells) this
problem is far too small for the GPU. Each timestep is ~200 CG iterations ×
~8 small kernels — roughly 1 600 kernel launches — so the GPU is
**launch-latency bound**, not compute bound, and is unlikely to beat 8 CPU
threads here. The same code achieves an 11× GPU speedup on a bare stencil at
1024² (`docs/Gpu.md`). This is a property of the algorithm, not the hardware;
the fix is fewer, larger kernels (multigrid), not GPU tuning.

The builds also differ in precision — `double` on CPU, `float` on GPU, the
configured default because consumer GPUs run FP64 at 1/64 rate. Step and
iteration counts are identical, but it is not an apples-to-apples arithmetic
comparison.

---

## 4. Reproducing the numbers

```bash
# build
cmake -S . -B build-cpu -DSATURN_BACKEND=openmp -DCMAKE_BUILD_TYPE=Release
cmake --build build-cpu -j
cmake -S . -B build-gpu -DSATURN_BACKEND=cuda   -DCMAKE_BUILD_TYPE=Release
cmake --build build-gpu -j

# run
time ./build-cpu/saturnCFD examples/cavity/re100.yaml --threads 8
time ./build-gpu/saturnCFD examples/cavity/re100.yaml

# compare
python3 tools/compareCavity.py results/cavity/re100/centrelineU.csv --re 100
```

Outputs land in `results/cavity/re100/`:

| file | contents |
|---|---|
| `centrelineU.csv` | `y, u` along `x = 0.5` — what you compare with Ghia |
| `centrelineV.csv` | `x, v` along `y = 0.5` |
| `field.pvd` | ParaView collection (written every 200 steps) |

---

## 5. Bug found by this case: the diffusion safety factor was on the limit

Worth recording, because it produced a result that *looked* converged and
matched Ghia to 0.4 % while the underlying velocity field was wrong.

`diffusionSafety: 0.25` puts the timestep exactly on the explicit diffusion
limit `dt = h²/(4ν)`. At that point the highest wavenumber has amplification
factor

```
1 - dt·(8ν/h²) = -1
```

— unit magnitude, so the **checkerboard mode neither grows nor decays**. The
singular lid corner excites it once and it stays forever. On a staggered grid
that mode is discretely divergence-free (`u` and `v` in antiphase), so the
pressure projection cannot remove it, and averaging two faces to cell centres
hides it from ParaView. The only visible symptom was `max|u| = 2.34` in a
cavity whose lid moves at 1.0.

| `diffusionSafety` | `max\|u\|` | `max\|div u\|` | RMS vs Ghia |
|---|---:|---:|---:|
| 0.25 | 2.34 | 6e-5 | 0.0039 |
| 0.20 | **0.98** | 5e-11 | **0.0024** |

Using 0.2 gives amplification −0.6 and the mode dies within a few steps.
Fixed default in `CavityParams`, and all example case files.

The general lesson: for an explicit scheme, a "safety factor" of exactly the
limit value is not conservative — it is the boundary between decaying and
undamped.
