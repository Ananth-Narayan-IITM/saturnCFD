cmake_minimum_required(VERSION 3.24)

project(saturnCFD VERSION 1.0.0 LANGUAGES CXX
        DESCRIPTION "Structured finite-volume CFD with YAML, PETSc and Kokkos")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(SATURN_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}" CACHE INTERNAL "")
set(SATURN_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}" CACHE INTERNAL "")

include(cmake/SaturnOptions.cmake)
include(cmake/SaturnDependencies.cmake)
include(cmake/SaturnSolvers.cmake)

add_subdirectory(src)
add_subdirectory(apps)

if(SATURN_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

include(cmake/SaturnDocs.cmake)

# ---------------------------------------------------------- host tooling --
find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
  add_custom_target(doctor
    COMMAND ${Python3_EXECUTABLE}
            ${SATURN_SOURCE_DIR}/tools/checkRequirements.py
            --toml ${SATURN_SOURCE_DIR}/requirements.toml
    WORKING_DIRECTORY ${SATURN_SOURCE_DIR}
    COMMENT "Checking host requirements from requirements.toml"
    USES_TERMINAL)
endif()

if(CMAKE_EXPORT_COMPILE_COMMANDS)
  add_custom_target(compileDb
    COMMAND ${CMAKE_COMMAND} -E rm -f
            ${SATURN_SOURCE_DIR}/compile_commands.json
    COMMAND ${CMAKE_COMMAND} -E create_symlink
            ${CMAKE_BINARY_DIR}/compile_commands.json
            ${SATURN_SOURCE_DIR}/compile_commands.json
    COMMENT "Linking compile_commands.json at the repository root for clangd"
    VERBATIM)
endif()

message(STATUS "")
message(STATUS "saturnCFD ${PROJECT_VERSION}")
message(STATUS "  phase      : 1 (mesh + config + comm + FVM + simulation + PETSc)")
message(STATUS "  build      : ${CMAKE_BUILD_TYPE}")
message(STATUS "  tests      : ${SATURN_BUILD_TESTS}")
message(STATUS "  docs       : ${SATURN_BUILD_DOCS}")
message(STATUS "  yaml-cpp   : ${SATURN_YAML_ORIGIN}")
message(STATUS "  doctest    : ${SATURN_DOCTEST_ORIGIN}")
message(STATUS "  MPI        : ${SATURN_ENABLE_MPI} (${SATURN_MPI_ORIGIN})")
message(STATUS "  PETSc      : ${SATURN_ENABLE_PETSC} (${SATURN_PETSC_ORIGIN})")
message(STATUS "  Kokkos     : ${SATURN_ENABLE_KOKKOS} (${SATURN_KOKKOS_ORIGIN})")
if(SATURN_ENABLE_KOKKOS)
  message(STATUS "  backend    : ${SATURN_BACKEND}")
endif()
message(STATUS "")
