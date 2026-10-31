# Writing a user solver

**Status:** accepted model (ADR `0011`). `saturnBurger` is the first
implemented solver; the `saturnCFDSolver` scaffold tool is still to come.

This is the saturnCFD equivalent of copying an OpenFOAM solver directory,
editing its equations and rebuilding. You copy a solver under `src/solvers/`,
add one CMake line, edit the equations, and rebuild. The mesh, finite-volume,
PETSc and I/O layers stay untouched.

## 1. Copy a solver

> `saturnBurger` exists and is the template. The `saturnCFDSolver` tool that
> automates the copy is still to come; the manual copy below works now.

```bash
# future:
saturnCFDSolver saturnBurger myBurger
```

The tool will copy `src/solvers/saturnBurger` to `src/solvers/saturnMyBurger`,
rename the class/registration string and print the CMake line. Until then, do it
by hand:

```bash
cp -r src/solvers/saturnBurger src/solvers/saturnMyBurger
# edit the class/registration names
# add the saturnMyBurger target to src/solvers/CMakeLists.txt
```

Then:

```bash
cmake -S . -B buildCPU
cmake --build buildCPU -j
saturnCFD -listSolvers
saturnCFD -dumpConfig myBurgers -writeTemplate myBurgers.yaml
saturnCFD -checkConfig myBurgers.yaml
saturnCFD myBurgers.yaml
```

## 2. Directory layout

```text
src/solvers/myBurgers/
  CMakeLists.txt
  MyBurgersSolver.hpp
  MyBurgersSolver.cpp     # lifecycle + registration only
  MyBurgersCase.hpp       # typed SI configuration
  MyBurgersOptions.cpp    # registerOptions()
  createFields.cpp        # state, initial conditions, BC setup
  equations.cpp           # flux/operator assembly and advance
  diagnostics.cpp         # solver-specific diagnostics
  README.md               # what you changed and how to run it
  cases/example.yaml      # a starting case
```

### What each file is for

| file | edit when you want to change... |
|---|---|
| `MyBurgersSolver.cpp` | lifecycle, registration, time-step policy |
| `MyBurgersCase.hpp` | the typed options your solver needs |
| `MyBurgersOptions.cpp` | YAML keys, defaults, descriptions, units |
| `createFields.cpp` | fields, initial conditions, boundary conditions |
| `equations.cpp` | the PDE, fluxes, source terms, time integration |
| `diagnostics.cpp` | extra CSV/VTK output, residuals, norms |
| `cases/example.yaml` | a concrete case |

The split is deliberate: a physics change belongs in `equations.cpp` and
`createFields.cpp`, not in the driver.

## 3. Configuration and units

Every option is registered with a type, default, description and dimension:

```cpp
void MyBurgersSolver::registerOptions(ConfigSchema& schema) const {
  schema.addDouble("flow.viscosity", 1.0e-3,
                   "kinematic viscosity [m^2/s]",
                   Dimension::KinematicViscosity);
  schema.addDouble("flow.inletVelocity", 1.0,
                   "inlet velocity [m/s]", Dimension::Velocity);
  schema.addInt("time.maxSteps", 10000, "hard step limit");
}
```

v1.0 is SI only. The YAML `units:` block may say `system: SI` or be omitted;
anything else is a validation error. The solver receives SI `double` values.

`-dumpConfig myBurgers` generates the template from these registrations. You do
not write a separate template file.

## 4. State and boundary conditions

`createFields.cpp` builds the state from the validated case and the mesh:

- allocate cell/face fields with the right components;
- read initial conditions from YAML;
- validate that every mesh patch has a BC;
- fill boundary values and ghost/face data.

The core provides the mesh patch list, the field containers and the BC
application helpers. The solver decides which fields exist and what their
dimensions mean.

## 5. Equations

`equations.cpp` is the physics file. It may:

- assemble the convective/diffusive fluxes;
- add source terms;
- call the PETSc linear solver through the core `LinearSolver` interface;
- advance one time step;
- update the solver state.

It must not:

- parse YAML;
- read files;
- allocate a new linear-algebra backend;
- touch the driver or the mesh reader;
- hard-code a unit conversion.

A typical OpenFOAM-like split is:

```text
createFields.cpp   -> createFields.H
equations.cpp      -> UEqn.H + pEqn.H (or the Burgers/heat equivalent)
diagnostics.cpp    -> continuity/residual diagnostics
```

## 6. Build integration

A user solver is a static library linked into `saturnCFD` at configure time.
There is no runtime plugin in v1.0: this keeps Kokkos/CUDA device code and C++
ABI predictable.

Add exactly one line to `src/solvers/CMakeLists.txt`:

```cmake
addSolver(myBurgers)
```

`addSolver(name)` is the project's CMake helper. It adds
`src/solvers/<name>`, creates the static library and links it into
`saturnCFD`. The solver's own `CMakeLists.txt` contains the source list and the
registration. No other core file changes.

## 7. Solver API version

The solver interface is a public contract:

```cpp
static_assert(SATURN_SOLVER_API_VERSION == 1,
              "this solver was written for API v1");
```

When the core interface changes, the API version is bumped and the migration is
documented. A user solver compiled against an incompatible core fails at
compile time, not at runtime.

## 8. Tests for a user solver

- a unit test for the new flux/source term;
- a manufactured-solution or analytic case;
- a `-checkConfig` test for the new options;
- a `-dumpConfig` snapshot test;
- a short regression case in `cases/`.

If you copy a built-in solver, copy its tests too.

## 9. GPU notes

The user solver is compiled with the same Kokkos backend as the core. That is
why there is no runtime plugin:

- CPU: serial/OpenMP builds work like any other solver;
- CUDA/HIP: the solver's kernels are compiled by the same wrapper;
- do not use host-only code inside a Kokkos kernel;
- keep units out of kernels: the solver already receives SI.

A CPU-only runtime plugin can be added later as a convenience, but it is not the
v1.0 model.

## 10. Do and do not

**Do**

- copy the closest built-in solver;
- change one equation at a time;
- keep the YAML options registered in one file;
- validate against an analytic case before a complex case;
- record observations in `docs/validation/`.

**Do not**

- edit `src/simulation`, `src/mesh`, `src/fv`, `src/linalg` or `src/io` for a
  new PDE;
- add hidden YAML keys;
- introduce a unit conversion in a kernel;
- hard-code mesh dimensions or patch names;
- make the solver depend on another solver.

## 11. Worked example

To change Burgers to a forced Burgers equation:

1. run `saturnCFDSolver burgers forcedBurgers`;
2. add `addSolver(forcedBurgers)` to `src/solvers/CMakeLists.txt`;
3. add an option in `ForcedBurgersOptions.cpp`:
   ```cpp
   schema.addDouble("flow.forcing", 0.0,
                    "constant source [m/s^2]", Dimension::Acceleration);
   ```
4. read it in `ForcedBurgersCase.hpp`;
5. add `+ forcing` to the update in `equations.cpp`;
6. build, `-dumpConfig forcedBurgers`, `-checkConfig`, run a manufactured case;
7. record the observed order in `docs/validation/`.

No core file changes beyond the one CMake line.
