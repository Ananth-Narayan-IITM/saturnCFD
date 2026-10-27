#!/usr/bin/env bash
# saturnCFD -> Kokkos nvcc_wrapper shim.
#
# nvcc needs a host compiler it supports. Kokkos' nvcc_wrapper picks g++ by
# default, which is usually too new for the installed CUDA. This shim pins a
# compatible one (override with NVCC_WRAPPER_DEFAULT_COMPILER) and then hands
# over to the vendored wrapper, so the choice survives into the build step
# instead of living in a shell export that people forget.
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
wrapper="${here}/../kokkos/bin/nvcc_wrapper"

if [[ ! -x "${wrapper}" ]]; then
  echo "error: vendored nvcc_wrapper not found at ${wrapper}" >&2
  exit 1
fi

if [[ -z "${NVCC_WRAPPER_DEFAULT_COMPILER:-}" ]]; then
  for candidate in g++-13 g++-12 g++-11 g++; do
    if command -v "${candidate}" >/dev/null 2>&1; then
      export NVCC_WRAPPER_DEFAULT_COMPILER="${candidate}"
      break
    fi
  done
fi

exec "${wrapper}" "$@"
