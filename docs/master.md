# saturnCFD

> Structured finite-volume CFD for scalar transport, Stokes flow and incompressible Navier–Stokes, driven by YAML and validated against analytic solutions.

Note: all paths below are relative to the `saturnCFD` repository root.

This page is the **presentation script / cheat sheet**, not the full manual.
It gives the shortest defensible answer to “why this?” and the figure that
proves it.

---

## 0. One-sentence pitch

saturnCFD is a small, readable, SI-consistent finite-volume code that solves
scalar transport (Burgers, heat), Stokes flow and (next) incompressible
Navier–Stokes on structured hexahedral meshes, with MPI and Kokkos from the
first kernel, PETSc for linear algebra, and analytic/MMS verification at every
stage.

The point is not to reimplement OpenFOAM. The point is a code where one
timestep can be followed from the YAML file to the last kernel, and every
numerical choice is validated.

---

## 1. Build, run, help

### 1.1 Requirements

```bash
python3 tools/checkRequirements.py
python3 tools/checkRequirements.py --install   # PETSc, CUDA only if a GPU exists, Kokkos
cmake --build buildCPU --target doctor
```

Core dependencies: CMake >= 3.24, C++20, MPI, PETSc >= 3.19, Kokkos, yaml-cpp
0.8.x, doctest.

### 1.2 CPU / GPU build and install

```bash
tools/compileSaturnCFD cpu        # CPU only
tools/compileSaturnCFD gpu        # GPU only (CUDA-enabled Kokkos)
tools/compileSaturnCFD both       # both, CPU is the default command
tools/compileSaturnCFD both --default gpu
tools/compileSaturnCFD both --clean
```

The script configures `buildCPU` / `buildGPU` with the CMake presets, builds,
installs into `~/.local/saturnCFD/{cpu,gpu}`, and links:

```text
~/.local/bin/saturnCPU      # CPU backend
~/.local/bin/saturnGPU      # GPU backend
~/.local/bin/saturnCFD      # CPU by default (or --default gpu)
```

<!-- Equivalent manual build:

```bash
cmake --preset cpu
cmake --build --preset cpu -j
ctest --preset cpu
``` -->

GPU notes: the GPU configure needs `KOKKOS_CUDA_ROOT` (default
`~/.local/kokkos-cuda`) with `bin/nvcc_wrapper`; one GPU per rank is the
current model.

### 1.3 Running

```bash
saturnCFD case.yaml                 # run (CPU backend by default)
saturnCPU case.yaml                 # explicit CPU backend
saturnGPU case.yaml                 # explicit GPU backend (one rank per GPU)
saturnCPU -threads 4 case.yaml      # 4 CPU threads in one process
mpirun -np 4 saturnCPU case.yaml    # 4 ranks; threads split automatically (cores/ranks)
mpirun -np 4 saturnCPU -threads 1 case.yaml # 4 ranks x 1 thread (recommended for MPI scaling)
```

### 1.4 Help, validation and templates

```bash
saturnCFD -help                     # overview
saturnCFD -help config              # topic help
saturnCFD -listSolvers              # saturnBurger, saturnHeat, saturnStokes
saturnCFD -checkConfig case.yaml    # validate without solving
saturnCFD -checkMesh mesh.msh       # mesh topology/geometry check
saturnCFD -dumpConfig saturnStokes  # print a YAML template
saturnCFD -dumpConfig common -writeTemplate common.yaml
saturnCFD -dumpConfig common -mesh mesh.msh   # patch-name skeleton
```

**What `-dumpConfig` does.** It asks the solver for its registered
`ConfigSchema` and prints a commented YAML template with every key, default,
type, allowed values and units. `-writeTemplate` saves it; `-mesh` fills the
real patch names. It is generated from the same code that validates the case,
so the template cannot drift from the implementation.

**Why this matters in the viva.** A typo, an unknown key, a missing boundary
condition, a non-SI unit or a solver/field mismatch fails before the mesh is
allocated. The configuration is a checked contract, not a text file.

### 1.5 Tests

```bash
ctest --preset default      # everything in buildCPU
ctest --preset cpu          # everything except GPU-labelled tests
ctest --preset solver       # FV operators, Burgers, heat, Stokes
ctest --preset saturnCFD    # CLI, config, mesh, PETSc, VTK, checkpoint
ctest --preset gpu          # GPU tests in buildGPU
```

Current state: **47/47 CTest cases pass** (~ 106 s), Doxygen PDF 601 pages
with 0 warnings, Beamer 35 pages.

