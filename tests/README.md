# tests

doctest + CTest. Every numerical feature has a unit or manufactured test, and
every solver gets a regression case.

Test executables: `testSaturn` (units/mesh/config/CLI regression),
`testSaturnMPI` (partition/halo), `testSaturnFV` (FV operators, MMS, time loop,
Burgers and heat), and `testSaturnPETSc` (PETSc Poisson/nullspace).

## Layout

```text
tests/
  TestMain.cpp
  TestFvMain.cpp
  TestFv.cpp
  TestLocalMesh.cpp
  TestSimulation.cpp
  TestSaturnBurger.cpp
  TestSaturnBurger2D.cpp
  TestSaturnBurger3D.cpp
  TestSaturnHeat.cpp
  TestParallel.cpp
  UniformMesh.hpp
  checkBurgers.cmake
  checkBurgersVtk.cmake
  checkBurgersCheckpoint.py
  checkHeat.cmake
  checkHeatCheckpoint.py
  regression/
    mesh/
    config/
    burgers/
    burgers2d/
    burgers3d/
    heat2d/
    heat3d/
```

## Labels

| label | purpose |
|---|---|
| `[solver]` | umbrella: FV operators, Burgers and heat numerics |
| `[saturnCFD]` | umbrella: CLI, config, mesh, PETSc, VTK, checkpoint |
| `[unit]` | helpers and algorithms |
| `[mesh]` | reader, topology, structured detection, VTK |
| `[config]` | YAML, schema, SI-only units, BCs |
| `[fv]` | fields, LocalMesh, operators, boundary ghosts |
| `[mms]` | manufactured-solution order |
| `[regression]` | accepted end-to-end cases |
| `[gpu]` | CUDA-labelled tests |
| `[parity]` | CPU/GPU agreement |
| `[performance]` | timing smoke tests (planned) |

Every test carries exactly one umbrella label, so the `solver` and `saturnCFD`
presets partition the suite.

## Running

```bash
ctest --preset default          # everything, output on failure
ctest --preset cpu              # everything except GPU-labelled tests
ctest --preset solver           # FV operators, Burgers and heat
ctest --preset saturnCFD        # CLI, config, mesh, PETSc, VTK, checkpoint
ctest --preset gpu              # GPU-labelled tests in buildGPU

# equivalent fallback
ctest --test-dir buildCPU --output-on-failure
ctest --test-dir buildCPU -L solver --output-on-failure
ctest --test-dir buildCPU -L saturnCFD --output-on-failure
```

## Rules

- No test writes into the source tree.
- No test depends on the current working directory.
- `fv: distributed LocalMesh` checks rank-0 extraction against direct local
  extraction on every rank.
- MPI test targets set `OMP_NUM_THREADS=1`; otherwise several ranks
  oversubscribe the node and `fv_np4` becomes very slow.
- A failing regression is fixed or rebaselined with a reason, never deleted.
- Expected files are text and reviewable.
- Phase 1 regression policy: `docs/validation/regressionTests.md`.
