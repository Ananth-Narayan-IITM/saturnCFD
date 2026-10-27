cmake_minimum_required(VERSION 3.24)

# ===========================================================================
# Backend selection
# ===========================================================================
# ONE switch decides the whole build route: which Kokkos backend to enable,
# which compiler to use, and therefore which precision makes sense.
#
#   cmake -S . -B build-gpu -DSATURN_BACKEND=cuda
#   cmake -S . -B build-cpu -DSATURN_BACKEND=openmp
#
# The compiler has to be chosen BEFORE project(), because CMake fixes the
# compiler at that point. That is why this block sits above it.

set(SATURN_BACKEND "openmp" CACHE STRING
    "Target backend: serial | openmp | cuda | hip | sycl")
set_property(CACHE SATURN_BACKEND PROPERTY STRINGS serial openmp cuda hip sycl)

if(SATURN_BACKEND STREQUAL "cuda")
  set(_saturnWrapper "${CMAKE_CURRENT_SOURCE_DIR}/external/scripts/nvccWrapper.sh")
  if(NOT EXISTS "${_saturnWrapper}")
    message(FATAL_ERROR
      "SATURN_BACKEND=cuda needs ${_saturnWrapper}, which is missing.")
  endif()
  set(CMAKE_CXX_COMPILER "${_saturnWrapper}" CACHE FILEPATH
      "C++ compiler (Kokkos nvcc_wrapper shim)" FORCE)
endif()

project(saturnCFD VERSION 0.3.0 LANGUAGES CXX
        DESCRIPTION "Finite-volume incompressible CFD with block-structured AMR")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ===========================================================================
# Precision
# ===========================================================================
# Default "auto": float on CUDA, double elsewhere. Consumer GPUs run FP64 at
# 1/64 of FP32, so a double build on a laptop GPU is slower than the CPU.
set(SATURN_PRECISION "auto" CACHE STRING "Floating point precision: auto | float | double")
set_property(CACHE SATURN_PRECISION PROPERTY STRINGS auto float double)

if(SATURN_PRECISION STREQUAL "auto")
  if(SATURN_BACKEND STREQUAL "cuda")
    set(_saturnPrec "float")
  else()
    set(_saturnPrec "double")
  endif()
else()
  set(_saturnPrec "${SATURN_PRECISION}")
endif()

# ===========================================================================
# Dimension and options
# ===========================================================================
set(SATURN_NDIM "2" CACHE STRING "Spatial dimensions (2 or 3)")
set_property(CACHE SATURN_NDIM PROPERTY STRINGS 2 3)

option(SATURN_ENABLE_MPI "Enable MPI domain decomposition" OFF)
option(SATURN_BUILD_TESTS "Build unit tests" ON)
option(SATURN_ENABLE_WARNINGS "Enable -Wall -Wextra" ON)

# ===========================================================================
# Kokkos
# ===========================================================================
# By default the vendored Kokkos in external/kokkos is built as part of this
# project, so the Kokkos build tree lives under saturnCFD/build-*/ and nothing
# outside the repository is required. Point SATURN_KOKKOS_ROOT at a prebuilt
# install to reuse one instead (recommended on a cluster).
set(SATURN_KOKKOS_ROOT "" CACHE PATH
    "Prebuilt Kokkos install to use instead of the vendored source")
set(SATURN_CUDA_ARCH "" CACHE STRING
    "Kokkos CUDA architecture (e.g. AMPERE86). Empty = auto-detect from nvidia-smi")

if(SATURN_KOKKOS_ROOT)
  find_package(Kokkos REQUIRED PATHS "${SATURN_KOKKOS_ROOT}" NO_DEFAULT_PATH)
  set(_saturnKokkosOrigin "prebuilt at ${SATURN_KOKKOS_ROOT}")
