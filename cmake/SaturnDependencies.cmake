include(FetchContent)

# ---------------------------------------------------------------- yaml-cpp --
find_package(yaml-cpp 0.8 QUIET)
if(TARGET yaml-cpp::yaml-cpp)
  set(SATURN_YAML_TARGET yaml-cpp::yaml-cpp)
  set(SATURN_YAML_ORIGIN "system")
elseif(TARGET yaml-cpp)
  set(SATURN_YAML_TARGET yaml-cpp)
  set(SATURN_YAML_ORIGIN "system")
elseif(SATURN_FETCH_DEPS)
  set(YAML_CPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  set(YAML_CPP_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
  set(YAML_CPP_BUILD_CONTRIB OFF CACHE BOOL "" FORCE)
  set(YAML_CPP_INSTALL OFF CACHE BOOL "" FORCE)
  set(YAML_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(
    yaml-cpp
    GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
    GIT_TAG 0.8.0
    GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(yaml-cpp)
  if(TARGET yaml-cpp::yaml-cpp)
    set(SATURN_YAML_TARGET yaml-cpp::yaml-cpp)
  else()
    set(SATURN_YAML_TARGET yaml-cpp)
  endif()
  set(SATURN_YAML_ORIGIN "fetched 0.8.0")
else()
  message(FATAL_ERROR
    "yaml-cpp 0.8 not found. Install it or configure with "
    "-DSATURN_FETCH_DEPS=ON.")
endif()

# ----------------------------------------------------------------- doctest --
if(SATURN_BUILD_TESTS)
  find_package(doctest QUIET)
  if(TARGET doctest::doctest)
    set(SATURN_DOCTEST_ORIGIN "system")
  elseif(SATURN_FETCH_DEPS)
    set(DOCTEST_NO_INSTALL ON CACHE BOOL "" FORCE)
    FetchContent_Declare(
      doctest
      GIT_REPOSITORY https://github.com/doctest/doctest.git
      GIT_TAG v2.4.11
      GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(doctest)
    set(SATURN_DOCTEST_ORIGIN "fetched v2.4.11")
  else()
    message(FATAL_ERROR
      "doctest not found. Install it or configure with -DSATURN_FETCH_DEPS=ON.")
  endif()
else()
  set(SATURN_DOCTEST_ORIGIN "disabled")
endif()

# --------------------------------------------------------------------- MPI --
if(SATURN_ENABLE_MPI)
  find_package(MPI COMPONENTS CXX REQUIRED)
  # Do not force --oversubscribe. Most workstations have enough slots; add it
  # explicitly on a machine with fewer slots than requested ranks:
  #   -DSATURN_MPI_PREFLAGS=--oversubscribe
  set(SATURN_MPI_ORIGIN "system")
else()
  set(SATURN_MPI_ORIGIN "disabled")
endif()

# ------------------------------------------------------------------ Kokkos --
if(SATURN_ENABLE_KOKKOS)
  if(SATURN_USE_SYSTEM_KOKKOS)
    find_package(Kokkos 4.0 REQUIRED)
    set(SATURN_KOKKOS_ORIGIN "system")
  elseif(SATURN_FETCH_DEPS)
    # The backend is fixed at configure time. Disable every other Kokkos
    # backend explicitly so a cached option cannot change the build silently.
    set(Kokkos_ENABLE_SERIAL OFF CACHE BOOL "" FORCE)
    set(Kokkos_ENABLE_OPENMP OFF CACHE BOOL "" FORCE)
    set(Kokkos_ENABLE_CUDA OFF CACHE BOOL "" FORCE)
    set(Kokkos_ENABLE_HIP OFF CACHE BOOL "" FORCE)
    if(SATURN_BACKEND STREQUAL "serial")
      set(Kokkos_ENABLE_SERIAL ON CACHE BOOL "" FORCE)
    elseif(SATURN_BACKEND STREQUAL "openmp")
      set(Kokkos_ENABLE_OPENMP ON CACHE BOOL "" FORCE)
    elseif(SATURN_BACKEND STREQUAL "cuda")
      set(Kokkos_ENABLE_CUDA ON CACHE BOOL "" FORCE)
      set(Kokkos_ENABLE_CUDA_LAMBDA ON CACHE BOOL "" FORCE)
      if(SATURN_KOKKOS_CUDA_ARCH)
        set(Kokkos_ARCH_${SATURN_KOKKOS_CUDA_ARCH} ON CACHE BOOL "" FORCE)
      endif()
    elseif(SATURN_BACKEND STREQUAL "hip")
      set(Kokkos_ENABLE_HIP ON CACHE BOOL "" FORCE)
    endif()

    # Do not build Kokkos tests/examples/benchmarks inside saturnCFD.
    set(Kokkos_ENABLE_TESTS OFF CACHE BOOL "" FORCE)
    set(Kokkos_ENABLE_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(Kokkos_ENABLE_BENCHMARKS OFF CACHE BOOL "" FORCE)
    set(Kokkos_ENABLE_COMPILER_WARNINGS OFF CACHE BOOL "" FORCE)

    FetchContent_Declare(
      kokkos
      GIT_REPOSITORY https://github.com/kokkos/kokkos.git
      GIT_TAG ${SATURN_KOKKOS_TAG}
      GIT_SHALLOW TRUE
      GIT_PROGRESS TRUE)
    FetchContent_MakeAvailable(kokkos)
    set(SATURN_KOKKOS_ORIGIN "fetched ${SATURN_KOKKOS_TAG}")
  else()
    message(FATAL_ERROR
      "Kokkos not found. Configure with -DSATURN_FETCH_DEPS=ON or "
      "-DSATURN_USE_SYSTEM_KOKKOS=ON -DKokkos_ROOT=<prefix>.")
  endif()
else()
  set(SATURN_KOKKOS_ORIGIN "disabled")
endif()

if(SATURN_ENABLE_PETSC)
  find_package(PETSc REQUIRED)
endif()