---

## 2. Architecture and why these choices

### 2.1 Modules

| module | role |
|---|---|
| `core` | `Vec3`, `Real`, errors, units, timer |
| `config` | typed `ConfigSchema`, yaml-cpp parser, validation, templates |
| `mesh` | Gmsh reader, `checkMesh`, structured `(i,j,k)` view, patches |
| `fv` | interpolation, gradient, divergence, corrected Laplacian, MUSCL/van Leer, BCs |
| `linalg` | PETSc `Mat`/`Vec`/`KSP`/`PC` wrappers |
| `io` | VTU/PVD, CSV, JSON manifest, checkpoint/restart |
| `simulation` | time loop, `Solver` interface, registry |
| `solvers` | `saturnBurger`, `saturnHeat`, `saturnStokes`, (next) incompressible |

### 2.2 Why YAML

- Declarative and reproducible: one text file contains mesh, time, fields,
  boundary patches, numerics and output.
- Nested boundary data and comments are readable; JSON is noisier and has no
  comments.
- A typed schema is the single source of truth: `-dumpConfig`, `-checkConfig`
  and the solver all use it.
- Unknown keys are errors, not warnings. This is essential for BCs.
- `units.system: SI` is checked; non-SI input is rejected.

*Why not CLI flags?* Too many mesh patches/BCs, no reproducibility. *Why not a
hand-maintained template?* It drifts. *Why not JSON/TOML?* Readability and
comments; yaml-cpp is a mature parser and is isolated behind `ConfigSource`.

### 2.3 Why Kokkos

- One source for serial, OpenMP, and CUDA through `Kokkos::View` and
  `parallel_for`.
- Fields and halo kernels are the performance-critical part; PETSc stays on the
  host.
- No raw CUDA/OpenMP duplication: one code path to validate.

*Why not raw CUDA?* Two code paths and no CPU path. *Why not OpenMP alone?*
No GPU. *Why not `std::par`?* GPU portability and Kokkos' memory spaces.
Kokkos and PETSc are complementary: Kokkos owns the field kernels, PETSc owns
the sparse linear algebra.

### 2.4 Why PETSc

- MPI `Mat`/`Vec`/`KSP`/`PC`, DMDA for structured grids, direct and iterative
  solvers, AMG, null-space handling, mature and documented.
- No home-grown Krylov solver: that is a separate project and would distract
  from the CFD verification.
- Direct (`SuperLU_DIST`) gives an exact reference; fieldsplit gives $O(N)$
  memory.

*Why not Eigen?* Excellent serial, no MPI/AMG story. *Why not Trilinos?*
Heavier; PETSc is sufficient. *Why not hypre alone?* Strong AMG but no full
Krylov/assembly layer; PETSc calls it.

### 2.5 Why structured hex and collocated fields

- The v1 scope is structured hexahedral `.msh` meshes; 1D/2D are degenerate 3D
  meshes with a one-cell-thick rule.
- Simple `(i,j,k)` indexing, compact halos, GPU-friendly.
- Collocated cell-centred $u,v,w,p$ matches the structured mesh, Gmsh import
  and the 2D/3D rule. The checkerboard risk is handled by a pressure Laplacian
  stabilization (Brezzi–Pitkäranta in the Stokes core).
- A staggered MAC grid gives exact discrete continuity but splits the velocity
  components onto different meshes and complicates `.msh`/2D-3D handling. It is
  a later option, not needed for the validated cases.

*Why not unstructured/AMR now?* Scope control; structured validation first.
AMR and two-phase are post-v1.

### 2.6 Why a solver registry and separate modules

Burgers -> heat -> Stokes -> Navier–Stokes is a deliberate learning/validation
ladder: convection/limiters -> scalar transport -> vector elliptic + pressure ->
nonlinear coupling. The `Solver` interface + `solver:` key keeps each module
self-contained; a user solver is a copied directory plus one registration line.

### 2.7 Units

SI only in v1.0. Non-SI is rejected. This prevents unit-conversion bugs in
validation and keeps every coefficient dimensionally checkable.

---

## 3. Burgers: convection, shocks and limiters

### 3.1 Equation and exact solution

$$
u_t+\left(\frac{u^2}{2}\right)_x=\nu u_{xx}
$$

Exact viscous traveling wave:

$$
u(x,t)=\frac{U}{2}\left[
1-\tanh\left(\frac{U(x-x_0-Ut/2)}{4\nu}\right)\right],
\qquad U=1,\ \nu=0.02,\ x_0=0,
$$

