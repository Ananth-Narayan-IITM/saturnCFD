option(SATURN_BUILD_TESTS "Build the doctest test suite" ON)
option(SATURN_BUILD_DOCS "Enable Doxygen targets" ON)
option(SATURN_FETCH_DEPS "Fetch yaml-cpp/doctest/Kokkos when not installed" ON)
option(SATURN_ENABLE_MPI "Enable MPI decomposition" ON)
option(SATURN_ENABLE_KOKKOS "Enable Kokkos-backed fields and FVM operators" ON)
option(SATURN_ENABLE_PETSC "Enable PETSc (FVM/PETSc layer)" OFF)
set(SATURN_MPI_PREFLAGS "" CACHE STRING
    "Extra flags for mpiexec, e.g. --oversubscribe on Open MPI")
set(SATURN_BACKEND "openmp" CACHE STRING
    "Kokkos execution backend: serial, openmp, cuda or hip")
set_property(CACHE SATURN_BACKEND PROPERTY STRINGS serial openmp cuda hip)
option(SATURN_USE_SYSTEM_KOKKOS
       "Use an installed Kokkos instead of fetching a pinned source tree" OFF)
set(SATURN_KOKKOS_TAG "4.7.04" CACHE STRING
    "Kokkos git tag used by FetchContent")
set(SATURN_KOKKOS_CUDA_ARCH "" CACHE STRING
    "Kokkos CUDA architecture, e.g. PASCAL61; empty lets Kokkos detect")
set(SATURN_PRECISION "double" CACHE STRING
    "Field precision: double (float is planned, not wired yet)")
option(SATURN_ENABLE_WARNINGS "Enable -Wall -Wextra" ON)

if(NOT SATURN_BACKEND MATCHES "^(serial|openmp|cuda|hip)$")
  message(FATAL_ERROR
    "SATURN_BACKEND=${SATURN_BACKEND} is invalid; use serial, openmp, cuda or hip")
endif()

if(NOT SATURN_PRECISION STREQUAL "double")
  message(FATAL_ERROR
    "SATURN_PRECISION=${SATURN_PRECISION} is not implemented yet; use double")
endif()

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE "Release" CACHE STRING "" FORCE)
endif()

if(SATURN_ENABLE_WARNINGS AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()
