# Host requirements and environment check

**Status:** implemented. `requirements.toml` is the single list of host tools
and minimum versions; `tools/checkRequirements.py` checks it before a build.

## Why

A CMake configure can succeed while documentation tools, the language server or
the GPU toolkit are missing. The requirements file makes the host state explicit
before compiling, so a failure is not discovered halfway through a build or a
presentation.

## Check

From the repository root:

```bash
python3 tools/checkRequirements.py
```

The checker prints one line per requirement:

```text
[OK     ] cmake          3.28.3       >= 3.24       core    required
[OK     ] petsc          3.19.6       >= 3.19       core    required
[MISSING] clangd         unknown      >= 14.0       editor  optional
```

Exit code is zero when every **required** requirement is met. Missing optional
tools are reported but do not fail the check unless `--strict` is used.

## Install missing packages

```bash
python3 tools/checkRequirements.py --install
# core build tools only:
python3 tools/checkRequirements.py --install --core-only
```

`--install` runs `apt-get update` and installs the `apt` package names declared
in `requirements.toml`, using `sudo` when needed. CUDA has no apt entry; install
the CUDA toolkit manually for `buildGPU`.

After installation, re-run the checker.

## CMake target

The same check is available as a build target:

```bash
cmake --build buildCPU --target doctor
```

Kokkos, yaml-cpp and doctest are **fetched and compiled by CMake**; the checker
lists them separately. They need network access on a clean build.

## Requirements groups

| group | examples | required |
|---|---|---|
| core | cmake, C++20 compiler, make, git, Python, MPI, pkg-config, PETSc | yes |
| docs | Doxygen, Graphviz, latexmk, pdflatex, bibtex | optional, needed for docs |
| editor | clang-format, clangd | optional |
| gpu | nvcc / CUDA toolkit | optional, needed for `buildGPU` |

## What the checker cannot replace

`cmake --preset cpu` still performs the authoritative dependency detection:

- MPI compiler and library,
- PETSc package and version,
- Kokkos backend configure,
- yaml-cpp/doctest fetch,
- compiler features.

The checker is a pre-flight report, not a substitute for CMake.

## clangd

CMake generates:

```text
buildCPU/compile_commands.json
buildGPU/compile_commands.json
```

Link the current one at the repository root:

```bash
cmake --build buildCPU --target compileDb
```

Then clangd finds it automatically. Alternatively:

```bash
clangd --compile-commands-dir=buildCPU
```

VS Code clangd extension:

```json
"clangd.arguments": ["--compile-commands-dir=${workspaceFolder}/buildCPU"]
```

Use `buildGPU` when editing the CUDA build. There is no `compileCommands.json`.

The CLI has the same guidance:

```bash
saturnCFD -help clangd
```

## Doxygen timestamp

`docs/Doxyfile` sets `TIMESTAMP = YES`, so the generated HTML footer shows the
generation date. Rebuild the docs with:

```bash
cmake --build buildCPU --target docHtml
```

## Naming

Library targets are uniformly camelCase:

```text
saturnCore
saturnLinAlg
saturnSimulation
saturnBurger
```

The test executables are `testSaturn`, `testSaturnMPI`, `testSaturnFV` and
`testSaturnPETSc`. Build directories are `buildCPU` and `buildGPU`.

## Related

- `docs/guides/commands.md`
- `docs/guides/petsc.md`
- `docs/plan/solverSequence.md`
- `tools/checkRequirements.py`
- `requirements.toml`
