# 0011 — User-editable solvers, OpenFOAM-style

- **Status:** accepted
- **Date:** 2026-10-31
- **Owner:** saturnCFD v1
- **Supersedes:** none
- **Protocol:** `docs/protocols/solver.md`, `docs/guides/writingASolver.md`
- **Tests:** `tests/solvers/...`, `tests/buildCPU/...`

## Context

The solver must be editable by the user the way OpenFOAM solvers are: copy a
solver directory, change the equations or the state, rebuild, and run it with a
new case file. The core mesh, finite-volume, PETSc and I/O code must not be
edited for a new equation set.

The user explicitly prefers the copy-and-add-one-CMake-line workflow over an
external plugin directory. The scaffold tool must be C++, not a shell script,
so it behaves the same on every machine.

## Decision

1. **A solver is a self-contained directory** under
   `src/solvers/saturn<Name>/`. Built-in solvers and user copies use exactly the
   same layout and API.
2. **User workflow:** copy a built-in solver, add one line to
   `src/solvers/CMakeLists.txt`, rebuild.
   ```bash
   cp -r src/solvers/saturnBurger src/solvers/saturnMyBurger
   # rename class/target and add: add_subdirectory(saturnMyBurger)
   ```
   or let the C++ scaffold do the copy and print the line.
3. **No `user/solvers/` discovery path in v1.0.** One solver tree, one CMake
   list. This is the simplest mental model and matches the user's preference.
4. **Compile-time linking.** User solvers are static libraries linked into
   `saturnCFD`. There is no runtime `.so` plugin in v1.0; Kokkos/CUDA device
   code and C++ ABI stay predictable.
5. **Registration is one line** in the solver source:
   ```cpp
   SATURN_REGISTER_SOLVER("myBurgers",
                          [] { return std::make_unique<MyBurgersSolver>(); })
   ```
6. **The solver registers its own YAML options.** `-dumpConfig myBurgers`
   generates the template from the user's code.
7. **The solver API has a version constant.** A user solver compiled against an
   incompatible core fails at compile time with a clear message.
8. **The scaffold is C++:** `tools/saturnCFDSolver.cpp`, built as the `saturnCFDSolver`
   executable. It copies the directory, renames the class/registration strings,
   and prints the exact CMake line to add. No Bash script.
9. **OpenFOAM analogy:** copy `src/solvers/saturnBurger` to
   `src/solvers/saturnMyBurger`, edit the equations/fields, add one CMake line,
   rebuild, then `saturnCFD -listSolvers` shows the new solver.

## Solver directory layout

```text
src/solvers/saturnBurger/
  CMakeLists.txt           # target saturnBurger
  SaturnBurger.hpp         # options + class
  SaturnBurger.cpp         # geometry, BCs, RHS
  README.md                # equations, algorithm, validation
```

The current 1D solver keeps its options in the header. A larger solver may add
`createFields.cpp`, `equations.cpp` and `diagnostics.cpp`; the split is
deliberate: physics edits stay out of the driver and mesh.

The split is deliberate: a user changing the physics edits `equations.cpp` and
`createFields.cpp`, not the driver or the mesh.

## User workflow

```bash
# 1. copy and rename (C++ tool; no shell script)
saturnCFDSolver burgers myBurgers

# 2. add the printed line to src/solvers/CMakeLists.txt:
#    addSolver(myBurgers)

# 3. edit the equations/options/state
#    src/solvers/myBurgers/equations.cpp
#    src/solvers/myBurgers/createFields.cpp
#    src/solvers/myBurgers/MyBurgersOptions.cpp

# 4. build
cmake -S . -B buildCPU
cmake --build buildCPU -j

# 5. inspect and run
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
| `user/solvers/` auto-discovery | extra path and CMake magic; the user prefers copy + one line |
| One monolithic solver file with `#ifdef` | not editable, not testable, not OpenFOAM-like |
| User forks the whole repository | duplicates the substrate and loses upstream fixes |
| Bash scaffold script | not portable; the user wants C++ control |
| Python user-defined equations | too slow for the target and adds an interpreter; not v1.0 |

## Consequences

- **Build:** adding a solver requires a CMake reconfigure and one new line. Same
  spirit as OpenFOAM's `wmake`, with CMake instead of a custom build system.
- **API stability:** the solver interface becomes a public contract. Changes
  need an API version bump and a migration note.
- **Testing:** the scaffold test copies `burgers`, builds `myBurgers`, lists it
  and generates its template.
- **GPU:** user solvers are compiled with the same Kokkos backend as the core;
  no runtime plugin boundary. This is a feature, not a limitation.
- **Risk:** a user can write an unstable scheme. The core cannot validate the
  physics; it can validate the interface, the YAML and the mesh.

## Validation / exit criteria

- `saturnCFDSolver burgers myBurgers` creates a buildable copy under
  `src/solvers/myBurgers` and prints the CMake line.
- After adding the line, `saturnCFD -listSolvers` lists `myBurgers`.
- `-dumpConfig myBurgers` produces a template with the user's options.
- A copied solver with a changed coefficient runs and differs from the original
  in the expected way.
- A solver compiled against the wrong API version fails with a clear error.
- The project builds on a machine without Bash-specific tooling.

## References

- OpenFOAM solver copy/`wmake` workflow (methodology reference).
- Nektar++ solver/module extension model (methodology reference).
