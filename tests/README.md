# tests

doctest + CTest. Every numerical feature has a unit or manufactured test, and
every solver gets a regression case.

## Layout

```text
tests/
  TestMain.cpp
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
| `[mms]` | manufactured-solution order |
| `[regression]` | accepted end-to-end cases |
| `[parity]` | CPU/GPU agreement |
| `[performance]` | timing smoke tests |

## Running

```bash
ctest --test-dir build --output-on-failure
ctest --test-dir build -L regression --output-on-failure
ctest --test-dir build -L mesh --output-on-failure
```

## Rules

- No test writes into the source tree.
- No test depends on the current working directory.
- A failing regression is fixed or rebaselined with a reason, never deleted.
- Expected files are text and reviewable.
- Phase 1 regression policy: `docs/validation/regressionTests.md`.
