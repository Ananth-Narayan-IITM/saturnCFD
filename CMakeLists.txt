cmake_minimum_required(VERSION 3.24)
project(saturnCFD VERSION 0.1.0 LANGUAGES CXX
        DESCRIPTION "Block-structured AMR incompressible CFD, performance portable via Kokkos")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---------------------------------------------------------------- options ----
option(SATURN_ENABLE_MPI   "Enable MPI domain decomposition"     OFF)
option(SATURN_BUILD_TESTS  "Build unit tests"                    ON)
option(SATURN_ENABLE_WARNINGS "Enable -Wall -Wextra"             ON)

set(SATURN_NDIM "2" CACHE STRING "Spatial dimensions (2 or 3)")
set_property(CACHE SATURN_NDIM PROPERTY STRINGS 2 3)

set(SATURN_PRECISION "double" CACHE STRING "Floating point precision")
set_property(CACHE SATURN_PRECISION PROPERTY STRINGS float double)

if(NOT SATURN_NDIM MATCHES "^[23]$")
  message(FATAL_ERROR "SATURN_NDIM must be 2 or 3, got '${SATURN_NDIM}'")
endif()
if(NOT SATURN_PRECISION MATCHES "^(float|double)$")
  message(FATAL_ERROR "SATURN_PRECISION must be float or double, got '${SATURN_PRECISION}'")
endif()

# ------------------------------------------------------------- dependencies --
find_package(Kokkos REQUIRED)

set(SATURN_SOURCES
  src/amr/BoxArray.cpp
  src/amr/DistributionMapping.cpp
  src/amr/MultiFab.cpp
  src/comm/HaloExchange.cpp
  src/core/Config.cpp
  src/core/Timer.cpp
  src/linalg/Poisson.cpp
  src/solver/CavitySolver.cpp
  src/io/VTKWriter.cpp
)

add_library(saturn_core STATIC ${SATURN_SOURCES})
add_library(saturn::core ALIAS saturn_core)

target_include_directories(saturn_core PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
)

target_compile_definitions(saturn_core PUBLIC SATURN_NDIM=${SATURN_NDIM})
if(SATURN_PRECISION STREQUAL "double")
  target_compile_definitions(saturn_core PUBLIC SATURN_REAL_DOUBLE)
endif()

target_link_libraries(saturn_core PUBLIC Kokkos::kokkos)

if(SATURN_ENABLE_MPI)
  find_package(MPI COMPONENTS CXX REQUIRED)
  target_link_libraries(saturn_core PUBLIC MPI::MPI_CXX)
  target_compile_definitions(saturn_core PUBLIC SATURN_USE_MPI)
  message(STATUS "saturnCFD: MPI enabled (${MPI_CXX_LIBRARY_VERSION_STRING})")
else()
  message(STATUS "saturnCFD: MPI disabled (serial / threaded only)")
endif()

if(SATURN_ENABLE_WARNINGS)
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(saturn_core PRIVATE -Wall -Wextra -Wpedantic)
  endif()
endif()

# ------------------------------------------------------------------ driver ---
add_executable(saturnCFD apps/main.cpp)
target_link_libraries(saturnCFD PRIVATE saturn::core)

add_executable(saturn_cavity apps/cavity.cpp)
target_link_libraries(saturn_cavity PRIVATE saturn::core)

# ------------------------------------------------------------------- tests ---
if(SATURN_BUILD_TESTS)
  enable_testing()
  add_executable(saturn_tests
    tests/main.cpp
    tests/test_box.cpp
    tests/test_multifab.cpp
    tests/test_halo.cpp
    tests/test_config.cpp
    tests/test_poisson.cpp
  )
  target_link_libraries(saturn_tests PRIVATE saturn::core)
  target_include_directories(saturn_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
  add_test(NAME unit COMMAND saturn_tests)
endif()

message(STATUS "saturnCFD: NDIM=${SATURN_NDIM} precision=${SATURN_PRECISION} "
               "MPI=${SATURN_ENABLE_MPI} backend=${Kokkos_DEVICES}")
