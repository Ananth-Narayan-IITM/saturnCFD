# Regression tests

**Status:** accepted policy. Tests are added as each module lands.

## What a regression test is

A regression test runs a real case through the real binary and compares the
result with a stored expectation. It is not a unit test of a helper and not a
manufactured-solution order test. It answers one question:

> Did a change alter a result that we already accepted?

The expectation is a small text file, not a binary blob, so a diff shows exactly
what changed.

## Why we need them

Unit tests prove that a function does what its author intended. Manufactured
solutions prove the order of accuracy. Neither catches:

- a change to a boundary condition that shifts a centreline by 2 %;
- a YAML default that silently changes a case;
- a mesh reader that drops a patch;
- a generated template that no longer matches the options the solver reads;
- an output writer that renames or reorders a field;
- a linear-solver default that changes the iteration count or the answer;
- a compiler/backend change that changes the last digits.

A regression test is the memory of the project. It is also the evidence that a
fix did not break the validated cases.

## What is stored

For every case:

```text
tests/regression/<area>/
  <case>.yaml          # the exact input
  <case>.expected      # the accepted result
```

The `.expected` file is plain text:

```yaml
case: traveling64
git: 772d986
date: 2026-10-31
tolerance:
  l2: 1.0e-8
  linf: 1.0e-7
result:
  l2: 0.001234
  linf: 0.004567
  steps: 3950
  maxDivergence: 5.3e-11
```

Do not store a full field array unless the case is specifically about field
output. Store the smallest set of scalars that would catch a real regression.

## Rules

1. **Every solver gets a regression case before its first benchmark.**
   Burgers gets `traveling64` before any cavity run.
2. **Every bug fix gets a regression case** that fails before the fix and
   passes after it.
3. **Every output-format change gets a regression case** for the file header
   and field names.
4. **Tolerances are explicit and precision-aware.** Double and float do not
   share a tolerance.
5. **A failing regression is never "fixed" by deleting the case.** Either fix
   the code or record the new accepted value with a reason, date and git hash.
6. **Expected files are reviewed.** Updating them is a deliberate act, not a
   test-run side effect.
7. **Case names are camelCase.** `traveling64.yaml`, not `traveling-64.yaml`.
8. **No dependency on the working directory.** Paths come from CMake/CTest.
9. **No hidden nondeterminism.** If a case is stochastic or threaded, record
   the seed and the tolerance.

## What to compare

| area | compare |
|---|---|
| Burgers | $L^1, L^2, L^\infty$, observed order, shock position, conservation |
| heat | profile at sample points, boundary-layer thickness, $L^2$ |
| Stokes | Poiseuille/Couette sample values, divergence norm, pressure mean |
| incompressible | cavity centreline RMS/max, steps, max divergence, kinetic energy |
| mesh | `checkMesh` pass/fail, counts, volume range, structured flag, patch names |
| config | accepted/rejected case, exact error category, generated template round-trip |
| CLI | exit code, report keys, `.pvd` references |
| output | field names, component count, time/step metadata |

## Running

```bash
ctest --preset regression
```

or a single case:

```bash
saturnCFD tests/regression/burgers/traveling64.yaml
```

The C++ test harness may also call the library directly for speed. The case
file and expected file are still the same.

## Updating an expectation

1. Run the case and inspect the new result.
2. Confirm the change is intended and understood.
3. Update the value, `git`, `date` and a one-line reason in the `.expected`
   file.
4. Commit the code change and the expectation together.
5. Mention the change in `docs/validation/`.

If the change is not understood, it is a bug, not a new baseline.

## Phase 1 regression cases

