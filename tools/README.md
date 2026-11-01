# tools

Host and build tools that support compiling and installing saturnCFD. This
folder is intentionally small: anything that does not build, check or install
the code lives with the thing it belongs to.

| tool | purpose |
|---|---|
| `checkRequirements.py` | pre-flight host check; `--install` installs PETSc, CUDA and Kokkos |
| `compileSaturnCFD` | builds CPU/GPU and installs to `~/.local/saturnCFD/{cpu,gpu}` |
| `saturnCFDSolver.cpp` | build target `saturnCFDSolver`; copies a solver and prints `addSolver(name)` |

Related scripts outside this folder:

| script | location | purpose |
|---|---|---|
| `validateBurgers.py` | `docs/validation/scripts/` | convergence table and validation figure |
| `benchmarkBurgers.py` | `benchmarks/` | CPU vs GPU wall-clock comparison |
| `doxygenMathFilter.py` | `docs/doxygen/` | Markdown math to Doxygen math |

## Rules

- Prefer C++ or Python over shell scripts so the tool works on every machine.
- A tool that modifies the source tree prints exactly what it changed.
- No tool writes outside the repository unless the user asks.
- Build/install tools are documented in `docs/guides/` or `docs/plan/`.

## Tests

- The host check is exercised by `cmake --build buildCPU --target doctor`.
- Scaffold copy/rename and the Markdown filter do not have dedicated CTest
  cases yet; their outputs are reviewed directly.
