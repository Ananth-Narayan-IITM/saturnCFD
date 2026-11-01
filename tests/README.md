# tests

doctest + CTest. Every numerical feature has a unit or manufactured test, and
every solver gets a regression case.

Test executables: `testSaturn` (units/mesh/config/CLI regression),
`testSaturnMPI` (partition/halo), `testSaturnFV` (FV operators, MMS, time loop, Burgers), and
`testSaturnPETSc` (PETSc Poisson/nullspace).

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
  TestParallel.cpp
  UniformMesh.hpp
  checkBurgers.cmake
  checkBurgersVtk.cmake
  checkBurgersCheckpoint.py
  core/
  config/
  mesh/
  fv/
  linalg/
  simulation/
  solvers/
  regression/
    mesh/
    config/
    burgers/
```

## Labels

| label | purpose |
|---|---|
| `[unit]` | helpers and algorithms |
| `[mesh]` | reader, topology, structured detection, VTK |
| `[config]` | YAML, schema, SI-only units, BCs |
| `[fv]` | fields, LocalMesh, operators, boundary ghosts |
| `[mms]` | manufactured-solution order |
| `[regression]` | accepted end-to-end cases |
| `[parity]` | CPU/GPU agreement |
| `[performance]` | timing smoke tests |

## Running

```bash
ctest --preset default          # all tests, output on failure
ctest --preset regression       # regression tests only
ctest --preset mesh             # mesh tests only
ctest --preset config           # configuration tests only
ctest --preset cli              # CLI tests only
ctest --preset mpi              # 1/2/4-rank MPI tests
ctest --preset fv               # LocalMesh, operators and MMS
ctest --preset burgers          # 1D YAML Burgers case on 1/2/4 ranks
ctest --preset burgers2d        # 2D YAML Burgers regression
ctest --preset burgers3d        # 3D YAML Burgers regression
ctest --preset vtk              # VTK field-export regression
ctest --preset checkpoint       # checkpoint/restart round trip

# equivalent fallback
ctest --test-dir buildCPU --output-on-failure
ctest --test-dir buildCPU -L fv --output-on-failure
```

## Rules

- No test writes into the source tree.
- No test depends on the current working directory.
- MPI test targets set `OMP_NUM_THREADS=1`; otherwise several ranks
  oversubscribe the node and `fv_np4` becomes very slow.
- A failing regression is fixed or rebaselined with a reason, never deleted.
- Expected files are text and reviewable.
- Phase 1 regression policy: `docs/validation/regressionTests.md`.
