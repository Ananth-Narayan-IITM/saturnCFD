# External dependencies

## kokkos/

A vendored copy of **Kokkos 5.2.99**, taken from the development checkout at
`~/PhD/Kokkos/kokkos` (git checkout, `.git` removed). It is built as part of
this project, so the Kokkos build tree lives under `saturnCFD/build-*/` and the
repository needs nothing outside itself.

### Why vendored

So that one CMake switch works with no external paths:

```bash
cmake -S . -B build-gpu -DSATURN_BACKEND=cuda
```

Building Kokkos in-tree costs a few minutes on the first configure and nothing
afterwards, and it guarantees the Kokkos and saturnCFD compilers, C++ standard
and backend agree.

### Using a prebuilt Kokkos instead

On a cluster you usually want to reuse a tuned Kokkos install:

```bash
cmake -S . -B build -DSATURN_BACKEND=cuda \
      -DSATURN_KOKKOS_ROOT=/path/to/kokkos-install
```

That skips `external/kokkos` entirely.

### Updating

```bash
rsync -a --exclude='.git' /path/to/new/kokkos/ external/kokkos/
```

Keep `example/` — Kokkos' top-level `CMakeLists.txt` unconditionally adds it.

### Licence

Kokkos is Apache-2.0 with LLVM Exceptions; see `kokkos/LICENSE`. That is
compatible with saturnCFD's Apache-2.0.

## scripts/

`nvccWrapper.sh` — a shim around Kokkos' `nvcc_wrapper`. nvcc needs a host
compiler it supports, and Kokkos' wrapper defaults to whichever `g++` is first
on the PATH, which is often too new for the installed CUDA. The shim picks the
newest supported one (`g++-13`, `g++-12`, `g++-11`, `g++`) and sets
`NVCC_WRAPPER_DEFAULT_COMPILER` before handing over, so the choice survives into
the build step rather than living in a shell export people forget.
