# Toolchain for a CUDA-enabled, installed Kokkos.
#
# The CUDA build must use Kokkos' nvcc_wrapper as the C++ compiler. It is
# installed next to the Kokkos prefix:
#
#   export KOKKOS_ROOT=/path/to/cuda-kokkos
#   cmake --preset gpu
#
# The wrapper path and the Kokkos package root are taken from KOKKOS_ROOT so
# that the preset stays hardware-independent.

if(NOT DEFINED ENV{KOKKOS_ROOT})
  message(FATAL_ERROR
    "cuda-kokkos toolchain: set KOKKOS_ROOT to a CUDA-enabled Kokkos install")
endif()

set(Kokkos_ROOT "$ENV{KOKKOS_ROOT}" CACHE PATH "CUDA-enabled Kokkos install")
set(CMAKE_CXX_COMPILER "$ENV{KOKKOS_ROOT}/bin/nvcc_wrapper"
    CACHE FILEPATH "Kokkos nvcc wrapper")

if(NOT EXISTS "$ENV{KOKKOS_ROOT}/bin/nvcc_wrapper")
  message(FATAL_ERROR
    "cuda-kokkos toolchain: ${KOKKOS_ROOT}/bin/nvcc_wrapper not found")
endif()