on $x\in[-1,1]$, $t=0.5$ s.

2D/3D: the same wave along the diagonal,
$u=\frac{U}{2}[1-\tanh(U(\sum x_i-\sum x_{i,0}-\frac{d}{2}Ut)/(4\nu))]$.
2D: $U=1,\nu=0.02,t=0.5$. 3D example: $U=1,\nu=0.2,t=0.25$ on $32^3$
(the front is under-resolved at that grid; the stiff 3D convergence run is a
cluster follow-up).

### 3.2 Numerical method

- Cell-centred finite volume, conservative flux $F=u^2/2$.
- MUSCL reconstruction with the van Leer limiter.
- SSPRK3, adaptive $\Delta t$, CFL 0.3.
- Dirichlet boundaries with the exact wave; initial state = exact at $t=0$.

### 3.3 Validation

| case | metric | result |
|---|---|---|
| 1D $N=32\to1024$ | $L^2$ | 9.0e-3 -> 7.3e-6, orders 1.98–2.11 |
| 1D ranks 1/2/4 at $N=256$ | $L^2$ | identical (relative difference 0) |
| 2D $32^2\to128^2$ | $L^2$ | 9.9e-3 -> 6.0e-4, orders 1.96/2.10 |
| 3D $8^3\to16^3$ | $L^2$ | 3.36e-3 -> 1.30e-3, order 1.37 (coarse) |
| periodic inviscid shock | bounds | $[-0.9954,0.9954]$, integral $1.5\times10^{-16}$ |

**Why Burgers first:** it isolates nonlinear convection, shock formation,
limiters, diffusion and time integration before pressure coupling.

**Figure:** `docs/validation/figures/burgersValidation.png` (1D convergence),
`burgers2DValidation.png`, `burgers3DValidation.png`.

---

## 4. Heat: scalar transport and diffusion

### 4.1 Equation and exact solution

$$
T_t+\mathbf{a}\cdot\nabla T=\alpha\nabla^2 T
$$

Periodic Fourier mode:

$$
T(\mathbf{x},t)=A\sin\!\left(\mathbf{k}\cdot\mathbf{x}
-(\mathbf{a}\cdot\mathbf{k})t\right)
\exp\!\left(-\alpha|\mathbf{k}|^2t\right).
$$

| case | $\mathbf{a}$ | $\alpha$ | $\mathbf{k}$ | $A$ | end |
|---|---|---|---|---|---|
| 2D | $(1,0.5,0)$ | 0.01 | $2\pi(1,1,0)$ | 1 | 0.2 s |
| 3D | $(1,0.5,0.25)$ | 0.01 | $2\pi(1,1,1)$ | 1 | 0.1 s |

Periodic BCs; initial state = exact at $t=0$.

### 4.2 Numerical method

Cell-centred FV; MUSCL/van Leer convection; corrected face-normal Laplacian;
SSPRK3 with adaptive $\Delta t$, CFL 0.3; stability limit
$\Delta t\le\min(\mathrm{CFL}\,h/|\mathbf{a}|,\ \mathrm{safety}\,h^2/(2\alpha d))$.

### 4.3 Validation

| case | meshes | orders |
|---|---|---|
| 3D | $16^3/24^3/32^3$ | 2.30 / 2.33 |
| 3D example | $48^3$ | $L^2=1.48\times10^{-3}$ |
| 2D | $32^2/64^2/128^2$ | 2.37 / 2.20 |

- Identical norms on 1/2/4 ranks.
- CPU and CUDA builds agree to printed precision on the local Quadro P620
  ($L^2=1.478989\times10^{-3}$ at $48^3$).
- Checkpoint/restart reproduces the uninterrupted run to roundoff.

**Why heat second:** linear scalar transport adds Péclet effects, diffusion,
periodic halos and the time-step stability limit without pressure coupling.

**Figure:** `docs/validation/figures/heat3DValidation.png`,
`heat2DValidation.png`.

---

## 5. Stokes: vector elliptic + pressure constraint

### 5.1 Equations and cases

$$
-\mu\nabla^2\mathbf{u}+\nabla p=\mathbf{f},
\qquad
\nabla\cdot\mathbf{u}=0.
$$

