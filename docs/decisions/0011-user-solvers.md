# 0011 — User-editable solvers, OpenFOAM-style

- **Status:** accepted
- **Date:** 2026-10-31
- **Owner:** saturnCFD v1
- **Supersedes:** none
- **Protocol:** `docs/protocols/solver.md`, `docs/guides/writingASolver.md`
- **Tests:** `tests/solvers/...`, `tests/build/...`

## Context

The solver must be editable by the user the way OpenFOAM solvers are: copy a
solver directory, change the equations or the state, rebuild, and run it with a
new case file. The core mesh, finite-volume, PETSc and I/O code must not be
edited for a new equation set. The user should not have to fork the whole
project.

At the same time, v1.0 is CPU-first with Kokkos and CUDA planned. Runtime
plugins are awkward for device code and C++ ABI stability. The extension model
must work on CPU now and remain usable when the GPU backend arrives.

## Decision

1. **A solver is a self-contained directory**, not a branch inside a shared
   monolithic file.
2. **Built-in solvers** live in `src/solvers/<name>/`. **User solvers** live in
   `user/solvers/<name>/`. Both have the same structure and the same API.
3. **User solvers are compiled into the binary at configure/build time.** They
   are static libraries linked into `saturnCFD`. There is no runtime `.so`
   plugin in v1.0. This keeps Kokkos/CUDA device code and ABI predictable.
4. **Registration is one line** in the solver source:
   ```cpp
   SATURN_REGISTER_SOLVER("myBurgers",
                          [] { return std::make_unique<MyBurgersSolver>(); })
   ```
5. **The solver registers its own YAML options.** `-dumpConfig myBurgers`
   generates the template from the user's code.
6. **The solver API has a version constant.** A user solver compiled against an
   incompatible core fails at compile time with a clear message.
7. **`user/solvers/` is discovered at configure time.** A `user/solvers/CMakeLists.txt`
   or `SATURN_USER_SOLVERS` list adds the directories. No core edit is needed.
8. **A scaffold tool** (`tools/newSolver.sh <builtin> <newName>`) copies a
   built-in solver into `user/solvers/`, renames the class and registration, and
   leaves the equations for the user to edit.
9. **OpenFOAM analogy:** copy `src/solvers/burgers` to `user/solvers/myBurgers`,
   edit `createFields`, `equations` and `options`, reconfigure/build, then
   `saturnCFD -listSolvers` shows `myBurgers`.

## Solver directory layout

```text
src/solvers/burgers/
  CMakeLists.txt
  BurgersSolver.hpp
  BurgersSolver.cpp        # lifecycle + registration only
  BurgersCase.hpp          # typed SI options
  BurgersOptions.cpp       # registerOptions()
  createFields.cpp         # state, initial conditions, BC setup
  equations.cpp            # flux/operator assembly and advance
  diagnostics.cpp          # solver-specific diagnostics
  README.md                # what to edit, validation case
  cases/traveling.yaml     # example case
```

The split is deliberate: a user changing the physics edits `equations.cpp` and
`createFields.cpp`, not the driver or the mesh.

## User workflow

```bash
# 1. copy a solver
cp -r src/solvers/burgers user/solvers/myBurgers

# or scaffold it
tools/newSolver.sh burgers myBurgers

# 2. edit the equations/options/state
#    user/solvers/myBurgers/equations.cpp
#    user/solvers/myBurgers/createFields.cpp
#    user/solvers/myBurgers/BurgersOptions.cpp

# 3. configure and build
cmake -S . -B build -DSATURN_USER_SOLVERS=user/solvers
cmake --build build -j

# 4. inspect and run
saturnCFD -listSolvers
saturnCFD -dumpConfig myBurgers -writeTemplate myBurgers.yaml
saturnCFD myBurgers.yaml
```

The user does not edit `src/simulation`, `src/mesh`, `src/fv`, `src/linalg` or
`src/io`.

## Alternatives considered

| alternative | why not chosen |
|---|---|
| Runtime `.so` plugins | C++ ABI, Kokkos/CUDA device code and symbol visibility are fragile; defer to CPU-only later |
| One monolithic solver file with `#ifdef` | not editable, not testable, not OpenFOAM-like |
| User forks the whole repository | duplicates the substrate and loses upstream fixes |
| Edit core files for each new equation | violates the separation and makes every change a merge conflict |
| Python user-defined equations | too slow for the target and adds an interpreter; not v1.0 |

## Consequences

- **Build:** adding a solver requires a CMake reconfigure. This is the same
  workflow as OpenFOAM's `wmake`, one step up.
- **API stability:** the solver interface becomes a public contract. Changes
  need an API version bump and a migration note.
- **Testing:** each built-in solver is also the reference for the user template;
  the scaffold test copies it, builds it and runs a small case.
- **GPU:** user solvers are compiled with the same Kokkos backend as the core;
  no runtime plugin boundary. This is a feature, not a limitation.
- **Risk:** a user can write an unstable scheme. The core cannot validate the
  physics; it can validate the interface, the YAML and the mesh.

## Validation / exit criteria

- `tools/newSolver.sh burgers myBurgers` creates a buildable copy.
- `saturnCFD -listSolvers` lists `myBurgers`.
- `-dumpConfig myBurgers` produces a template with the user's options.
- A copied solver with a changed coefficient runs and differs from the original
  in the expected way.
- `-DSATURN_USER_SOLVERS=...` builds without editing `src/`.
- A solver compiled against the wrong API version fails with a clear error.

## References

- OpenFOAM solver copy/`wmake` workflow (methodology reference).
- Nektar++ solver/module extension model (methodology reference).