| case | what it locks | status |
|---|---|---|
| `mesh/box2x2x2` | structured hex pass, counts, volume, patches | implemented |
| `mesh/tetRejected` | unstructured mesh rejected with the right error | implemented |
| `mesh/thin2d` | one-cell-thick mesh reported as 2D | implemented |
| `config/siOnly` | `units.system: SI` and no block accepted | implemented |
| `config/cgsRejected` | `units.system: CGS` rejected before allocation | implemented |
| `config/unknownKey` | unknown YAML keys rejected | implemented |
| `config/templateRoundTrip` | generated template passes validation | implemented |
| `cli/checkMesh` | PASS/FAIL and exit code through the binary | implemented |
| `cli/checkConfig` | SI accepted, CGS rejected through the binary | implemented |
| `cli/defaultOutputs` | `.vtu` + `.json`, no `.vtp` without `-writeVTK` | implemented |
| `cli/writeVTK` | `-writeVTK` adds `.vtp` and `.pvd` | implemented |
| `cli/completion` | generated completion contains `-dumpConfig` | implemented |
| `cli/templateExtension` | `-writeTemplate results/file` creates `results/file.yaml` | implemented |
| `mpi/partition` | local extents sum to global, no overlap | implemented |
| `mpi/haloPeriodic` | analytic periodic ghosts on 1/2/4 ranks | implemented |
| `mpi/halo2d` | one-cell-thick 2D halo | implemented |
| `mpi/haloComponents` | multi-component exchange | implemented |
| `fv/cellField` | fill and owned-cell reduction | implemented |
| `fv/haloPeriodic` | Kokkos halo, two ghosts, 26 directions, 1/2/4 ranks | implemented |
| `fv/mms3d` | 3D periodic Laplacian order and stored `L2` | implemented |
| `fv/mms2d` | 2D periodic Laplacian order | implemented |
| `fv/localMesh` | cell/face geometry, patch tags, face distance | implemented |
| `fv/linearGradient` | linear interpolation/gradient exact with Dirichlet ghosts | implemented |
| `fv/linearDivergence` | linear divergence exact with Dirichlet ghosts | implemented |
| `fv/neumann` | Neumann ghosts reproduce the exact linear gradient | implemented |
| `fv/divergenceIntegral` | periodic divergence integrates to zero | implemented |
| `fv/skewLaplacian` | sheared-mesh non-orthogonal Laplacian exact for a linear field | implemented |
| `fv/upwindConvection` | upwind convection and constant-field divergence | implemented |
| `fv/musclConvection` | MUSCL order and TVD step boundedness | implemented |
| `fv/divergenceTheorem` | discrete divergence theorem on a sheared mesh | implemented |
| `fv/rhieChow` | linear-pressure zero correction and checkerboard suppression | implemented |
| `fv/advectionDiffusion` | steady advection–diffusion MMS order | implemented |
| `fv/gradientMms` | periodic gradient order on 1/2/4 ranks | implemented |
| `fv/divergenceMms` | periodic divergence order on 1/2/4 ranks | implemented |
| `fv/gpu` | device space, 32^3 MMS against the CPU reference, halo roundoff | implemented in code; needs a CUDA host to run |
| `petsc/poisson1d` | 1D Dirichlet Poisson, second-order | implemented |
| `petsc/poisson3d` | 3D Poisson with CG + GAMG | implemented |
| `petsc/neumann` | all-Neumann nullspace and residual | implemented |

The FVM entry is a library regression until the Burgers solver exists: the
stored 32$^3$ `L2=0.13435580757778084` is the smallest scalar that catches a
stencil, halo or geometry regression. It is checked on every rank and in a
CUDA build. Phase 2 adds `burgers/traveling64` and `burgers/shock128`.

## Why not only unit tests

A unit test of `checkMesh` can pass while the CLI mishandles `-report`, or the
YAML reader accepts a key the solver never reads. The regression case exercises
the path the user actually runs. The unit test tells you *where* it broke; the
regression tells you *that* it broke.

## References

- `docs/protocols/testing.md` — test categories and CTest labels.
- `docs/validation/README.md` — validation record format.
- ADR `0012` — doctest.
- ADR `0006` — configuration validation.