else()
  set(_saturnKokkosSource "${CMAKE_CURRENT_SOURCE_DIR}/external/kokkos")
  if(NOT EXISTS "${_saturnKokkosSource}/CMakeLists.txt")
    message(FATAL_ERROR
      "Vendored Kokkos not found at ${_saturnKokkosSource}. Either restore it "
      "or configure with -DSATURN_KOKKOS_ROOT=/path/to/kokkos-install")
  endif()

  set(Kokkos_ENABLE_SERIAL     ON  CACHE BOOL "" FORCE)
  set(Kokkos_ENABLE_TESTS      OFF CACHE BOOL "" FORCE)
  set(Kokkos_ENABLE_EXAMPLES   OFF CACHE BOOL "" FORCE)
  set(Kokkos_ENABLE_BENCHMARKS OFF CACHE BOOL "" FORCE)
  set(Kokkos_ENABLE_OPENMP     OFF CACHE BOOL "" FORCE)
  set(Kokkos_ENABLE_CUDA       OFF CACHE BOOL "" FORCE)
  set(Kokkos_ENABLE_HIP        OFF CACHE BOOL "" FORCE)
  set(Kokkos_ENABLE_SYCL       OFF CACHE BOOL "" FORCE)

  if(SATURN_BACKEND STREQUAL "serial")
    # Serial only: nothing else to enable.
  elseif(SATURN_BACKEND STREQUAL "openmp")
    set(Kokkos_ENABLE_OPENMP ON CACHE BOOL "" FORCE)
  elseif(SATURN_BACKEND STREQUAL "cuda")
    set(Kokkos_ENABLE_CUDA ON CACHE BOOL "" FORCE)

    if(NOT SATURN_CUDA_ARCH)
      # Ask the driver which GPU is present and map compute capability to the
      # architecture name Kokkos expects.
      find_program(SATURN_NVIDIA_SMI nvidia-smi)
      if(SATURN_NVIDIA_SMI)
        execute_process(
          COMMAND "${SATURN_NVIDIA_SMI}" --query-gpu=compute_cap
                  --format=csv,noheader
          OUTPUT_VARIABLE _cap
          OUTPUT_STRIP_TRAILING_WHITESPACE
          ERROR_QUIET
          RESULT_VARIABLE _capResult)
        if(_capResult EQUAL 0)
          string(STRIP "${_cap}" _cap)
          string(REPLACE "." "" _cap "${_cap}")
          if(_cap STREQUAL "70")
            set(SATURN_CUDA_ARCH "VOLTA70")
          elseif(_cap STREQUAL "72")
            set(SATURN_CUDA_ARCH "VOLTA72")
          elseif(_cap STREQUAL "75")
            set(SATURN_CUDA_ARCH "TURING75")
          elseif(_cap STREQUAL "80")
            set(SATURN_CUDA_ARCH "AMPERE80")
          elseif(_cap STREQUAL "86")
            set(SATURN_CUDA_ARCH "AMPERE86")
          elseif(_cap STREQUAL "87")
            set(SATURN_CUDA_ARCH "AMPERE87")
          elseif(_cap STREQUAL "89")
            set(SATURN_CUDA_ARCH "ADA89")
          elseif(_cap STREQUAL "90")
            set(SATURN_CUDA_ARCH "HOPPER90")
          endif()
          message(STATUS "saturnCFD: detected GPU compute capability ${_cap}"
                         " -> ${SATURN_CUDA_ARCH}")
        endif()
      endif()
    endif()

    if(NOT SATURN_CUDA_ARCH)
      message(FATAL_ERROR
        "Could not determine the CUDA architecture. Pass "
        "-DSATURN_CUDA_ARCH=AMPERE86 (or your GPU's arch).")
    endif()
    set(Kokkos_ARCH_${SATURN_CUDA_ARCH} ON CACHE BOOL "" FORCE)

  elseif(SATURN_BACKEND STREQUAL "hip")
    set(Kokkos_ENABLE_HIP ON CACHE BOOL "" FORCE)
    if(SATURN_CUDA_ARCH)
      set(Kokkos_ARCH_${SATURN_CUDA_ARCH} ON CACHE BOOL "" FORCE)
    endif()
  elseif(SATURN_BACKEND STREQUAL "sycl")
    set(Kokkos_ENABLE_SYCL ON CACHE BOOL "" FORCE)
  else()
    message(FATAL_ERROR
      "Unknown SATURN_BACKEND '${SATURN_BACKEND}'. "
      "Expected serial, openmp, cuda, hip or sycl.")
  endif()

  add_subdirectory("${_saturnKokkosSource}" EXCLUDE_FROM_ALL)
  set(_saturnKokkosOrigin "vendored (built into this build tree)")
endif()

# ===========================================================================
# Library
# ===========================================================================
set(SATURN_SOURCES
  src/amr/BoxArray.cpp
  src/amr/DistributionMapping.cpp
  src/amr/MultiFab.cpp
  src/comm/HaloExchange.cpp
  src/core/Config.cpp
  src/core/Timer.cpp
  src/io/VTKWriter.cpp
  src/linalg/Poisson.cpp
  src/solver/CavitySolver.cpp
)

add_library(saturn_core STATIC ${SATURN_SOURCES})
add_library(saturn::core ALIAS saturn_core)

target_include_directories(saturn_core PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
)

target_compile_definitions(saturn_core PUBLIC SATURN_NDIM=${SATURN_NDIM})
if(_saturnPrec STREQUAL "double")
  target_compile_definitions(saturn_core PUBLIC SATURN_REAL_DOUBLE)
endif()

target_link_libraries(saturn_core PUBLIC Kokkos::kokkos)

if(SATURN_ENABLE_MPI)
  find_package(MPI COMPONENTS CXX REQUIRED)
  target_link_libraries(saturn_core PUBLIC MPI::MPI_CXX)
  target_compile_definitions(saturn_core PUBLIC SATURN_USE_MPI)
endif()

if(SATURN_ENABLE_WARNINGS AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(saturn_core PRIVATE -Wall -Wextra)
endif()

# ===========================================================================
# Driver and tests
# ===========================================================================
add_executable(saturnCFD apps/main.cpp)
target_link_libraries(saturnCFD PRIVATE saturn::core)

if(SATURN_BUILD_TESTS)
  enable_testing()
  add_executable(saturn_tests
    tests/TestMain.cpp
    tests/TestBox.cpp
    tests/TestMultiFab.cpp
    tests/TestHalo.cpp
    tests/TestConfig.cpp
    tests/TestPoisson.cpp
  )
  target_link_libraries(saturn_tests PRIVATE saturn::core)
  target_include_directories(saturn_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
  add_test(NAME unit COMMAND saturn_tests)
endif()

# ===========================================================================
# Docs
# ===========================================================================
find_package(Doxygen QUIET)
if(DOXYGEN_FOUND)
  add_custom_target(doc
    COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/docs/Doxyfile
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Generating API documentation into build/doxygen/html")
endif()

# ===========================================================================
message(STATUS "")
message(STATUS "saturnCFD ${PROJECT_VERSION}")
message(STATUS "  backend    : ${SATURN_BACKEND}")
message(STATUS "  kokkos     : ${_saturnKokkosOrigin}")
message(STATUS "  precision  : ${_saturnPrec}")
message(STATUS "  dimension  : ${SATURN_NDIM}D")
message(STATUS "  mpi        : ${SATURN_ENABLE_MPI}")
message(STATUS "  sources    : ${CMAKE_CURRENT_SOURCE_DIR}")
message(STATUS "  build      : ${CMAKE_BINARY_DIR}")
message(STATUS "")
