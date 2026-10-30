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

# -------------------------------------------------------- Kokkos and PETSc --
# The options exist so the FVM/PETSc layers can be enabled without changing the
# top-level build.
if(SATURN_ENABLE_KOKKOS)
  find_package(Kokkos REQUIRED)
endif()
if(SATURN_ENABLE_PETSC)
  find_package(PETSc REQUIRED)
endif()