| case | exact solution | constants |
|---|---|---|
| Poiseuille 2D/3D | $u(y)=\frac{G}{2\mu}y(H-y)$ | $\mu=1,\ G=1,\ H=1$ |
| Couette 2D/3D | $u(y)=U y/H$ | $\mu=1,\ U=1,\ H=1$ |
| MMS periodic/wall | see §6 | $\mu=1$, $k=2\pi$ |

Walls at $y=0,1$; periodic in $x$ (and $z$ in 3D).

### 5.2 Discretisation

- Collocated cell-centred $u,v,w,p$; PETSc DMDA; 4 dof per cell.
- Momentum: per-unit-volume diffusion $\mu/h^2$, centred pressure gradient
  $0.5/h$.
- Continuity: centred divergence $0.5/h$ plus a Brezzi–Pitkäranta pressure
  Laplacian, coefficient $\beta/h^2$ with
  $\beta=\mathrm{stabilization}\,h_{\min}^2/\mu$.
- All equations per unit volume: physical pressure, and the Schur complement
  scales as $1/\mu$, not $1/(\mu h^3)$.
- Matrix inserted with `MatSetValuesStencil`; hand-built global ids are wrong
  on $y/z$-split DMDA layouts.
- Wall pressure: Neumann ghost; no-slip velocity ghost.
- Pressure gauge: reference row for wall cases; tiny pressure/velocity gauges
  only for fully periodic domains.

### 5.3 Direct solver

`linearSolver: direct` = PETSc `preonly` + LU, preferring SuperLU_DIST.
It is the validation oracle: one exact factorization, error is the
discretisation error.

Cost: 3D LU fill is $O(N^{4/3})$ memory and $O(N^2)$ work. Measured:

| mesh | wall [s] | peak RSS |
|---|---:|---:|
| 2D $128^2$ | 4.7 | 600 MB |
| 3D $24^3$ | 93.7 | 1493 MB |

This is why direct is not the production default.

### 5.4 Fieldsplit solver

Default: outer FGMRES at `rtol 1e-12`; `PC_COMPOSITE_SCHUR`,
`fact_type full`, `selfp`; velocity GMRES + GAMG (`rtol 1e-10`); pressure
preonly + GAMG. Sub-solver choices are YAML keys:

```yaml
stokes:
  linearSolver: fieldsplit
  velocityKsp: gmres
  velocityPc: gamg
  pressureKsp: preonly
  pressurePc: gamg
```

The table uses the **serial-tuned** ILU/Jacobi configuration; the production
default remains GAMG/GAMG because it is more robust in parallel. The
`velocityPc`/`pressurePc` keys select either without a code change.

| case | solver | its | wall [s] | peak RSS | $L^2$ |
|---|---|---:|---:|---:|---:|
| 2D $128^2$ | direct | 1 | 4.71 | 600 MB | 7.63e-6 |
| 2D $128^2$ | fieldsplit | 3 | 8.36 | 175 MB | 7.63e-6 |
| 3D $24^3$ | direct | 1 | 93.7 | 1493 MB | 2.17e-4 |
| 3D $24^3$ | fieldsplit | 2 | 1.67 | 165 MB | 2.17e-4 |

Fieldsplit and direct solutions differ by ~$3\times10^{-14}$ (2D) and
$5\times10^{-14}$ (3D) RMS.

**Why fieldsplit is the default:** $O(N)$ memory. **Why keep direct:** exact
reference, small meshes, high-memory runs. **Why 2D fieldsplit is slower at
128²:** the serial ILU inner solves and setup dominate at that size; memory is still
3.4x lower. In 3D the crossover is decisive: 56x faster, 9x less memory.

### 5.5 Why not SIMPLE/SIMPLEC for Stokes?

Steady Stokes is a **linear saddle-point** system. A coupled direct or
fieldsplit solve is exact and simpler; SIMPLE/SIMPLEC are segregated
pressure-correction methods aimed at nonlinear Navier–Stokes. They remain a
future option for the incompressible solver, not the Stokes validation core.

### 5.6 Bugs found during tuning (worth mentioning)

- The PC was missing `PCSetOptionsPrefix(pc, "stokes_")`, so the
  `-fieldsplit_*` options were ignored and PETSc silently fell back to
  GMRES/bjacobi.
- The pressure block is a Neumann Laplacian; local ILU/bjacobi factorisations
  can be singular on a rank without the reference row. Global GAMG (or Jacobi)
  fixes it.
- The top-wall one-sided divergence had the wrong sign; invisible for
  Poiseuille/Couette ($v=0$), exposed by the wall MMS.
