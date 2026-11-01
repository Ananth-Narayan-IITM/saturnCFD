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

## Install missing packages and Kokkos

```bash
python3 tools/checkRequirements.py --install
# core build tools only:
python3 tools/checkRequirements.py --install --core-only
```

`--install` does three things:

1. runs `apt-get update` and installs the `apt` packages declared in
   `requirements.toml`, using `sudo` when needed;
2. installs PETSc (`petsc-dev`) when it is missing;
3. installs `nvcc` through `nvidia-cuda-toolkit` **only when an NVIDIA GPU is
   detected**;
4. checks Kokkos and compiles it from source when
   `~/.local/kokkos-cpu` or `~/.local/kokkos-cuda` is missing.

Kokkos install locations:

```text
~/.local/kokkos-cpu    # OpenMP + Serial, CPU build
~/.local/kokkos-cuda   # CUDA + Serial, GPU build
```

The source is taken from `buildCPU/_deps/kokkos-src`,
`buildGPU/_deps/kokkos-src`, or cloned into
`~/.cache/saturnCFD/kokkos`. The CUDA build uses
`Kokkos_ARCH_NATIVE=ON` and the Kokkos `nvcc_wrapper`.

For a GPU build, point CMake at the CUDA Kokkos:

```bash
export KOKKOS_ROOT="$HOME/.local/kokkos-cuda"
```

`KOKKOS_ROOT` is needed only when configuring/building the GPU preset, not when
running `saturnGPU`.

After installation, re-run the checker.

## CMake target

The same check is available as a build target:

```bash
cmake --build buildCPU --target doctor
```

Kokkos, yaml-cpp and doctest can be **fetched and compiled by CMake**; the
checker also reports and can pre-build Kokkos. The CMake FetchContent path
still works on a clean build and needs network access.

## Requirements groups

| group | examples | required |
|---|---|---|
| core | cmake, C++20 compiler, make, git, Python, MPI, pkg-config, PETSc | yes |
| docs | Doxygen, Graphviz, latexmk, pdflatex, bibtex | optional, needed for docs |
| editor | clang-format, clangd | optional |
| gpu | nvcc / CUDA toolkit (`nvidia-cuda-toolkit`) | optional, installed only when an NVIDIA GPU is present |

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
