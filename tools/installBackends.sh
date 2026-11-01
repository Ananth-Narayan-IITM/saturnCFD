#!/usr/bin/env bash
#
# Build and install saturnCFD backends into one stable install root:
#
#   ~/.local/saturnCFD/cpu/bin/saturnCFD
#   ~/.local/saturnCFD/gpu/bin/saturnCFD
#
# and create the path-independent commands:
#
#   ~/.local/bin/saturnCFD-cpu
#   ~/.local/bin/saturnCFD-gpu
#
# Usage:
#   tools/installBackends.sh [cpu|gpu|both] [--default cpu|gpu]
#
# Environment overrides:
#   SATURN_PREFIX       install root (default: $HOME/.local/saturnCFD)
#   SATURN_BIN          command directory (default: $HOME/.local/bin)
#   KOKKOS_CUDA_ROOT    CUDA-enabled Kokkos prefix
#                       (default: $HOME/.local/kokkos-cuda)
#   SATURN_DEFAULT      default backend: cpu or gpu
#
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PREFIX="${SATURN_PREFIX:-$HOME/.local/saturnCFD}"
BIN_DIR="${SATURN_BIN:-$HOME/.local/bin}"
KOKKOS_CUDA_ROOT="${KOKKOS_CUDA_ROOT:-$HOME/.local/kokkos-cuda}"
SATURN_DEFAULT="${SATURN_DEFAULT:-}"

TARGET="${1:-both}"
if [[ "$TARGET" == "--default" ]]; then
  SATURN_DEFAULT="${2:-}"
  TARGET="both"
elif [[ "$TARGET" == --default=* ]]; then
  SATURN_DEFAULT="${TARGET#--default=}"
  TARGET="both"
elif [[ "${2:-}" == "--default" ]]; then
  SATURN_DEFAULT="${3:-}"
fi

usage() {
  cat <<'EOF'
Usage: tools/installBackends.sh [cpu|gpu|both] [--default cpu|gpu]

Installs into:
  $HOME/.local/saturnCFD/cpu
  $HOME/.local/saturnCFD/gpu

and links:
  $HOME/.local/bin/saturnCFD-cpu
  $HOME/.local/bin/saturnCFD-gpu

Environment:
  SATURN_PREFIX, SATURN_BIN, KOKKOS_CUDA_ROOT, SATURN_DEFAULT
EOF
}

if [[ "$TARGET" != "cpu" && "$TARGET" != "gpu" && "$TARGET" != "both" ]]; then
  usage
  exit 1
fi

mkdir -p "$BIN_DIR" "$PREFIX"
cd "$ROOT"

build_cpu() {
  echo "== CPU build =="
  cmake --preset cpu
  cmake --build --preset cpu -j
  cmake --install buildCPU --prefix "$PREFIX/cpu"
  ln -sfn "$PREFIX/cpu/bin/saturnCFD" "$BIN_DIR/saturnCFD-cpu"
  echo "installed: $PREFIX/cpu/bin/saturnCFD"
}

build_gpu() {
  echo "== GPU build =="
  if [[ ! -x "$KOKKOS_CUDA_ROOT/bin/nvcc_wrapper" ]]; then
    echo "error: CUDA-enabled Kokkos not found at $KOKKOS_CUDA_ROOT" >&2
    echo "       set KOKKOS_CUDA_ROOT to the Kokkos install prefix" >&2
    exit 1
  fi
  export KOKKOS_ROOT="$KOKKOS_CUDA_ROOT"
  cmake --preset gpu
  cmake --build --preset gpu -j
  cmake --install buildGPU --prefix "$PREFIX/gpu"
  ln -sfn "$PREFIX/gpu/bin/saturnCFD" "$BIN_DIR/saturnCFD-gpu"
  echo "installed: $PREFIX/gpu/bin/saturnCFD"
}

case "$TARGET" in
  cpu) build_cpu ;;
  gpu) build_gpu ;;
  both) build_cpu; build_gpu ;;
esac

if [[ -n "$SATURN_DEFAULT" ]]; then
  if [[ "$SATURN_DEFAULT" != "cpu" && "$SATURN_DEFAULT" != "gpu" ]]; then
    echo "error: --default must be cpu or gpu" >&2
    exit 1
  fi
  ln -sfn "$PREFIX/$SATURN_DEFAULT/bin/saturnCFD" \
          "$BIN_DIR/saturnCFD"
  echo "default : $BIN_DIR/saturnCFD -> $SATURN_DEFAULT"
fi

echo
echo "install root: $PREFIX"
echo "commands    : $BIN_DIR/saturnCFD-cpu"
echo "              $BIN_DIR/saturnCFD-gpu"

case ":$PATH:" in
  *":$BIN_DIR:"*)
    ;;
  *)
    echo
    echo "warning: $BIN_DIR is not on PATH. Add it once:"
    echo "  echo 'export PATH=\"$BIN_DIR:\$PATH\"' >> \"\$HOME/.bashrc\""
    ;;
esac