- Fully periodic domains have constant-pressure **and** constant-velocity
  nullspaces; tiny pressure/velocity gauges (1e-6) remove both, wall cases keep
  the exact reference row.

### 5.7 Validation

| case | mesh | $L^2$ | notes |
|---|---|---|---|
| Poiseuille 2D | $16^2$ | 4.88e-4 | second order |
| Poiseuille 2D | $32^2$ | 1.22e-4 | second order |
| Poiseuille 2D | $64^2$ | 3.05e-5 | example |
| Couette 2D | $64^2$ | ~5e-13 direct | exact line |
| Poiseuille 3D | $24^2\times8$ | 2.17e-4 | extruded |
| Couette 3D | $24^2\times8$ | ~1e-11 fieldsplit | exact line |

Ranks: 2D 1/2/4/8, 3D 1/2/4; fieldsplit-vs-direct test included.

**Figures:**
`examples/saturnStokes/results/{2d,3d}/figures/poiseuille_validation.png`,
`couette_validation.png`, the matching `*_solvers.png` plots, and
`examples/saturnStokes/results/summary.txt`.

---

## 6. MMS: verifying the discretisation, not just the solver

### 6.1 Why MMS if Poiseuille/Couette already pass?

Poiseuille and Couette have $p=0$ and do not exercise the pressure block or
the continuity source. MMS gives a known $u_m,p_m$, derives the exact
sources, and checks the observed order of every term.

### 6.2 Periodic MMS

$$
u=\sin(kx)\cos(ky)\cos(kz),\quad
v=-\cos(kx)\sin(ky)\cos(kz),\quad w=0,
$$
$$
p=\sin(kx)\sin(ky)\sin(kz),\qquad k=2\pi,\ \mu=1.
$$

2D drops the $z$ factors.

### 6.3 Wall MMS

$$
u=\pi\sin(ky)\sin(kx)\sin(kz),
$$
$$
v=-2\pi\sin^2(\pi y)\cos(kx)\sin(kz),\quad w=0,
$$
$$
p=\cos(kx)\cos(\pi y)\cos(kz),\qquad k=2\pi,\ \mu=1.
$$

2D drops the $z$ factors. $u=v=0$ at $y=0,1$; periodic in $x,z$.

### 6.4 Sources

$$
f=-\mu\nabla^2u_m+\nabla p_m,
\qquad
g=\nabla\cdot u_m-\beta\nabla^2p_m.
$$

Both velocities are divergence-free, so $g=-\beta\nabla^2p_m$ with
$\beta=\mathrm{stabilization}\,h_{\min}^2/\mu$. The pressure is shifted so the
reference cell is exactly zero.

A finite-difference unit test differentiates the exact fields and checks both
$f$ and $g$; this catches a wrong sign/factor in the hand-derived sources
before blaming the solver.

### 6.5 Measured orders

| case | meshes | velocity | pressure |
|---|---|---|---|
| periodic 2D | $16^2\to32^2$ | 2.01 | 1.89 |
| periodic 3D | $8^3\to16^3$ | 2.03 | 1.71 |
| wall 2D | $16^2\to32^2$ | 1.96 | 1.64 |
| wall 3D | $16^3\to24^3$ | 1.97 | 1.30 |
| wall 3D | $24^3\to32^3$ | 1.96 | 1.53 |

The wall 2D case is gated. The wall 3D pressure order is **pre-asymptotic**:
velocity is clean second order; pressure rises 1.30 -> 1.53 with refinement.
The 2D pressure order is 1.64 (gated), so the wall closure is consistent; a
gated 3D pressure order needs a finer (costly) sequence. This is a measured
limitation, not a hidden error.

**Figure/demo:** compare the wall MMS error slice from the generated CSV, or
show the order table and the `docs/validation/fieldsplit.md` solver comparison.

---

## 7. What to show the professor

| claim | figure / output | path |
|---|---|---|
| Burgers is second order | 1D/2D convergence | `docs/validation/figures/burgersValidation.png`, `burgers2DValidation.png` |
| Heat is second order and rank/GPU consistent | 2D/3D Fourier mode | `docs/validation/figures/heat2DValidation.png`, `heat3DValidation.png` |
| Stokes matches the parabola/line | field + profile + error | `examples/saturnStokes/results/{2d,3d}/figures/*_validation.png` |
| Fieldsplit matches direct | solution diff, time, memory | `examples/saturnStokes/results/{2d,3d}/figures/*_solvers.png` |
| MMS is second order | order tables | `docs/validation/stokes.md`, this page |
| Mesh/config is checked | `-checkMesh`/`-checkConfig` logs | run the commands live |

