option(SATURN_BUILD_TESTS "Build the doctest test suite" ON)
option(SATURN_BUILD_DOCS "Enable Doxygen targets" ON)
option(SATURN_FETCH_DEPS "Fetch yaml-cpp/doctest when not installed" ON)
option(SATURN_ENABLE_KOKKOS "Enable Kokkos (Phase 2+)" OFF)
option(SATURN_ENABLE_PETSC "Enable PETSc (Phase 2+)" OFF)
option(SATURN_ENABLE_WARNINGS "Enable -Wall -Wextra" ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE "Release" CACHE STRING "" FORCE)
endif()

if(SATURN_ENABLE_WARNINGS AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()