Regenerate the example plots:

```bash
cd examples/saturnStokes
python3 validate.py --dim 2d --cli saturnCPU
python3 validate.py --dim 3d --cli saturnCPU
python3 validate.py --dim both --cli saturnCPU
```

---

## 8. Likely “why / why not” questions

**Why not OpenFOAM / FEniCS / SU2?**
We need a small code where the full path (mesh -> FV -> PETSc -> validation) is
under our control and readable. OpenFOAM is a large legacy codebase; FEniCS is
FEM, not the cell-centred FV path we validate; SU2 is compressible-oriented.
We borrow proven ideas (checkMesh, dictionary validation, pressure
stabilisation, MMS) but keep the implementation auditable. The Stokes core
uses a Brezzi–Pitkäranta pressure Laplacian; Rhie–Chow is the planned
incompressible-NS face-flux path.

**Why not a staggered grid?**
Collocated fields are simpler with `.msh`, the 2D/3D rule and the vector form.
The checkerboard mode is explicitly damped by the pressure Laplacian
stabilisation and validated. A staggered/MAC grid is a legitimate future
option but not needed for the current cases.

**Why not SIMPLE/SIMPLEC?**
Steady Stokes is linear. A coupled solve is exact and simpler; SIMPLE is a
segregated nonlinear-NS method. It may return for the incompressible solver.

**Why not write your own Krylov solver?**
PETSc already provides MPI Mat/Vec/KSP/PC, AMG, direct solvers and null-space
handling. Reinventing it would delay the physics and add untested code.

**Why use both Kokkos and PETSc?**
Kokkos owns the field/halo kernels and the CPU/GPU portability; PETSc owns the
sparse linear algebra. No PETSc call is inside a Kokkos kernel.

**Why YAML instead of CLI flags or a compiled input deck?**
Many mesh patches, BCs and solver options; YAML is readable, reproducible and
schema-checked. `-dumpConfig` generates the template from the same schema.

**Why keep direct if fieldsplit is the default?**
Direct is the exact oracle for validation and for small/high-memory runs.
Fieldsplit is the production path because its memory is $O(N)$.

**Why is 2D fieldsplit slower than direct at 128²?**
The serial ILU inner solves and setup dominate this small problem (8.4 s vs 4.7 s), while
memory is 3.4x lower. At 3D 24³ fieldsplit is 56x faster and uses 9x less
memory. The default is chosen for the memory scaling, not the smallest 2D
case.

**Why is the 3D wall MMS pressure order only ~1.5?**
The velocity is 1.97 (second order). The pressure order is pre-asymptotic on
the affordable 3D meshes: 1.30 at 16³->24³, 1.53 at 24³->32³; the 2D wall case
is 1.64 and gated. Refining 3D is expensive with direct LU; fieldsplit makes
the next step feasible.

**Why the 1e-6 gauges for fully periodic domains?**
Fully periodic Stokes has a constant-pressure and a constant-velocity
nullspace. The gauges remove both without perturbing wall cases; the resulting
error is $O(10^{-6})$, far below the discretisation error.

**Why SI only?**
Dimensional consistency: every coefficient and BC has checkable units; non-SI
input is rejected. Unit conversions are a common source of silent validation
errors.

**Why structured hex only, no AMR?**
Scope: structured `.msh` meshes make the FV operators, halos and GPU kernels
auditable. AMR and unstructured meshes are post-v1.

**Why no experimental validation yet?**
The verification ladder is analytic/MMS first: it isolates discretisation and
solver errors. The exit-boundary `.msh` case and external comparisons are the
next validation tier.

**What about GPU?**
The GPU backend builds and runs; the heat solver has a CPU/CUDA agreement
check on the local Quadro P620. GPU benchmarks for Stokes are part of the
performance study, not the correctness claim shown here.

---

## 9. Known limitations (say them before the professor does)

- 3D wall MMS pressure order is measured (1.53 at 24³->32³) but not gated;
  2D wall MMS is gated.
- Solution-adaptive AMR, unstructured meshes, compressible and two-phase flow
  are out of v1.0.
- Fully periodic fieldsplit uses 1e-6 pressure/velocity gauges.
- Direct 3D LU is memory-limited; fieldsplit is the default for a reason.
- The exit-boundary Gmsh `.msh` example and the Navier–Stokes solver are the
  next milestones.
